# Estudo — Harmônicos alinhados à escala

Nov 15, 2026 · @Guilherme · card "DSP: harmônicos alinhados à escala (estudo de viabilidade)" · projeto Matiza

## Resumo

**Recomendação: fazer, numa versão simplificada (motor híbrido), com um knob Align que começa em 0 %.** É viável em CPU e reduz de verdade a aspereza dos acordes, mas o ganho é desigual entre afinações. Nas EDOs longe do 12-TET (5, 7, 10, 13), a aspereza cai **25 % a 50 %**. Nas EDOs "parecidas" com o 12-TET (19, 31), cai só **cerca de 10 %**. A 19 EDO, que era o exemplo do card, é justamente uma das que menos ganham. O argumento de venda fica mais forte assim: *"7 EDO soa tão consonante quanto o 12-TET"*.

| Pergunta do card | Resposta curta |
| --- | --- |
| É possível? | Sim, com um banco de senoides (síntese aditiva) para os harmônicos alinhados. O wavetable sozinho não consegue, porque harmônicos fora dos inteiros não cabem num ciclo. |
| É viável em CPU? | Sim, no **híbrido**: os 24 primeiros harmônicos no aditivo, o resto no wavetable. São **9 %** de um núcleo no pior caso (192 osciladores), contra 2,8 % do wavetable atual na mesma medição. O aditivo puro com 64 harmônicos custa 15 % a 24 %. |
| Técnica de síntese | Senoides por **recorrência** (um vetor que gira) com **SSE**, 4 harmônicos por instrução. É 5 a 10 vezes mais barato que uma tabela de seno por harmônico. A IFFT com overlap-add ficou descartada (ver "Técnicas avaliadas"). |
| Máximo de harmônicos | Até **24** alinhados no aditivo, ou menos quando a escala não comporta (o "limite de colisão", abaixo). Acima disso, os harmônicos ficam naturais, no wavetable. |
| Limite de unison | Nenhum novo: o pior caso (8 cópias) cabe nos 9 %. |
| Alinhar pela nota tocada ou pela tônica? | **Pela nota tocada**, puxando cada harmônico para a altura mais próxima da afinação do teclado. Isso resolve EDO, afinação desenhada (RF17) e MTS-ESP (RF18) com o mesmo código. |
| Soa melhor? | Pelo modelo de dissonância, sim (tabela abaixo). Ainda falta a prova de ouvido: os .wav de comparação já estão gerados. |

## A ideia, em uma figura

Um som "afinado" (corda, serra, voz) tem harmônicos nos múltiplos inteiros da fundamental: 1×, 2×, 3×, 4×… Quando você toca um acorde, os harmônicos das notas ficam muito perto uns dos outros. Se ficam *quase* juntos, mas não exatamente, batem e soam ásperos.

No 12-TET, a quinta (700 ct) fica a 2 ct da quinta justa (702 ct), então quase não bate. Numa escala como a 7 EDO, os intervalos ficam longe da série harmônica e o acorde soa áspero. A ideia do Sethares é mover os harmônicos do som para cima dos graus da escala. Assim, os harmônicos das notas do acorde caem **exatamente uns sobre os outros** e o batimento some.

Exemplo em 19 EDO (degrau de 63 ct):

| Harmônico | Natural | Alinhado | Desvio |
| --- | --- | --- | --- |
| 3 | 3,000 | 2,988 | −7 ct |
| 5 | 5,000 | 4,979 | −7 ct |
| 7 | 7,000 | 6,914 | −22 ct |
| 9 | 9,000 | 8,925 | −14 ct |
| 11 | 11,000 | 11,109 | +17 ct |
| 13 | 13,000 | 12,855 | −20 ct |

As oitavas (2, 4, 8, 16) não mudam. Na 19 EDO os desvios são pequenos porque ela já tem terças e quintas próximas das justas, e é por isso que ela ganha pouco. Na 7 EDO o desvio chega a ±85 ct (metade do degrau de 171 ct), e o ganho é grande.

