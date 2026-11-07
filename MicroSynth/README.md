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
| 4. Synth completo (ondas PolyBLEP, filtro, ADSR, volume, pitch bend) | Testada no Ableton |
| 5. Estado e robustez (RF4, RF5, pluginval) | pluginval nível 5 passou; aguardando teste de reabrir o set no Ableton |

## Validações

| Versão | Data | pluginval | Resultado |
| --- | --- | --- | --- |
| 0.1.0 (etapa 5) | 2026-11-07 | 1.0.4, nível 5, Release | Passou (o validador VST3 da Steinberg não estava configurado e foi pulado) |
