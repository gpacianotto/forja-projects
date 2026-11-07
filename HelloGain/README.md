# HelloGain

Efeito VST3 de ganho (-48 a +6 dB) criado só para validar o setup do workspace. Pode ser apagado.
Especificação: `Spec — HelloGain.md`.

## Build

```
cmake -B build
cmake --build build --config Release
```

O `.vst3` é copiado automaticamente para `C:\Program Files\Common Files\VST3\Dev`.

## Validações

| Versão | Data | pluginval | Resultado |
| --- | --- | --- | --- |
| 0.1.0 | 2026-11-06 | 1.0.4, nível 5 | Passou (o validador VST3 da Steinberg não estava configurado e foi pulado) |
