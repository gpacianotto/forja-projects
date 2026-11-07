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
Testes unitários (Catch2, baixado pelo CMake no primeiro `cmake -B build`):

```
ctest --test-dir build -C Debug
```

O app Standalone sai em `build/MicroSynth_artefacts/Debug/Standalone/MicroSynth.exe`.

## Progresso

| Etapa | Status |
| --- | --- |
| 1. Esqueleto | Testada no Ableton |
| 2. Som básico (8 vozes, seno, ADSR fixo) | Testada no Ableton |
| 3. Motor de afinação (`edo`, `baseHz`, `refNote`) | Testada no Ableton |
| 4. Synth completo (ondas PolyBLEP, filtro, ADSR, volume, pitch bend) | Compila, testes passam, pluginval nível 5 (Debug) passou; aguardando teste no Ableton |
