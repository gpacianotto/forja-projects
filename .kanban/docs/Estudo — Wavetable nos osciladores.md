# Estudo — Wavetable nos osciladores

Nov 10, 2026 · @Guilherme · card "Feat: editar a forma de onda dos osciladores (estudo de viabilidade)" · projeto Matiza

## Resumo

**Recomendação: fazer, em três etapas, começando por uma versão simplificada.** Trocar o motor dos osciladores de PolyBLEP para wavetable é viável e melhora as duas coisas que mais importam aqui: gasta **4 a 5 vezes menos CPU** e tem **30 a 40 dB menos aliasing** nas notas agudas. A afinação microtonal não é afetada. O editor de onda desenhada vem na segunda etapa, reaproveitando o editor de pontos do card do LFO.

| Pergunta do card | Resposta curta |
| --- | --- |
| É possível? | Sim. É a técnica do Serum e do Vital: uma tabela com um ciclo da onda, lida em qualquer velocidade. |
| É viável em CPU? | Sim, e fica **mais barato** que hoje: 192 osciladores (8 vozes × 3 × 8 de unison) custam cerca de 1,3 % a 2 % de um núcleo, contra 5 % a 10 % hoje. |
| E o aliasing? | Muito melhor: numa serra em 3520 Hz, −62 dB contra −24 dB do PolyBLEP. |
| E o microtonal? | Sem impacto. A tabela é escolhida pela frequência da nota, não pelo número da nota MIDI, então qualquer EDO funciona. |
| Memória? | 88 KB por forma de onda com mipmaps. Uma tabela de 256 quadros, como as do Serum, ocupa cerca de 22 MB. |
| Onde salvar a onda desenhada? | No estado do plugin (junto com o set), como a forma manual do LFO. Não é automatizável. |

## Como funciona

Um **wavetable** é um array com um ciclo da onda (aqui, 2048 amostras). Para tocar uma nota, o oscilador anda pelo array mais rápido ou mais devagar, conforme a frequência, e interpola entre as amostras vizinhas. Em Java seria como ler um `float[]` com um índice fracionário.

O problema é o aliasing: uma serra tem harmônicos infinitos. Se a tabela guardar todos os harmônicos que cabem em 2048 amostras, numa nota aguda os de cima passam de Nyquist (metade da taxa de amostragem) e voltam espelhados como notas desafinadas.

A solução são os **mipmaps**: várias versões da mesma tabela, uma por oitava, cada uma só com os harmônicos que cabem abaixo de Nyquist naquela oitava. A nota escolhe a versão certa pela frequência. Como as versões são calculadas antes (fora da thread de áudio), tocar custa só uma leitura e uma interpolação por amostra.

## Medições

Feitas com o benchmark em `Matiza/bench/wavetable/` (build Release, MSVC, nesta máquina). O protótipo de wavetable usa tabelas de 2048 amostras, um mipmap por oitava e interpolação linear.

**CPU: 192 osciladores, 1 segundo de áudio a 48 kHz** (porcentagem de um núcleo, só os osciladores)

| Oscilador | CPU |
| --- | --- |
| PolyBLEP, seno (hoje) | 6,9 % |
| PolyBLEP, serra (hoje) | 5,2 % |
| PolyBLEP, quadrada (hoje) | 10,2 % |
| Wavetable, serra | 1,3 % |
| Wavetable, seno | 2,0 % |

O seno atual é caro porque chama `std::sin` a cada amostra; a quadrada faz duas correções PolyBLEP. Na tabela, toda forma de onda custa o mesmo. A variação entre 1,3 % e 2,0 % é ruído da medição.

**Aliasing da serra** (energia fora dos harmônicos em relação ao total; quanto mais negativo, melhor)

| Nota | Serra sem correção | PolyBLEP (hoje) | Wavetable |
| --- | --- | --- | --- |
| 440 Hz | −19,5 dB | −35,3 dB | −48,0 dB |
| 1760 Hz | −13,5 dB | −28,9 dB | −65,0 dB |
| 3520 Hz | −10,3 dB | −24,5 dB | −62,1 dB |
| 7040 Hz | −7,7 dB | −23,0 dB | −58,6 dB |

O PolyBLEP já melhora muito em relação à serra sem correção, mas nas notas agudas ainda deixa um aliasing audível. Em 440 Hz, o −48 dB da tabela vem da interpolação linear, não do aliasing; interpolação cúbica melhora isso se fizer falta.

## Custos e riscos

- **Brilho nas notas agudas.** Com um mipmap por oitava, uma nota no meio da oitava perde até uma oitava de harmônicos no topo do espectro (acima de ~12 kHz). Mitigação: dois mipmaps por oitava (o dobro de memória, ainda pequeno).
- **O timbre muda um pouco.** Sets antigos não quebram (os IDs dos parâmetros continuam os mesmos), mas a serra e a quadrada vão soar um pouco mais limpas nas notas agudas. Como a alpha ainda não foi distribuída, isso é aceitável.
- **Trocar a tabela sem travar o áudio.** Ao desenhar ou carregar uma onda, os mipmaps são calculados na thread da interface (com FFT) e entregues ao áudio por um ponteiro atômico. A tabela antiga é liberada fora da thread de áudio. Não pode haver `new`, `delete` nem lock no `processBlock`.
- **Geração das tabelas.** O protótipo soma senoides uma a uma, o que é lento para 256 quadros. Na implementação, usar `juce::dsp::FFT`: transformar o ciclo, zerar os harmônicos acima do limite de cada oitava e voltar.
- **Interface.** O editor de pontos é o maior trabalho e é o mesmo do card do LFO. Fazer os dois juntos evita escrever duas vezes.

## Plano proposto (aguarda sua decisão)

| Etapa | Entrega | Critério de aceite |
| --- | --- | --- |
| A. Motor wavetable | Os 4 osciladores atuais (seno, serra, quadrada, triângulo) passam a ler tabelas com mipmaps. Mesmos parâmetros e IDs. | CPU com 8 vozes × 3 × 8 cópias cai visivelmente no Live; serra aguda sem "notas fantasmas"; testes de afinação continuam passando. |
| B. Onda desenhada | Nova opção **Custom** na forma de onda de cada oscilador, com editor de pontos (o mesmo do LFO) e gráfico. Salva no estado do plugin. | Desenhar uma onda muda o som na hora, sem estalo; fechar e reabrir o set mantém a onda. |
| C. Wavetables com posição | Importar arquivos .wav de wavetable (formato do Serum: quadros de 2048 amostras) e knob **Position** para varrer os quadros, automatizável e destino da matriz (usa uma das vagas reservadas). | Carregar um .wav do Serum e varrer o Position com um LFO soa contínuo. |

A etapa A sozinha já vale a pena: menos CPU (o que dá folga para o unison) e menos aliasing, sem mudar nada na interface. B e C podem esperar a alpha.

## Resultado da etapa A (10/11/2026)

Implementada como RF13. Com 2 mipmaps por oitava e o `nextSample` inline, o oscilador do Matiza mede **1,4 %** de um núcleo para 192 osciladores (o PolyBLEP media 5 % a 10 %), e a serra em 3520 Hz tem **−61 dB** de aliasing (o PolyBLEP tinha −24 dB). Duas lições da implementação: o `std::floor` a cada amostra dobrava o custo, e o `nextSample` em outro `.cpp` (sem inline) dobrava de novo.

## Fontes e referências

- Benchmark: `Matiza/bench/wavetable/WavetableBench.cpp`.
- Técnica de mipmaps: a mesma usada por Serum e Vital (estudar o Vital só como referência: é GPL, não copiar código para um produto fechado).
