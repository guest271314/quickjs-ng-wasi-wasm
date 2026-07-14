/*
 * QuickJS stand alone interpreter
 *
 * Copyright (c) 2017-2021 Fabrice Bellard
 * Copyright (c) 2017-2021 Charlie Gordon
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish,     1: unknown import: `env::__extern_get` has not been defined
distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "quickjs-libc.h"
#include "quickjs.h"

const char* str = (const char[]) {
  #embed "../index.js"
  , 0 // Null terminator
};

#ifdef QJS_USE_MIMALLOC
#include <mimalloc.h>
#endif

static int qjs__argc;
static char** qjs__argv;
static int eval_buf(JSContext* ctx,
                    const void* buf,
                    int buf_len,
                    const char* filename,
                    int eval_flags) {
  bool use_realpath;
  JSValue val;
  int ret;

  if ((eval_flags & JS_EVAL_TYPE_MASK) == JS_EVAL_TYPE_MODULE) {
    /* for the modules, we compile then run to be able to set
       import.meta */
    val = JS_Eval(ctx, buf, buf_len, filename,
                  eval_flags | JS_EVAL_FLAG_COMPILE_ONLY);
    if (!JS_IsException(val)) {
      // ex. "<cmdline>" pr "/dev/stdin"
      use_realpath = !(*filename == '<' || !strncmp(filename, "/dev/", 5));
      if (js_module_set_import_meta(ctx, val, use_realpath, true) < 0) {
        js_std_dump_error(ctx);
        ret = -1;
        goto end;
      }
      val = JS_EvalFunction(ctx, val);
    }
    val = js_std_await(ctx, val);
  } else {
    val = JS_Eval(ctx, buf, buf_len, filename, eval_flags);
  }
  if (JS_IsException(val)) {
    js_std_dump_error(ctx);
    ret = -1;
  } else {
    ret = 0;
  }
end:
  JS_FreeValue(ctx, val);
  return ret;
}

/* also used to initialize the worker context */
static JSContext* JS_NewCustomContext(JSRuntime* rt) {
  JSContext* ctx;
  ctx = JS_NewContext(rt);
  if (!ctx)
    return NULL;
  /* system modules */
  js_init_module_std(ctx, "qjs:std");
  // js_init_module_std(ctx, "qjs:os");
  JSValue args = JS_NewArray(ctx);
  int i;
  for (i = 0; i < qjs__argc; i++) {
    JS_SetPropertyUint32(ctx, args, i, JS_NewString(ctx, qjs__argv[i]));
  }

  return ctx;
}

struct trace_malloc_data {
  uint8_t* base;
};

#ifdef QJS_USE_MIMALLOC
static void* js_mi_calloc(void* opaque, size_t count, size_t size) {
  return mi_calloc(count, size);
}

static void* js_mi_malloc(void* opaque, size_t size) {
  return mi_malloc(size);
}

static void js_mi_free(void* opaque, void* ptr) {
  if (!ptr)
    return;
  mi_free(ptr);
}

static void* js_mi_realloc(void* opaque, void* ptr, size_t size) {
  return mi_realloc(ptr, size);
}

static const JSMallocFunctions mi_mf = {js_mi_calloc, js_mi_malloc, js_mi_free,
                                        js_mi_realloc, mi_malloc_usable_size};
#endif

#define PROG_NAME "qjs"

int main(int argc, char** argv) {
  JSRuntime* rt;
  JSContext* ctx;
  rt = JS_NewRuntime();
  int r = 0;
  /* save for later */
  qjs__argc = argc;
  qjs__argv = argv;

  if (!rt) {
    fprintf(stderr, "qjs: cannot allocate JS runtime\n");
    exit(2);
  }
  js_std_set_worker_new_context_func(JS_NewCustomContext);
  js_std_init_handlers(rt);
  ctx = JS_NewCustomContext(rt);
  if (!ctx) {
    fprintf(stderr, "qjs: cannot allocate JS context\n");
    exit(2);
  }

  /* loader for ES6 modules */
  JS_SetModuleLoaderFunc2(rt, NULL, js_module_loader,
                          js_module_check_attributes, NULL);

  /* exit on unhandled promise rejections */
  JS_SetHostPromiseRejectionTracker(rt, js_std_promise_rejection_tracker, NULL);


  eval_buf(ctx, str, strlen(str), "<input>", JS_EVAL_TYPE_MODULE);
  
  if (0) {
  } else {
    r = js_std_loop(ctx);
  }
  if (r) {
    js_std_dump_error(ctx);
    goto fail;
  }
 
  js_std_free_handlers(rt);
  JS_FreeContext(ctx);
  JS_FreeRuntime(rt);

  return 0;
fail:
  js_std_free_handlers(rt);
  JS_FreeContext(ctx);
  JS_FreeRuntime(rt);
  return 1;
}
