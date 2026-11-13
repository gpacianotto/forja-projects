# Spec — Sintetizador Microtonal (JUCE / VST3)

Nov 6, 2026 · @Guilherme

## Visão geral

Vamos construir um sintetizador VST3 em C++/JUCE que toca em qualquer divisão igual da oitava (EDO) e com frequência de referência ajustável. O synth calcula a frequência de cada nota por conta própria, sem depender de pitch bend, MPE ou do Tuning System do Live.

- **Nome:** Matiza, da marca Forja Instruments (definido em 10/11/2026; ver `.kanban/docs/Plano de produto — Matiza, by Forja Instruments.md`).
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
| Osciladores (RF12) | Unison (`osc1Unison`, `osc2Unison`, `osc3Unison`) | 1 a 8 cópias | 1 |
| Osciladores (RF12) | Detune (`osc1Detune`, `osc2Detune`, `osc3Detune`) | 0 a 100 cents | 25 ct |
| Osciladores (RF12) | Blend (`osc1Blend`, `osc2Blend`, `osc3Blend`) | 0 a 100 % | 50 % |
| Osciladores (RF12) | Width (`osc1Width`, `osc2Width`, `osc3Width`) | 0 a 100 % | 100 % |

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

**Parâmetros dos LFOs (v0.5)** — N = 1, 2, 3

| Grupo | Parâmetro (ID) | Faixa | Padrão |
| --- | --- | --- | --- |
| LFO | Forma (`lfoNShape`) | Sine, Triangle, Saw Up, Saw Down, Square, Sample & Hold | Sine |
| LFO | Rate (`lfoNRate`) | 0,02 a 40 Hz (centro em 2 Hz) | 2 Hz |
| LFO | Sync (`lfoNSync`) | sim/não | não |
| LFO | Divisão (`lfoNDivision`) | 1/32, 1/16T, 1/16, 1/8T, 1/8, 1/8D, 1/4T, 1/4, 1/4D, 1/2, 1 Bar, 2 Bars, 4 Bars | 1/4 |
| LFO | Trigger (`lfoNTrigger`) | Free, Retrigger | Retrigger |

As listas de forma e divisão também são fechadas: nunca reordenar nem remover opções.

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
  - Interface: aba **Matrix** com alças arrastáveis das fontes, o Mod Env (knobs + gráfico com ponto) e a lista dos 8 slots. Soltar uma fonte num knob de destino ocupa o primeiro slot livre (com os 8 ocupados, aparece um aviso). Knobs modulados mostram um anel com o alcance da modulação; arrastar o anel ajusta a quantidade do primeiro slot ligado àquele knob (mudou no RF14: um anel por slot).
- RF11 (v0.5): 3 LFOs, fontes bipolares (-1 a +1) LFO 1–3 da matriz.
  - Formas: Sine, Triangle, Saw Up, Saw Down, Square, Sample & Hold (valor aleatório novo a cada ciclo).
  - Rate livre em Hz ou, com `Sync`, uma divisão do tempo do host (sem tempo disponível, usa 120 BPM).
  - Trigger **Free**: todas as vozes compartilham a mesma fase; com Sync e o Live tocando, a fase segue a posição da música (o LFO cai no tempo). Trigger **Retrigger**: cada nota reinicia o LFO da sua voz na fase 0.
  - Interface: aba **LFOs** com os 3 LFOs; com Sync ligado, o knob Rate é desabilitado e a divisão é habilitada. LFO 1–3 entram nas alças arrastáveis do cabeçalho.
  - Para a modulação rápida do LFO não ser abafada, a modulação do cutoff e do volume dos osciladores é interpolada a cada recálculo da matriz (32 amostras), separada da suavização de 20 ms dos knobs.

