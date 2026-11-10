# Spec — Sintetizador Microtonal (JUCE / VST3)

Nov 6, 2026 · @Guilherme

## Visão geral

Vamos construir um sintetizador VST3 em C++/JUCE que toca em qualquer divisão igual da oitava (EDO) e com frequência de referência ajustável. O synth calcula a frequência de cada nota por conta própria, sem depender de pitch bend, MPE ou do Tuning System do Live.

- **Nome provisório:** MicroSynth (a definir).
- **Plataforma do MVP:** Windows 11, formato VST3 e app Standalone, testado no Ableton Live 12.
- **Uso inicial:** próprio. A comercialização vem depois que o MVP se provar no dia a dia.
- **Diferencial:** funciona com qualquer EDO (24, 21, 31, 52…) e qualquer frequência base, com a afinação automatizável como parâmetro do plugin.

## Escopo do MVP

O MVP prova uma coisa: tocar uma escala de 24 EDO no Ableton e ouvir os quartos de tom afinados. Tudo o que não serve a isso fica para depois.

**Dentro do MVP**

- Synth polifônico (8 vozes) com um oscilador por voz: senoide, dente de serra, quadrada e triângulo.
- Envelope ADSR de amplitude.
- Filtro passa-baixa com cutoff e ressonância.
- Motor de afinação: divisões por oitava, frequência base e nota de referência.
- Pitch bend global simples (sem MPE).
- Interface genérica do JUCE (sliders automáticos) e depois uma interface própria mínima.
- Estado salvo no projeto do Ableton (parâmetros persistem ao reabrir o set).

**Fora do MVP**

- Importação de arquivos Scala (.scl/.kbm).
- Suporte a MTS-ESP e MPE.
- Wavetables, LFOs, efeitos internos.
- Sistema de presets próprio.
- macOS/AU, instalador, licenciamento e venda.

## Requisitos funcionais e parâmetros

Todos os parâmetros abaixo são expostos ao host, então podem ser automatizados no Ableton. Os valores de faixa são uma proposta inicial e podem ser ajustados.

| Grupo | Parâmetro (ID) | Faixa | Padrão |
| --- | --- | --- | --- |
| Afinação | Divisões por oitava (`edo`) | 1 a 72 (inteiro) | 12 |
| Afinação | Frequência base (`baseHz`) | 20 a 2000 Hz | 440 Hz |
| Afinação | Nota de referência (`refNote`) | 0 a 127 (nota MIDI) | 69 (A3 no Live) |
| Afinação | Pitch bend range (`bendSteps`) | 0 a 24 passos da EDO | 2 |
| Oscilador | Forma de onda (`wave`) | Seno, Serra, Quadrada, Triângulo | Serra |
| Envelope | Attack / Decay / Sustain / Release | 1 ms a 5 s / 1 ms a 5 s / 0 a 1 / 1 ms a 10 s | 5 ms / 200 ms / 0,8 / 300 ms |
| Filtro | Cutoff (`cutoff`) | 20 Hz a 20 kHz (escala log) | 8 kHz |
| Filtro | Ressonância (`reso`) | 0,1 a 10 | 0,7 |
| Saída | Volume (`gain`) | -48 a +6 dB | -12 dB |

**Parâmetros pós-MVP (v0.2)**

| Grupo | Parâmetro (ID) | Faixa | Padrão |
| --- | --- | --- | --- |
| Osciladores | Liga/desliga (`osc1On`, `osc2On`, `osc3On`) | sim/não | Osc 1 ligado; 2 e 3 desligados |
| Osciladores | Forma de onda (`wave` = osc 1, `osc2Wave`, `osc3Wave`) | Seno, Serra, Quadrada, Triângulo | Serra |
| Osciladores | Volume (`osc1Level`, `osc2Level`, `osc3Level`) | 0 a 100 % | 100 % |
| Osciladores | Oitava (`osc1Octave`, `osc2Octave`, `osc3Octave`) | -3 a +3 | 0 |
| Osciladores | Transposição (`osc1Steps`, `osc2Steps`, `osc3Steps`) | -24 a +24 passos da EDO | 0 |
| Osciladores | Desafinação fina (`osc1Fine`, `osc2Fine`, `osc3Fine`) | -100 a +100 cents | 0 |
| Vozes | Modo (`voiceMode`) | Poly, Mono | Poly |
| Vozes | Legato (`legato`) | sim/não | sim |
| Vozes | Glide (`glide`) | 0 a 2000 ms | 0 ms |
| Afinação | Trava de afinação (`tuningLock`) | sim/não (não automatizável) | não |

