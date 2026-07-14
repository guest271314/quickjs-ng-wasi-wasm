# quickjs-ng-wasi-wasm
Embed JavaScript into QuickJS NG, compile to WASM with WASI-SDK

## Usage

Write your JavaScript in `index.js`, run `build.sh`. Output file is `qjs-wasi.wasm`, your JavaScript embedded into the 
compiled WASM file with WASI support, so you can do `wasmtime qjs-wasi.wasm` to execute your embedded JavaScript with the `qjs` engine in WASM.
