---

kanban-plugin: board

---

## Backlog

- [ ] # Feat: matriz de modulação
	
	Permitir ligar fontes de modulação a parâmetros de destino, como a aba Matrix do Serum. Nome técnico: **matriz de modulação** (modulation matrix).
	
	Cada linha (slot) da matriz tem: fonte → destino → quantidade (amount, positiva ou negativa).
	
	Fontes iniciais (antes do LFO): velocity, mod wheel, aftertouch e um segundo envelope (envelope de modulação).
	Destinos iniciais: cutoff, resonance, pitch e volume de cada oscilador, fine, glide.
	
	[a definir] número de slots (sugestão: 8 fixos, com IDs estáveis), lista final de destinos e se haverá arrastar-e-soltar na interface.
	
	Base para o card do LFO.
	
	#MicroSynth
- [ ] # Feat: LFO
	
	Adicionar LFO como fonte de modulação da matriz.
	
	Implementar **depois** do card "Feat: matriz de modulação" (o LFO só tem efeito ligado a um destino).
	
	Proposta: formas de onda (seno, triângulo, serra, quadrada, sample & hold), rate em Hz ou sincronizado ao tempo do Ableton, e modo por nota (reinicia a cada nota) ou livre.
	
	[a definir] quantidade de LFOs (1 ou 2).
	
	#MicroSynth


## Em Andamento



## Em Revisão



## Concluído

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