**Parâmetros da matriz de modulação (v0.4)**

| Grupo | Parâmetro (ID) | Faixa | Padrão |
| --- | --- | --- | --- |
| Matriz | Fonte do slot N (`mod1Source` … `mod8Source`) | lista de fontes (abaixo), não automatizável | None |
| Matriz | Destino do slot N (`mod1Dest` … `mod8Dest`) | lista de destinos (abaixo), não automatizável | None |
| Matriz | Quantidade do slot N (`mod1Amount` … `mod8Amount`) | -100 a +100 % | 0 % |
| Mod Env | Attack / Decay / Sustain / Release (`modEnvAttack`, `modEnvDecay`, `modEnvSustain`, `modEnvRelease`) | mesmas faixas do envelope de volume | 5 ms / 300 ms / 0 / 300 ms |

**Listas fechadas** (a ordem é o valor salvo: nunca reordenar nem remover; opções novas só ocupam as vagas reservadas)

- Fontes: None, Velocity, Mod Wheel, Aftertouch, Key Track, Mod Env, LFO 1, LFO 2, LFO 3, Reserved 1, Reserved 2.
- Destinos: None, Cutoff, Resonance, Pitch, Osc 1 Level, Osc 1 Pitch, Osc 1 Fine, Osc 2 Level, Osc 2 Pitch, Osc 2 Fine, Osc 3 Level, Osc 3 Pitch, Osc 3 Fine, Reserved 1, Reserved 2, Reserved 3, Reserved 4.

**Comportamentos obrigatórios**

- RF1: cada nota MIDI recebida toca na frequência calculada pelo motor de afinação.
- RF2: com `edo = 12`, `baseHz = 440` e `refNote = 69`, o synth soa idêntico ao temperamento padrão.
- RF3: mudar `edo`, `baseHz` ou `refNote` afeta as próximas notas tocadas (o reajuste de notas já presas fica para depois do MVP).
- RF4: notas cuja frequência fique abaixo de 20 Hz ou acima de 45% da taxa de amostragem são ignoradas, para evitar sons inaudíveis e aliasing.
- RF5: o estado completo é salvo e restaurado com o set do Ableton.
- RF6 (v0.2): cada voz tem 3 osciladores somados antes do filtro. A frequência de cada um é a da nota × 2^(oitava + passos / `edo` + cents / 1200). O oscilador 1 continua usando o parâmetro `wave`, e os osciladores 2 e 3 começam desligados, então sets salvos na v0.1 soam igual. Ligar, desligar ou mudar o volume de um oscilador é suavizado (20 ms) para não estalar. Um oscilador cuja frequência transposta saia da faixa do RF4 fica mudo; os outros continuam tocando. Não há compensação automática de volume: com 3 osciladores no máximo, o som fica mais alto.
- RF7 (v0.2): no modo **Poly**, o synth toca até 8 notas (comportamento da v0.1). No modo **Mono**, toca uma nota por vez, com prioridade para a última nota tocada; ao soltar a nota atual, volta para a nota anterior que ainda está pressionada. Com `legato` ligado, trocar de nota com outra tecla ainda pressionada não redispara o envelope; com `legato` desligado, toda nota redispara o envelope a partir do nível atual (sem estalo). O `glide` desliza a altura da nota anterior para a nova em tempo constante, linear em passos da EDO (exponencial em Hz), sempre que a voz já está soando. Trocar de modo solta todas as notas (com release).
- RF8 (v0.2): a interface é organizada em abas: **Oscillators**, **Amp & Filter** (vozes, envelope, filtro e volume) e **Tuning**. Na aba Tuning, o botão de trava (`tuningLock`) desabilita os knobs de afinação na interface para evitar mudanças acidentais. A trava é salva com o set, mas não é automatizável e não bloqueia automação do host.
- RF9 (v0.3): a aba Amp & Filter mostra dois gráficos que acompanham os knobs e a automação em tempo real: a curva de resposta do filtro passa-baixa (frequência em escala log de 20 Hz a 20 kHz, ganho em dB) e o desenho do envelope ADSR. Os tempos do envelope são desenhados em escala de raiz quadrada, para que um attack de 5 ms continue visível ao lado de um release de 10 s; o sustain ocupa uma faixa fixa. Um ponto mostra a posição atual do envelope da última nota tocada. Os gráficos só leem valores (atômicos), sem travar a thread de áudio.
- RF10 (v0.4): matriz de modulação com 8 slots (fonte → destino → quantidade), calculada por voz a cada bloco.
  - Fontes unipolares (0 a 1): Velocity, Mod Wheel, Aftertouch, Mod Env. Bipolares (-1 a +1): Key Track (−1 na nota MIDI 0, +1 na 127) e LFOs. LFOs valem 0 até o RF11.
  - A modulação soma `fonte × quantidade` ao valor normalizado (0 a 1) do destino, com limite nas pontas; vários slots no mesmo destino se somam. No Pitch, 100 % = 24 passos da EDO.
  - Mod Wheel e Aftertouch são suavizados (20 ms) para não estalar.
  - Interface: aba **Matrix** com alças arrastáveis das fontes, o Mod Env (knobs + gráfico com ponto) e a lista dos 8 slots. Soltar uma fonte num knob de destino ocupa o primeiro slot livre (com os 8 ocupados, aparece um aviso). Knobs modulados mostram um anel com o alcance da modulação; arrastar o anel ajusta a quantidade do primeiro slot ligado àquele knob.