- RF12 (v0.6): unison em cada oscilador, como no Serum.
  - **Unison** (`osc1Unison`, `osc2Unison`, `osc3Unison`): 1 a 8 cópias da onda por nota. Com 1, o oscilador soa como antes (mesma fase inicial, centro, sem detune).
  - **Detune** (`osc1Detune`…): 0 a 100 cents. As cópias ficam espalhadas por igual entre −detune e +detune em volta da nota (em cents absolutos, iguais em qualquer EDO).
  - **Blend** (`osc1Blend`…): 0 a 100 %. Volume das cópias desafinadas em relação às centrais (a do meio, com número ímpar de cópias, ou as duas do meio, com número par). Com 0 %, só as centrais soam.
  - **Width** (`osc1Width`…): 0 a 100 %. Abertura estéreo: as cópias vão da esquerda (as mais graves) para a direita (as mais agudas), com pan de potência constante. Com 0 %, tudo fica no centro.
  - O volume do conjunto é normalizado pela potência (soma dos quadrados dos ganhos = 1): mudar o número de cópias ou o blend não muda muito o volume percebido.
  - Com 2 ou mais cópias, cada uma começa numa fase aleatória a cada nota, para o ataque não soar como um flanger.
  - A voz passa a ser estéreo: o filtro processa os dois canais.
  - Interface: knobs Unison, Detune, Blend e Width em cada grupo de oscilador, na aba Oscillators.

- RF13 (v0.7): os osciladores leem **wavetables** em vez de calcular a onda com PolyBLEP (etapa A do estudo de wavetable).
  - Cada forma de onda (seno, serra, quadrada, triângulo) é uma tabela de 2048 amostras com um ciclo, montada por soma de harmônicos no carregamento do plugin (fora da thread de áudio).
  - **Mipmaps:** 21 versões de cada tabela, duas por oitava, com 1023 harmônicos na primeira e 1 na última. O oscilador escolhe a versão com mais harmônicos que ainda fiquem abaixo de Nyquist para a frequência atual (pelo incremento de fase, então vale em qualquer taxa de amostragem e qualquer EDO).
  - Leitura com interpolação linear entre amostras vizinhas.
  - Formas e fases iguais às do PolyBLEP (serra subindo de −1 a +1, quadrada começando em +1, triângulo começando em 0 e descendo). Por serem limitadas em banda, a serra e a quadrada têm um pequeno sobressinal perto do salto (fenômeno de Gibbs, até ~18 %); numa nota muito aguda, a quadrada vira uma senoide de pico 4/π ≈ 1,27.
  - Parâmetros, IDs e interface não mudam.

- RF14 (v0.7): várias modulações no mesmo knob, cada uma controlável separadamente.
  - Cada fonte de modulação tem uma cor própria, nos tons da paleta Têmpera, usada nas alças do cabeçalho e nos anéis.
  - Um knob modulado mostra **um anel por slot**, concêntricos, na ordem dos slots (o slot de número menor fica por dentro), cada um na cor da sua fonte e com o alcance só daquele slot. Com mais de um anel, o knob encolhe um pouco para abrir espaço.
  - Arrastar um anel ajusta a quantidade do slot daquele anel (antes: sempre o primeiro slot do destino).
  - Clique direito no knob (ou num anel) abre um menu com cada slot ligado a ele ("LFO 1: +35 %"), com as ações **Invert**, **Set amount to 0 %** e **Remove**.

- RF15 (v0.8): formas desenhadas à mão nos LFOs e nos osciladores (etapa B do estudo de wavetable), com o mesmo editor de pontos.
  - Nova opção **Custom** no fim da lista de formas de cada LFO (`lfoNShape`) e de cada oscilador (`wave`, `osc2Wave`, `osc3Wave`). As formas prontas continuam exatas e automatizáveis; com Custom, toca a forma desenhada.
  - **Editor de pontos:** até 32 pontos ligados por segmentos. O primeiro ponto fica em x = 0 e o último em x = 1 (só a altura muda). Duplo clique num espaço vazio cria um ponto; duplo clique ou clique direito num ponto o apaga; arrastar move. Cada segmento tem uma alça no meio: arrastar para cima ou para baixo curva o segmento, duplo clique volta a reta.
  - **Grade:** Off, 1/4, 1/8 ou 1/16, com **Snap** opcional (x na grade, altura em passos de 1/4). Grade e Snap são da interface, não do som.
  - **Formas prontas como ponto de partida:** com uma forma pronta escolhida, o editor mostra os pontos dela (o seno é aproximado por curvas). A primeira edição muda a forma para Custom, partindo desses pontos. Sample & Hold não é editável.
  - Os pontos são salvos no estado do plugin (junto com o set), não são automatizáveis.
  - **Áudio:** a forma do LFO vira uma tabela de 2048 amostras; a do oscilador vira um wavetable com os mesmos 21 mipmaps do RF13 (montados com FFT, sem DC). As tabelas são montadas na thread da interface e entregues ao áudio por um buffer triplo com índices atômicos, sem lock e sem alocação no áudio.
  - Interface: na aba LFOs, cada LFO mostra o editor acima dos controles, com um ponto na fase atual. Na aba Oscillators, clicar no gráfico da onda abre o editor numa janela flutuante.
  - Compatibilidade: acrescentar Custom no fim das listas não muda os valores salvos no estado, mas muda a escala normalizada das escolhas, então automações gravadas das formas mudariam. Aceitável: a alpha ainda não foi distribuída.