## Medições

Feitas com o benchmark `Matiza/bench/aligned-harmonics/` (Release, MSVC, nesta máquina, blocos de 64 amostras).

### Som: aspereza dos acordes

Usei o modelo de dissonância de Plomp-Levelt, na forma usada pelo Sethares. Ele soma a aspereza de cada par de harmônicos próximos. Os números abaixo são para uma serra com 12 harmônicos, a partir do dó central, e são relativos à **tríade maior do 12-TET com harmônicos naturais = 1,00**. Quanto menor, menos áspero.

| Acorde | Natural | Align 50 % | Align 100 % |
| --- | --- | --- | --- |
| 12 EDO maior | 1,00 | 0,94 | 0,84 |
| 12 EDO menor | 1,02 | 0,96 | 0,86 |
| 19 EDO maior | 0,99 | 0,95 | 0,90 |
| 19 EDO menor | 1,00 | 0,96 | 0,91 |
| 31 EDO maior | 0,97 | 0,94 | 0,91 |
| 13 EDO (0-4-8) | 1,10 | 1,00 | 0,77 |
| 10 EDO (0-3-6) | 1,08 | 0,97 | 0,76 |
| 7 EDO (0-2-4) | 1,10 | 1,00 | **0,62** |
| 5 EDO (0-1-3) | 1,13 | 1,06 | **0,53** |

Leituras:
- Com o alinhamento, a 7 EDO e a 5 EDO ficam **menos ásperas que o 12-TET normal**. É o diferencial que dá para vender.
- A 19 EDO e a 31 EDO já soam quase como o 12-TET sem alinhamento, e o ganho é pequeno.
- O próprio 12-TET também melhora (1,00 → 0,84), porque as terças maiores do 12-TET são 14 ct mais altas que as justas e o alinhamento elimina esse batimento. É uma opção curiosa até para quem não usa microtonal.
- O modelo mede só a aspereza (batimento), não o quanto o timbre fica "estranho". Com Align alto, uma nota sozinha soa um pouco metálica, como um sino. Por isso o knob é necessário.

### Limite de colisão

Os harmônicos altos ficam cada vez mais juntos (o 30º e o 31º estão a 57 ct). Quando a distância entre dois vizinhos fica menor que o degrau da escala, os dois caem no mesmo grau. Daí em diante, alinhar não faz sentido.

| EDO | 5 | 7 | 10 | 12 | 13 | 19 | 22 | 24 | 31 | 53 | 72 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Harmônicos alinháveis | 10 | 14 | 18 | 22 | 24 | 34 | 37 | 41 | 53 | 86 | 118 |

Na prática, o limite útil é **24 harmônicos** ou o limite de colisão, o que for menor. Acima do 24º, os harmônicos de uma serra têm menos de 1/24 do volume da fundamental (−28 dB) e pesam pouco na aspereza.

### CPU: 192 osciladores (8 vozes × 3 osc × 8 de unison), % de um núcleo

| Motor | 16 harm. | 32 harm. | 64 harm. |
| --- | --- | --- | --- |
| Wavetable atual (RF13), referência | 2,8 % | 2,8 % | 2,8 % |
| (a) Tabela de seno por harmônico | 28 % | 67 % | 156 % |
| (b) Recorrência, escalar | 13 % | 27 % | 53 % |
| (c) Recorrência, SSE | 5,0 % | 8,4 % | 15,5 % |
| (d) Recorrência, SSE, com a frequência mudando a cada bloco | 6,3 % | 11,3 % | 23,6 % |

| Híbrido (aditivo SSE + wavetable) | 16 no aditivo | 24 no aditivo | 32 no aditivo |
| --- | --- | --- | --- |
| CPU | 7,6 % | **9,3 %** | 10,8 % |