- RF11 (v0.5): 3 LFOs (fontes LFO 1–3 da matriz), com forma de onda, rate livre ou sincronizado ao tempo do host e modo por nota ou livre. Detalhes no card do LFO.

## Modelo de afinação

Cada tecla MIDI vira um passo da EDO, contado a partir da nota de referência, que soa na frequência base.

```latex
f(n) = f_{base} \cdot 2^{\frac{n - n_{ref}}{N}}
```

Na fórmula, n é a nota MIDI recebida, n\_ref é `refNote`, N é `edo` e f\_base é `baseHz`. Com pitch bend, o expoente ganha + bend × `bendSteps` / N, em que bend vai de -1 a +1.

**Exemplos (baseHz = 440, refNote = 69)**

| Nota MIDI | 12 EDO | 24 EDO |
| --- | --- | --- |
| 69 | 440,00 Hz | 440,00 Hz |
| 70 | 466,16 Hz | 452,89 Hz |
| 71 | 493,88 Hz | 466,16 Hz |
| 72 | 523,25 Hz | 479,82 Hz |

**Cobertura de oitavas**

São só 128 notas MIDI, então quanto maior a EDO, menos oitavas cabem no teclado. Com `refNote = 69`:

- 24 EDO cobre cerca de 5,3 oitavas, de 60 Hz a 2,3 kHz.
- 52 EDO cobre cerca de 2,4 oitavas, de 175 Hz a 953 Hz.

No MVP, mover `refNote` resolve isso: ele desloca a janela de notas audíveis para o grave ou para o agudo. Um seletor de oitava mais amigável entra no roadmap.

**Atenção no Ableton:** não carregue um Tuning System do Live no mesmo set. O Live mandaria pitch bend MPE e a afinação seria aplicada duas vezes. O piano roll continuará mostrando nomes de notas de 12 tons, que não correspondem ao que soa.

## Arquitetura técnica

O projeto usa JUCE 8 com CMake, e a afinação fica isolada numa classe pura (`TuningEngine`) que não depende do JUCE e pode ser testada sozinha.

&#91;embedded content: arquitetura · 7 componentes\]

O Live manda o MIDI ao PluginProcessor; cada voz consulta o TuningEngine no note-on, e o áudio volta ao Live pelo processBlock.

**Responsabilidades das classes**

| Classe | Base JUCE | Responsabilidade |
| --- | --- | --- |
| `PluginProcessor` | `juce::AudioProcessor` | Dono dos parâmetros (`AudioProcessorValueTreeState`), do `Synthesiser` e do estado salvo; repassa MIDI e áudio no `processBlock` |
| `PluginEditor` | `juce::AudioProcessorEditor` | Interface. Começa como `GenericAudioProcessorEditor`, depois vira knobs próprios |
| `TuningEngine` | nenhuma (C++ puro) | `noteToHz(note, bend)` a partir de `edo`, `baseHz`, `refNote`, `bendSteps`; sem alocação, segura para a thread de áudio |
| `MicroVoice` | `juce::SynthesiserVoice` | Uma voz: oscilador, ADSR e filtro; pede a frequência ao `TuningEngine` no note-on |
| `MicroSound` | `juce::SynthesiserSound` | Marcador exigido pelo `Synthesiser` (aceita todas as notas e canais) |
| `MicroSynthesiser` | `juce::Synthesiser` | Guarda do RF4 e modo Mono (RF7): pilha de notas pressionadas e troca de nota na voz 0 |
| `Oscillator` | nenhuma (C++ puro) | Oscilador com PolyBLEP; cada voz tem 3 (RF6) |
| `Glide` | nenhuma (C++ puro) | Rampa linear da altura (em notas/passos) para o portamento |
| `NoteStack` | nenhuma (C++ puro) | Pilha de tamanho fixo das notas pressionadas no modo Mono, sem alocação |