- RF16 (v0.9): gráfico dos steps da EDO na aba Tuning, abaixo dos knobs de afinação, num painel **Steps** com duas sub-abas: **Circle** e **Line**. Só visualização (a edição é o RF17).
  - **Escala:** o espaçamento é em cents (escala logarítmica, a que o ouvido percebe). O step k de uma EDO N fica em k × 1200 / N cents; numa EDO, as linhas ficam equidistantes. Os Hz aparecem só como texto (régua da Line e lista de notas).
  - **Circle:** uma volta = uma oitava (1200 ¢). Uma linha do centro até a borda por step, começando no step 0 (Ref Note) no topo, em sentido horário; ângulo = cents × 360 / 1200. O step 0 tem cor própria (bronze). No centro, o número da EDO; o tamanho do step em cents fica no cabeçalho do painel ("19 EDO · 63.2 ¢/step"), junto dos botões Circle/Line e 12-TET. Os números dos steps ficam do lado de fora da borda; com mais de 24 steps, só os pares (com mais de 48, a cada 4), e os steps acesos sempre têm número.
  - **Line:** a mesma oitava, da esquerda (0 ¢) para a direita (1200 ¢), com linhas verticais. Régua em cents embaixo (marcas a cada 100 ¢) e em Hz em cima (de Base Freq até 2 × Base Freq). A última linha (0′) é o step 0 da oitava seguinte.
  - **Notas tocadas:** cada nota segurada acende a linha do seu step: step = (nota − `refNote`) mod N (resto sempre positivo, também para notas abaixo da Ref Note) e oitava = ⌊(nota − `refNote`) / N⌋. A linha acesa fica mais grossa, na cor primária, com brilho e um ponto na borda; ao soltar a nota, o brilho se apaga em cerca de 300 ms. Notas em oitavas diferentes acendem a mesma linha. Ao lado do gráfico, uma lista das notas seguradas: "MIDI 75 → step 6, oct +0 · 379 ¢ · 547,7 Hz".
  - **Pitch bend e glide:** o ponto da nota acompanha a altura que está soando (nota + glide + pitch bend), andando entre as linhas; a linha do step da nota tocada continua acesa. Modulação de pitch da matriz (LFO, envelope) e a transposição dos osciladores não entram, para o gráfico não tremer com o vibrato.
  - **Referência 12-TET:** botão **12-TET** no painel Steps. Ligado, mostra as 12 posições do temperamento comum (tracinhos fora da borda no Circle; marcas a cada 100 ¢ destacadas na Line). Desligado por padrão. É uma opção da interface, salva no estado do plugin, não é parâmetro do host.
  - **Áudio → interface:** para cada uma das 8 vozes, a thread de áudio publica em atômicos se a voz está tocando, a nota e a altura atual em steps (fracionária, com glide e bend), uma vez por bloco. A interface lê num timer de 30 Hz (como o ponto do envelope e a fase dos LFOs). Custo de CPU desprezível: 8 escritas atômicas por bloco.
  - A sub-aba escolhida e o botão 12-TET não mudam o som.