Observações:
- O wavetable mediu 2,8 % aqui, contra 1,4 % no estudo anterior, porque o laço é diferente (blocos, frequências). O que vale é a comparação **dentro** da mesma tabela.
- **Recorrência:** cada harmônico é um vetor (cos, sen) que gira um ângulo fixo a cada amostra (4 multiplicações e 2 somas, sem chamar `sin`). Em float, o vetor perde ou ganha tamanho aos poucos; uma correção por bloco segura isso. Depois de 10 s, o erro máximo foi de −79 dB, inaudível.
- **SSE:** instruções que fazem a mesma conta em 4 floats de uma vez (todo processador x64 tem). Em Java, é o que a Vector API faria. AVX (8 de uma vez) daria mais ~2×, mas nem todo processador tem e exigiria duas versões do código. Fica como otimização futura.
- **Frequência mudando** (pitch bend, vibrato do LFO, glide): a cada bloco, é preciso recalcular o giro de cada harmônico com `cos` e `sin`. No pior caso isso soma 30 % a 50 %. Uma aproximação polinomial de seno (vetorizada) reduz essa parte, se fizer falta.
- Esses são números do pior caso. Com 1 cópia de unison, divida por 8.

## Técnicas avaliadas

| Técnica | Veredito |
| --- | --- |
| Tabela de seno por harmônico | Descartada: custa um oscilador wavetable inteiro **por harmônico** (156 % com 64). |
| Recorrência com SSE (banco de senoides) | **Escolhida.** Barata, sem latência, cada voz muda de frequência a cada bloco sem artefato. |
| IFFT com overlap-add (sintetizar o espectro por blocos) | Descartada por agora. Fica barata com muitos harmônicos, mas acrescenta latência ou artefatos de quadro, complica o pitch bend e a modulação por voz (cada voz precisa da própria FFT) e não compensa com só 24 harmônicos. |
| Híbrido (aditivo embaixo, wavetable em cima) | **Escolhida como arquitetura.** O aditivo fica onde o alinhamento importa; o wavetable mantém o brilho e o anti-aliasing dos mipmaps. |

## Como seria no Matiza

1. **Receita do quadro (fora do áudio).** A amplitude e a fase dos 24 primeiros harmônicos de cada forma:
   - Seno, serra, quadrada e triângulo têm fórmula.
   - A forma desenhada (RF15) passa pela FFT que já monta os mipmaps.
2. **Tabela "do resto" (fora do áudio).** Os mipmaps da mesma forma **sem** os 24 primeiros harmônicos. É a mesma FFT do RF15, zerando também os de baixo.
3. **Alinhamento (fora do áudio, a cada nota).** No note-on, para cada harmônico h da nota tocada:
   - Calcular a frequência natural (h × fundamental).
   - Encontrar a altura mais próxima na afinação do teclado. O `TuningEngine` já sabe responder isso para EDO, afinação desenhada e MTS-ESP; com o master, são as 128 frequências dele, sem precisar deduzir a escala.
   - Guardar a razão alinhada.

   São 24 buscas por nota, feitas no note-on, sem alocação. Com Align entre 0 e 100 %, a razão é interpolada em cents.
4. **Síntese (tempo real, na voz).** Cada cópia de unison vira wavetable "do resto" mais o banco SSE de até 24 senoides. O pitch bend, o Fine e o detune do unison multiplicam todas as frequências juntas: as razões ficam fixas, então o alinhamento sobrevive ao bend. Harmônicos que passariam de Nyquist são desligados.
5. **Align = 0 %:** a voz usa só o wavetable completo de hoje, exatamente como agora, sem custo extra. Isso cumpre o primeiro critério de aceite do card.

**Por que pela nota tocada e não pela tônica.** O objetivo é que os harmônicos das notas do acorde caiam uns sobre os outros. Se cada harmônico vai para uma altura **que existe no teclado**, dois harmônicos que caem na mesma altura ficam exatamente iguais, qualquer que seja a nota tocada. Isso vale também em escalas não equidistantes (RF17) e na escala do master, sem regra especial.