**Estrutura de pastas**

```
microsynth/
├── CMakeLists.txt
├── libs/
│   └── JUCE/              (submódulo git, JUCE 8)
├── src/
│   ├── PluginProcessor.h/.cpp
│   ├── PluginEditor.h/.cpp
│   ├── TuningEngine.h/.cpp
│   ├── MicroVoice.h/.cpp
│   ├── MicroSound.h
│   └── Parameters.h       (IDs e layout dos parâmetros)
└── tests/
    └── TuningEngineTests.cpp
```

**Trecho principal do CMakeLists.txt**

```cmake
cmake_minimum_required(VERSION 3.22)
project(MicroSynth VERSION 0.1.0)

add_subdirectory(libs/JUCE)

juce_add_plugin(MicroSynth
    COMPANY_NAME "Guilherme"
    PLUGIN_MANUFACTURER_CODE Gmsy
    PLUGIN_CODE Mcsy
    FORMATS VST3 Standalone
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    PRODUCT_NAME "MicroSynth")

target_sources(MicroSynth PRIVATE
    src/PluginProcessor.cpp src/PluginEditor.cpp
    src/TuningEngine.cpp src/MicroVoice.cpp)

target_link_libraries(MicroSynth PRIVATE
    juce::juce_audio_utils juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
```

**Regras de thread de áudio**

- Nada de alocação de memória, locks ou I/O dentro do `processBlock` e das vozes.
- Parâmetros lidos via `getRawParameterValue()` (atômicos).
- O oscilador de dente de serra e o de quadrada usam PolyBLEP para reduzir aliasing.

## Ambiente de desenvolvimento (Windows 11)

O setup leva por volta de uma hora, e a maior parte é o download do Visual Studio.

- [ ] Instalar o **Visual Studio Community** (versão mais recente) com o workload "Desenvolvimento para desktop com C++"
- [ ] Instalar o **Git** e o **CMake** (3.22 ou superior; o do Visual Studio também serve)
- [ ] Criar o repositório `microsynth` e adicionar o JUCE como submódulo: `git submodule add https://github.com/juce-framework/JUCE libs/JUCE`
- [ ] Baixar o **pluginval** (Tracktion) para validar o plugin
- [ ] Compilar o **AudioPluginHost** do JUCE (em `libs/JUCE/extras`) para testes rápidos fora do Ableton
- [ ] No Ableton: Preferências → Plug-Ins → ativar "Use VST3 Plug-in System Folders"

**Comandos de build**

```
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Debug
```

O gerador deve corresponder à versão do Visual Studio instalada. O arquivo `MicroSynth.vst3` sai em `build/MicroSynth_artefacts/Debug/VST3/`. Com `COPY_PLUGIN_AFTER_BUILD TRUE` no `juce_add_plugin`, o JUCE copia o plugin para `C:\Program Files\Common Files\VST3`, o que exige rodar o build como administrador. A alternativa é apontar uma pasta personalizada de VST3 no Ableton.

## Plano de implementação

São seis etapas curtas, e cada uma termina com algo que você pode tocar ou testar. A ordem prioriza provar a afinação o quanto antes (etapa 3).

| Etapa | Entrega | Critério de aceite |
| --- | --- | --- |
| 1. Esqueleto | Projeto CMake + JUCE compilando um plugin VST3 vazio | O Ableton lista e carrega o MicroSynth numa track MIDI sem crash |
| 2. Som básico | `Synthesiser` com 8 vozes, oscilador senoidal e ADSR fixo, afinação padrão do JUCE | Tocar o teclado no Live produz som com polifonia, sem estalos no note-off |
| 3. Motor de afinação | `TuningEngine` + parâmetros `edo`, `baseHz`, `refNote`; testes unitários passando | Com 24 EDO, a nota 70 mede cerca de 452,9 Hz no Tuner do Live |
| 4. Synth completo | Formas de onda com PolyBLEP, filtro passa-baixa, parâmetros de ADSR, ganho e pitch bend | Todos os parâmetros aparecem na automação do Live e respondem em tempo real |
| 5. Estado e robustez | Salvar/restaurar estado, guarda de Nyquist (RF4), pluginval | O set reabre com os mesmos valores; pluginval passa no nível 5 |
| 6. Interface própria | Editor com knobs agrupados (Afinação, Oscilador, Envelope, Filtro) | Usável sem a interface genérica; redimensiona sem quebrar o layout |

