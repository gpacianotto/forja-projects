# Matiza

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

O app Standalone sai em `build/Matiza_artefacts/Debug/Standalone/Matiza.exe`.

## Progresso

| Etapa | Status |
| --- | --- |
| 1. Esqueleto | Testada no Ableton |
| 2. Som básico (8 vozes, seno, ADSR fixo) | Testada no Ableton |
| 3. Motor de afinação (`edo`, `baseHz`, `refNote`) | Testada no Ableton |
| 4. Synth completo (ondas PolyBLEP, filtro, ADSR, volume, pitch bend) | Testada no Ableton |
| 5. Estado e robustez (RF4, RF5, pluginval) | Testada no Ableton |
| 6. Interface própria (grupos Tuning, Oscillator, Output, Envelope, Filter; redimensionável) | Testada no Ableton |

**MVP v0.1.0 concluído em 2026-11-07.** Próximo item do roadmap (Spec): reajuste das notas presas ao mudar a afinação, com glide.

## Validações

| Versão | Data | pluginval | Resultado |
| --- | --- | --- | --- |
| 0.1.0 (etapa 5) | 2026-11-07 | 1.0.4, nível 5, Release | Passou (o validador VST3 da Steinberg não estava configurado e foi pulado) |
| 0.1.0 (etapa 6) | 2026-11-07 | 1.0.4, nível 5, Release | Passou, com a interface própria (validador VST3 da Steinberg pulado) |
| 0.1.0 (rebranding Matiza) | 2026-11-10 | 1.0.4, nível 5, Release | Passou como "Forja Instruments: Matiza" (códigos `Forj`/`Mtza`), com a paleta Têmpera |
| 0.1.0 (unison, RF12) | 2026-11-10 | 1.0.4, nível 5, Release | Passou, com voz estéreo e até 8 cópias por oscilador |
| 0.1.0 (wavetable, RF13) | 2026-11-10 | 1.0.4, nível 5, Release | Passou, com osciladores de wavetable e mipmaps |
| 0.1.0 (anéis por slot, RF14) | 2026-11-10 | 1.0.4, nível 5, Release | Passou |
| 0.1.0 (formas desenhadas, RF15) | 2026-11-10 | 1.0.4, nível 5, Release | Passou, com os pontos no estado |