**Etapa C (Position).** Ainda não existe. Quando vier, a interpolação entre quadros passa a interpolar receitas (24 amplitudes) e tabelas do resto. Isso não bloqueia este trabalho: o alinhamento funciona desde já com as 4 formas e a desenhada.

## Custos e riscos

- **CPU:** cerca de 3× o oscilador de hoje com Align ligado. Pelo benchmark, cabe com folga, mas é preciso confirmar no Live com unison no máximo (terceiro critério de aceite).
- **Timbre metálico com Align alto,** principalmente em 5 e 7 EDO, onde os harmônicos andam até meio degrau. É inerente à ideia (os timbres do Sethares soam assim); o knob deixa o usuário dosar.
- **Fase entre o aditivo e o resto.** As duas partes precisam começar com a fase coerente, ou a forma de onda muda no ataque. Com Align = 0 isso não aparece (o caminho é só o wavetable).
- **Afinação trocada com nota presa:** as razões alinhadas são calculadas no note-on. Se a afinação mudar com a nota presa, a nota mantém as razões antigas até soltar. É o mesmo comportamento que já aceitamos para a afinação.
- **Prova de ouvido pendente.** O modelo de dissonância é bem aceito na literatura, mas o critério de aceite é o teste cego com os produtores parceiros.

## Para ouvir agora

O benchmark gera 7 acordes de 4 s (serra aditiva, 220 Hz) em `Matiza/bench/aligned-harmonics/build/audio/`:
- `01-12edo-maior-natural.wav`: referência.
- `02` e `03`: 19 EDO natural × alinhado. A diferença é sutil.
- `04` e `05`: 10 EDO natural × alinhado.
- `06` e `07`: 7 EDO natural × alinhado. É onde a diferença deve ser mais clara.

## Plano proposto (aguarda sua decisão)

| Etapa | Entrega | Critério de aceite |
| --- | --- | --- |
| A. Matemática pura | `AlignedSpectrum.h`: alinhamento pela afinação do teclado, limite de colisão, receitas das 4 formas e da desenhada, interpolação do Align. Testes unitários. | Testes passando; alinhamento em 12, 19 e 7 EDO, na afinação desenhada e com o master igual às tabelas deste estudo. |
| B. Motor híbrido | Banco SSE de até 24 senoides na voz, mipmaps "do resto", parâmetro `align` (0–100 %, padrão 0) e destino na matriz de modulação. | Align = 0 soa idêntico ao atual; CPU medida no plugin; pluginval nível 5. |
| C. Interface | Knob Align e um gráfico com os harmônicos sobre as linhas da escala (reaproveita o ScaleLines do RF19). | Mexer no Align mostra os harmônicos andando até os graus. |
| D. Teste de escuta | Acordes em 7, 10, 12 e 19 EDO com e sem Align, em teste cego com os produtores parceiros. | Preferência pelo alinhado nas EDOs distantes do 12-TET. |

Decisões em aberto:
1. Fazer agora ou depois da alpha?
2. Align **global** (um knob, mais simples) ou **por oscilador** (mais flexível, 3 knobs)?

**Decisão (15/11/2026):** fazer agora, com o Align **global** (parâmetro `harmonicAlign`). Plano no Spec como RF21, etapas 22 a 25. A etapa A (matemática pura, `src/AlignedSpectrum.h`, 12 testes) foi concluída no mesmo dia.

## Fontes e referências

- Benchmark: `Matiza/bench/aligned-harmonics/AlignedHarmonicsBench.cpp`.
- William A. Sethares, *Tuning, Timbre, Spectrum, Scale* (Springer). A curva de dissonância e a ideia de casar espectro e escala vêm daí. O modelo usado aqui é a parametrização de Plomp-Levelt que o livro apresenta.
- Estudo anterior: [[Estudo — Wavetable nos osciladores]].
