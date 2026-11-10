---

kanban-plugin: board

---

## Backlog



## Em Andamento



## Em Revisão



## Concluído

- [x] # Feat: LFO
	
	Adicionar **3 LFOs** como fontes de modulação da matriz (já reservados na lista de fontes: LFO 1, LFO 2, LFO 3).
	
	Implementar **depois** do card "Feat: matriz de modulação" (o LFO só tem efeito ligado a um destino).
	
	Formas: seno, triângulo, serra subindo e descendo, quadrada, sample & hold. Rate em Hz ou sincronizado ao tempo do Ableton (1/32 a 4 compassos, com tercinas e pontuadas). Trigger livre (fase comum, presa ao tempo da música com sync) ou por nota (reinicia a cada nota). Nova aba LFOs.
	
	Ver RF11 no Spec.
	
	#MicroSynth
- [x] # Feat: matriz de modulação
	
	Permitir ligar fontes de modulação a parâmetros de destino, como a aba Matrix do Serum. Nome técnico: **matriz de modulação** (modulation matrix).
	
	**8 slots fixos.** Cada slot: fonte → destino → quantidade (-100 % a +100 % da faixa do destino; no Pitch, 100 % = 24 passos da EDO). Quantidade automatizável; fonte e destino não.
	
	**Fontes** (lista fechada para sempre): None, Velocity, Mod Wheel, Aftertouch, Key Track, Mod Env, LFO 1, LFO 2, LFO 3 + 2 vagas reservadas. Os LFOs ficam sem efeito até o card do LFO.
	
	**Destinos** (lista fechada para sempre): None, Cutoff, Resonance, Pitch (todos os osciladores), Osc 1/2/3 Level, Osc 1/2/3 Pitch, Osc 1/2/3 Fine + 4 vagas reservadas.
	
	**Envelope de modulação (Mod Env):** segundo ADSR com 4 knobs e gráfico com ponto, como o do volume.
	
	**Interface:**
	- Nova aba Matrix com as fontes como alças arrastáveis e a lista dos 8 slots (editar quantidade, trocar fonte/destino, apagar).
	- Arrastar uma fonte até um knob de destino cria a ligação no primeiro slot livre; com os 8 ocupados, aparece um aviso.
	- Anel colorido em volta do knob modulado mostrando até onde a modulação leva o valor.
	- Arrastar o anel ajusta a quantidade direto no knob.
	
	Ver RF10 no Spec.
	
	Base para o card do LFO.
	
	#MicroSynth
- [x] # Interface: gráfico visual do filtro e do envelope
	
	Mostrar na aba Amp & Filter:
	- Filtro: curva de resposta de frequência do passa-baixa (eixo em escala log), que se move com Cutoff e Resonance.
	- Envelope: desenho do ADSR (attack, decay, sustain, release) que muda conforme os knobs.
	
	Os gráficos atualizam em tempo real, inclusive com automação do Ableton.
	
	> Tentar: mostrar a posição atual do envelope da última nota tocada (ponto andando na curva).
	
	Ver RF9 no Spec.
	
	#MicroSynth
- [x] # Feat: Modo Mono (monofônico × polifônico)
	
	Adicionar seletor de modo de vozes: **Poly** (8 vozes, comportamento atual) ou **Mono** (uma nota por vez).
	
	No modo Mono: prioridade para a última nota tocada; ao soltar a nota, volta para a anterior ainda pressionada; legato (troca de nota sem redisparar o envelope) e glide (portamento) ajustável.
	
	Ver RF7 no Spec.
	
	#MicroSynth
- [x] # Feat: Múltiplos Osciladores
	
	3 osciladores fixos por voz. Cada um com: liga/desliga, forma de onda, volume, oitava, transposição em passos da EDO e desafinação fina (cents).
	
	O oscilador 1 usa o parâmetro `wave` atual; osciladores 2 e 3 começam desligados, para sets antigos soarem igual.
	
	Criar área específica na interface para os osciladores.
	
	Ver RF6 no Spec.
	
	#MicroSynth
- [x] # Interface: criar mecanismo de abas no vst
	
	Criar abas no vst para o usuário navegar. Afinação fica numa aba separada.
	
	Na aba de afinação, adicionar um seletor (cadeado) que trava os parâmetros de afinação, para evitar que o usuário mexa sem querer.
	
	Ver RF8 no Spec.
	
	#MicroSynth




%% kanban:settings
```
{"kanban-plugin":"board","list-collapse":[false,false,false,false],"show-checkboxes":true,"new-line-trigger":"enter","hide-card-count":false,"full-list-lane-width":true,"lane-width":350,"new-note-template":"Projetos de Software/Untitled Kanban.md","tag-sort":[{"tag":"#MicroSynth"}],"tag-colors":[{"tagKey":"#MicroSynth","color":"rgba(255, 0, 0, 1)","backgroundColor":"rgba(255, 255, 255, 1)"}],"move-tags":true,"tag-action":"kanban"}
```
%%