- RF17 (v0.10): afinação desenhada no gráfico do RF16 (arrastar as linhas dos steps).
  - Nas duas visões, cada linha (menos a do step 0) tem uma alça que o usuário arrasta. A linha fica sempre entre as vizinhas (não cruza) e mostra, enquanto arrasta, a posição em cents, a posição igual da EDO e o espaço até as vizinhas. O step 0 fica fixo em 0 ¢ e a oitava continua 1200 ¢.
  - A afinação desenhada é uma tabela com a posição em cents de cada step, guardada junto com o N para o qual foi feita. Salva no estado do plugin, não automatizável. No centro do Circle, "custom" substitui o tamanho do step.
  - **Reset:** botão **Reset (equal)** volta às linhas equidistantes. Mudar o `edo` por qualquer caminho (knob, automação do Ableton, preset) também descarta a afinação desenhada e volta ao equidistante com o novo número de linhas. Regra simples e sem lock: se o N da tabela for diferente do `edo` atual, vale a afinação igual.
  - **Lock:** com `tuningLock` ligado, as alças somem e o gráfico não aceita arrasto (só visualização), como os knobs de afinação.
  - **Afinação:** o `TuningEngine` passa a ler a tabela: f = baseHz × 2^((oitava × 1200 + cents[step]) / 1200). Alturas fracionárias (glide, pitch bend) interpolam linearmente em cents entre o step e o seguinte. A tabela vai da interface para o áudio pelo buffer triplo do RF15, sem lock e sem alocação no áudio.
  - **Tudo o que é medido em steps segue a tabela** (decidido em 12/11/2026): a transposição em steps de cada oscilador (RF6), o destino Pitch da matriz (RF10) e o pitch bend somam steps à altura da nota antes de consultar a tabela. Ex.: Osc 2 em +4 steps cai exatamente na quarta linha desenhada acima da nota, então o intervalo entre os osciladores pode mudar de nota para nota. A oitava do oscilador continua ×2 e o Fine continua em cents.
  - O ponto das notas no gráfico (RF16) também usa a tabela, então cai em cima das linhas desenhadas.
  - Notas já seguradas mantêm a altura ao mexer numa linha (como no RF3); as próximas notas usam a nova afinação.
  - [a definir] Importar e exportar arquivos Scala (.scl), formato padrão de afinações microtonais.

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
| `MatizaVoice` | `juce::SynthesiserVoice` | Uma voz: oscilador, ADSR e filtro; pede a frequência ao `TuningEngine` no note-on |
| `MatizaSound` | `juce::SynthesiserSound` | Marcador exigido pelo `Synthesiser` (aceita todas as notas e canais) |
| `MatizaSynthesiser` | `juce::Synthesiser` | Guarda do RF4 e modo Mono (RF7): pilha de notas pressionadas e troca de nota na voz 0 |
| `Oscillator` | nenhuma (C++ puro) | Oscilador com PolyBLEP; cada voz tem 3 (RF6) |
| `Glide` | nenhuma (C++ puro) | Rampa linear da altura (em notas/passos) para o portamento |
| `NoteStack` | nenhuma (C++ puro) | Pilha de tamanho fixo das notas pressionadas no modo Mono, sem alocação |

**Estrutura de pastas**

```
matiza/
├── CMakeLists.txt
├── libs/
│   └── JUCE/              (submódulo git, JUCE 8)
├── src/
│   ├── PluginProcessor.h/.cpp
│   ├── PluginEditor.h/.cpp
│   ├── TuningEngine.h/.cpp
│   ├── MatizaVoice.h/.cpp
│   ├── MatizaSound.h
│   └── Parameters.h       (IDs e layout dos parâmetros)
└── tests/
    └── TuningEngineTests.cpp
```

**Trecho principal do CMakeLists.txt**

```cmake
cmake_minimum_required(VERSION 3.22)
project(Matiza VERSION 0.1.0)

add_subdirectory(libs/JUCE)

juce_add_plugin(Matiza
    COMPANY_NAME "Forja Instruments"
    PLUGIN_MANUFACTURER_CODE Forj
    PLUGIN_CODE Mtza
    FORMATS VST3 Standalone
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    BUNDLE_ID com.forjainstruments.matiza
    PRODUCT_NAME "Matiza")

target_sources(Matiza PRIVATE
    src/PluginProcessor.cpp src/PluginEditor.cpp
    src/TuningEngine.cpp src/MatizaVoice.cpp)

target_link_libraries(Matiza PRIVATE
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
- [ ] Criar o repositório `matiza` e adicionar o JUCE como submódulo: `git submodule add https://github.com/juce-framework/JUCE libs/JUCE`
- [ ] Baixar o **pluginval** (Tracktion) para validar o plugin
- [ ] Compilar o **AudioPluginHost** do JUCE (em `libs/JUCE/extras`) para testes rápidos fora do Ableton
- [ ] No Ableton: Preferências → Plug-Ins → ativar "Use VST3 Plug-in System Folders"

**Comandos de build**

```
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Debug
```

O gerador deve corresponder à versão do Visual Studio instalada. O arquivo `Matiza.vst3` sai em `build/Matiza_artefacts/Debug/VST3/`. Com `COPY_PLUGIN_AFTER_BUILD TRUE` no `juce_add_plugin`, o JUCE copia o plugin para `C:\Program Files\Common Files\VST3`, o que exige rodar o build como administrador. A alternativa é apontar uma pasta personalizada de VST3 no Ableton.

