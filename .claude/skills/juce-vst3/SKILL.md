---
name: juce-vst3
description: Padrões para implementar plugins VST3 com JUCE 8 e CMake neste workspace (efeitos e sintetizadores): parâmetros, vozes, DSP, estado e regras de tempo real. Use ao escrever ou revisar código C++ de qualquer projeto VST3.
---

# Implementando VST3 com JUCE

## Base do projeto

- O `CMakeLists.txt` inclui o JUCE compartilhado:
  `add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../_libs/JUCE" "${CMAKE_BINARY_DIR}/JUCE")`.
- C++20. Módulos usuais: `juce_audio_utils`, `juce_dsp`.
- Formatos: `VST3 Standalone`. O Standalone ajuda a testar sem o Ableton.
- `COPY_PLUGIN_AFTER_BUILD TRUE` com `VST3_COPY_DIR "C:/Program Files/Common Files/VST3/Dev"`.

## Parâmetros

- Sempre `juce::AudioProcessorValueTreeState` (APVTS) como membro público `apvts`.
- Layout numa função estática `createParameterLayout()`, IDs com `juce::ParameterID { "id", 1 }`.
- IDs em camelCase, centralizados num `Parameters.h` (`namespace ParamIDs { inline constexpr auto gain = "gain"; }`).
- No construtor, guardar `std::atomic<float>*` de `apvts.getRawParameterValue(...)` para ler no áudio.
- Faixas: frequências com `NormalisableRange` + `setSkewForCentre` (ex.: centro em 1 kHz); escolhas com
  `AudioParameterChoice`; inteiros com `AudioParameterInt`.
- Estado: `getStateInformation` → `apvts.copyState().createXml()` + `copyXmlToBinary`;
  `setStateInformation` → `getXmlFromBinary` + `apvts.replaceState`.

## Efeito vs. instrumento

- Efeito: `BusesProperties().withInput("Input", stereo).withOutput("Output", stereo)`.
- Instrumento: só `.withOutput("Output", stereo)`; `IS_SYNTH TRUE`, `NEEDS_MIDI_INPUT TRUE` no CMake;
  `acceptsMidi()` retorna `true`.

## Sintetizadores

- `juce::Synthesiser` + subclasses de `SynthesiserVoice` e `SynthesiserSound`.
- No construtor do processador: `addVoice` N vezes, `addSound` uma vez.
- `prepareToPlay`: `synth.setCurrentPlaybackSampleRate(sr)` e preparar o DSP de cada voz (`juce::dsp::ProcessSpec`).
- `processBlock`: `buffer.clear()`, atualizar parâmetros das vozes, `synth.renderNextBlock(buffer, midi, 0, n)`.
- Voz: em `startNote`, calcular a frequência (projetos microtonais: **nunca** `MidiMessage::getMidiNoteInHertz`,
  sempre a classe de afinação do projeto), resetar fase e disparar o ADSR (`juce::ADSR`).
  Em `stopNote`, `adsr.noteOff()` com `allowTailOff`; quando o envelope acabar, `clearCurrentNote()`.
- Em `renderNextBlock` da voz, **somar** (`addSample`) ao buffer de saída, nunca sobrescrever.

## DSP

- Filtros: `juce::dsp::StateVariableTPTFilter` (estável sob modulação) ou `juce::dsp::LadderFilter`.
- Ganho: `juce::Decibels::decibelsToGain` + `juce::SmoothedValue` (rampa de 20 ms).
- Osciladores: implementar à mão para controlar fase; serra e quadrada com PolyBLEP.
- Sempre `juce::ScopedNoDenormals` no início do `processBlock`.
- Frequências acima de ~0,45 × sample rate geram aliasing: limitar ou silenciar a voz.

## Regras de tempo real (repetidas por importância)

Nada de alocação, locks, I/O ou log na thread de áudio. Na dúvida, mover para `prepareToPlay`.

## Interface

1. Começar com `juce::GenericAudioProcessorEditor` (zero código de UI).
2. Interface própria só na etapa prevista no Spec: `Slider` + `AudioProcessorValueTreeState::SliderAttachment`,
   layout em `resized()` com `juce::FlexBox` ou `Grid`, componentes agrupados.

## Testes

- Lógica pura (afinação, conversões) em classes sem dependência do JUCE, testadas com Catch2
  (via `FetchContent`) ou `juce::UnitTest`, registradas no `ctest`.
- Teste de referência: comparar com valores conhecidos (ex.: 12 EDO, A = 440 Hz ↔ `getMidiNoteInHertz`).

## Referências

- Tutoriais oficiais: https://juce.com/learn/tutorials
- Exemplos CMake: `_libs/JUCE/examples/CMake/AudioPlugin`
- Código aberto para estudo (GPL, não copiar para produto fechado): Surge XT, Vital.