**Pós-MVP (v0.2)** — cards do kanban

| Etapa | Entrega | Critério de aceite |
| --- | --- | --- |
| 7. Múltiplos osciladores | 3 osciladores por voz (RF6) | Set da v0.1 soa igual; ligar o osc 2 uma oitava abaixo engrossa o som sem estalo |
| 8. Modo Mono | `voiceMode`, `legato`, `glide` (RF7) | Em Mono, segurar uma nota e tocar outra desliza (glide) sem redisparar o envelope; soltar volta à anterior |
| 9. Abas e trava | Abas Oscillators / Amp & Filter / Tuning e `tuningLock` (RF8) | Com a trava ligada, os knobs de afinação não mexem; reabrir o set mantém a trava |
| 10. Gráficos | Curva do filtro e desenho do ADSR com ponto de posição (RF9) | Girar Cutoff/Resonance/ADSR ou automatizá-los move os gráficos; tocar uma nota faz o ponto percorrer o envelope |
| 11. Matriz de modulação | 8 slots, Mod Env, aba Matrix, arrastar e soltar, anel nos knobs (RF10) | Velocity → Cutoff abre o filtro com notas fortes; arrastar Mod Env até o Cutoff cria o slot; o anel acompanha e ajusta a quantidade |
| 12. LFOs | 3 LFOs como fontes (RF11) | LFO 1 → Pitch faz vibrato audível; com sync, acompanha o tempo do Live |

Uma sugestão de ritmo: as etapas 1 e 2 cabem numa primeira noite, e a etapa 3 é o marco que prova a ideia.

## Testes e validação

A afinação é verificada por código e de ouvido; a estabilidade é verificada pelo pluginval.

**Testes unitários do TuningEngine** (Catch2 ou `juce::UnitTest`)

- Com 12 EDO, 440 Hz e referência 69, `noteToHz` bate com `juce::MidiMessage::getMidiNoteInHertz` para as notas 0 a 127 (tolerância de 0,01 Hz).
- Subir N passos com N divisões dobra a frequência, para N = 12, 19, 24, 31 e 52.
- A nota de referência sempre devolve exatamente `baseHz`.
- Pitch bend máximo com `bendSteps = 2` equivale a tocar a nota dois passos acima.

**Testes manuais no Ableton**

- Colocar o Tuner do Live depois do MicroSynth e conferir a tabela de exemplos do modelo de afinação.
- Gravar automação de `edo` e `baseHz` e conferir que as notas seguintes mudam.
- Carregar 4 instâncias com 8 vozes cada e acompanhar a CPU no medidor do Live.
- Salvar o set, fechar, reabrir e conferir os parâmetros.

**Validação automática**

- pluginval com `--strictness-level 5` antes de considerar qualquer versão estável.

## Roadmap pós-MVP e questões em aberto

Depois que o MVP estiver em uso, a ordem abaixo prioriza o que mais diferencia o produto.

1. Reajuste das notas presas ao mudar a afinação, com glide suave (afinação automatizável "de verdade").
2. Seletor de oitava e teclado visual com N teclas por oitava.
3. Importação de arquivos Scala (.scl/.kbm) para afinações não iguais.
4. Cliente MTS-ESP, para obedecer à afinação de outros plugins.
5. Wavetables, LFOs e um sistema de presets.
6. Build para macOS (VST3 + AU), instalador, licenciamento e loja.

**Questões em aberto**

- [ ] Nome definitivo do produto e códigos de fabricante e plugin (`Gmsy`/`Mcsy` são provisórios).
- [ ] Faixa máxima de EDO: 72 é suficiente, ou vale ir até 128 ou mais?
- [ ] Unidade do pitch bend: passos da EDO (proposto) ou cents?
- [x] Qual biblioteca de testes usar: Catch2 ou `juce::UnitTest`? → **Catch2 v3** (07/11/2026), testa o `TuningEngine` sem JUCE.
- [ ] Licença do código: repositório privado (closed source) desde já?