## Plano de implementação

São seis etapas curtas, e cada uma termina com algo que você pode tocar ou testar. A ordem prioriza provar a afinação o quanto antes (etapa 3).

| Etapa | Entrega | Critério de aceite |
| --- | --- | --- |
| 1. Esqueleto | Projeto CMake + JUCE compilando um plugin VST3 vazio | O Ableton lista e carrega o Matiza numa track MIDI sem crash |
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
| 13. Unison | Unison, detune, blend e largura estéreo em cada oscilador (RF12) | Serra com 7 cópias, detune 25 ct e width 100 % soa larga no estéreo; Unison 1 soa igual à versão anterior; 8 vozes × 3 osciladores × 8 cópias rodam no Live sem estalar |
| 14. Motor wavetable | Osciladores leem tabelas com mipmaps (RF13) | Mesmo som nas notas graves; notas agudas sem "notas fantasmas"; CPU menor com unison no máximo |
| 15. Várias modulações no knob | Cores por fonte, um anel por slot e menu do clique direito (RF14) | LFO 1 e Velocity no Cutoff mostram dois anéis de cores diferentes; arrastar cada um muda só o seu slot; o menu inverte e remove |
| 16. Formas desenhadas | Editor de pontos no LFO e nos osciladores, opção Custom (RF15) | Desenhar uma rampa no LFO 1 ligado ao Cutoff muda o filtro como desenhado; desenhar uma onda no Osc 1 muda o timbre sem estalo nem aliasing; reabrir o set mantém as formas |
| 17. Gráfico dos steps | Painel Steps com Circle e Line, notas acesas, ponto com bend/glide e botão 12-TET (RF16) | Com 19 EDO, as 19 linhas aparecem equidistantes nas duas visões; tocar um acorde acende as linhas dos seus steps; pitch bend e glide movem o ponto entre as linhas; mudar o EDO redesenha na hora |
| 18. Afinação desenhada | Arrastar as linhas, Reset, Lock e `TuningEngine` com tabela (RF17) | Arrastar a linha do step 4 de 7 EDO para 702 ¢ e tocar esse step mede ~660 Hz no Tuner do Live (Base 440); mudar o EDO volta ao equidistante; com Lock as linhas não mexem; reabrir o set mantém o desenho |

Uma sugestão de ritmo: as etapas 1 e 2 cabem numa primeira noite, e a etapa 3 é o marco que prova a ideia.

## Testes e validação

A afinação é verificada por código e de ouvido; a estabilidade é verificada pelo pluginval.

**Testes unitários do TuningEngine** (Catch2 ou `juce::UnitTest`)

- Com 12 EDO, 440 Hz e referência 69, `noteToHz` bate com `juce::MidiMessage::getMidiNoteInHertz` para as notas 0 a 127 (tolerância de 0,01 Hz).
- Subir N passos com N divisões dobra a frequência, para N = 12, 19, 24, 31 e 52.
- A nota de referência sempre devolve exatamente `baseHz`.
- Pitch bend máximo com `bendSteps = 2` equivale a tocar a nota dois passos acima.

**Testes manuais no Ableton**

- Colocar o Tuner do Live depois do Matiza e conferir a tabela de exemplos do modelo de afinação.
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

- Wavetable nos osciladores: estudo de viabilidade concluído em 10/11/2026 (`.kanban/docs/Estudo — Wavetable nos osciladores.md`). Recomendação: trocar o motor PolyBLEP por wavetable com mipmaps, depois onda desenhada e importação de .wav. Plano aguardando decisão.

**Questões em aberto**

- [x] Nome definitivo do produto e códigos de fabricante e plugin → **Matiza**, fabricante **Forja Instruments**, códigos `Forj` e `Mtza` (10/11/2026).
- [ ] Faixa máxima de EDO: 72 é suficiente, ou vale ir até 128 ou mais?
- [ ] Unidade do pitch bend: passos da EDO (proposto) ou cents?
- [x] Qual biblioteca de testes usar: Catch2 ou `juce::UnitTest`? → **Catch2 v3** (07/11/2026), testa o `TuningEngine` sem JUCE.
- [ ] Licença do código: repositório privado (closed source) desde já?
