# MicroSynth

Sintetizador VST3 microtonal: toca em qualquer divisão igual da oitava (EDO), com frequência base
e nota de referência ajustáveis.
Especificação (fonte da verdade): `Spec — Sintetizador Microtonal (JUCE VST3).md`.

## Build

```
cmake -B build
cmake --build build --config Debug
```

O `.vst3` é copiado automaticamente para `C:\Program Files\Common Files\VST3\Dev`.
O app Standalone sai em `build/MicroSynth_artefacts/Debug/Standalone/MicroSynth.exe`.

## Progresso

| Etapa | Status |
| --- | --- |
| 1. Esqueleto | Compila; aguardando teste no Ableton |
