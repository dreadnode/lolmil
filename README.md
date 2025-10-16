# LOLMIL

Code for [LOLMIL: Living Off the Land Models and Inference Libraries](https://dreadnode.io/blog/lolmil-living-off-the-land-models-and-inference-libraries)

## Building

1. Download [Phi-3-mini-4k-instruct-onnx](https://huggingface.co/microsoft/Phi-3-mini-4k-instruct-onnx)
2. Update [config.json](./config.json.example)
3. Build

```bash
cmake -B build -G "Visual Studio 17 2022" -A x64
```

4. Run

```bash
.\build\Release\onnxtest.exe
```

