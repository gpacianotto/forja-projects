# Spec — HelloGain

Nov 6, 2026 · @Guilherme

## Visão geral

Efeito VST3 de ganho, sem DSP próprio além de multiplicar o sinal. Existe só para provar a cadeia
inteira do workspace (template → build → pasta VST3 → Ableton → pluginval) antes de começar o
MicroSynth. Pode ser apagado depois que a verificação final do setup passar.

## Escopo do MVP

- Um parâmetro de ganho em dB, aplicado a mono ou estéreo.
- Interface genérica do JUCE (um slider por parâmetro).
- Estado salvo no set do Ableton.

Fora do escopo: interface própria, medidores, qualquer outro processamento.

## Parâmetros

| ID | Nome | Faixa | Padrão | Unidade | Suavização |
| --- | --- | --- | --- | --- | --- |
| `gain` | Gain | -48 a +6 (passo 0,1) | 0 | dB | rampa linear de 20 ms (`SmoothedValue`) |

## Arquitetura

- `PluginProcessor`: código do template `juce-plugin`, sem alterações.
  - Barramentos: entrada e saída com o mesmo layout, mono ou estéreo.
  - `processBlock`: converte dB → ganho linear e multiplica cada amostra, com rampa para evitar estalos.
  - Estado: `AudioProcessorValueTreeState` serializado em XML.
- Formatos: VST3 (copiado para `C:\Program Files\Common Files\VST3\Dev`) e Standalone.
- `PLUGIN_CODE`: `Hlgn`.

## Plano de implementação

1. **Build Debug.** `cmake -B build` e `cmake --build build --config Debug` terminam sem erros, e o
   `HelloGain.vst3` aparece em `VST3\Dev`.
2. **Teste no Ableton.** Depois do Rescan, o HelloGain aparece em Plug-Ins → VST3, carrega numa track de
   áudio, e o slider Gain altera o volume sem estalos. Salvar o set, reabrir e conferir que o valor voltou.
3. **Validação.** Skill `build-e-validar`: build Release e pluginval nível 5 sem falhas.

## Testes

Não há lógica pura para testar isoladamente: o comportamento é coberto pelo teste manual no Ableton
(etapa 2) e pelo pluginval (etapa 3), que já testa estado, automação e diferentes tamanhos de bloco.
