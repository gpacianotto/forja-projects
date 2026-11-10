---

kanban-plugin: board

---

## Backlog

- [ ] # Interface: controlar várias modulações no mesmo knob (UX)
	
	Hoje, com mais de um slot no mesmo knob, o anel laranja mostra o alcance somado e arrastar ajusta só o primeiro slot.
	
	Melhorar para controlar cada modulação separadamente. Opções a avaliar:
	- Um anel por slot, em raios diferentes (concêntricos), cada um com a cor da sua fonte.
	- Seletor no knob (ex.: pequenos pontos/abas por fonte) que escolhe qual slot o anel ajusta.
	- Clique direito no knob abre um menu com a quantidade de cada slot ligado a ele.
	
	Dar cores diferentes às fontes (alças e anéis) ajuda em qualquer opção.
	
	[a definir] qual opção (ou combinação) seguir.
	
	#MicroSynth
- [ ] # Interface: gráfico do LFO com edição manual da onda
	
	Mostrar a forma de cada LFO num gráfico de linhas, com ponto da fase atual.
	
	Permitir desenhar a onda à mão, como no Serum: pontos que o usuário adiciona, arrasta e apaga, ligados por retas ou curvas (arrastar o meio do segmento curva a linha).
	
	As formas prontas (Sine, Triangle, Saw, Square, S&H) continuam como presets: escolher uma carrega os pontos dela, que podem ser editados depois.
	
	Atenção: a forma desenhada não cabe num parâmetro do host. Ela é salva no estado do plugin (junto com o set), mas não é automatizável. Manter o parâmetro `lfoNShape` para compatibilidade.
	
	[a definir] número máximo de pontos, tipos de curva, grade/snap.
	
	#MicroSynth
- [ ] # Feat: editar a forma de onda dos osciladores (estudo de viabilidade)
	
	Permitir que o usuário desenhe ou edite a onda de cada oscilador, como no editor de wavetable do Serum.
	
	Primeiro passo é um **estudo**: é possível e viável? Pontos a investigar:
	- Técnica: trocar o oscilador PolyBLEP por **wavetable** (tabela com um ciclo da onda), com versões filtradas por faixa de frequência (mipmaps) para evitar aliasing em notas agudas.
	- Custo de CPU e memória com 8 vozes × 3 osciladores (× unison, se o card de unison vier antes).
	- Microtonal: a wavetable funciona em qualquer frequência, então a afinação por EDO não é afetada.
	- Onde salvar a onda desenhada (estado do plugin, como a forma manual do LFO) e importação de arquivos .wav de wavetable.
	
	Resultado esperado do estudo: recomendação (fazer / não fazer / fazer versão simplificada) e, se sim, um plano em etapas no Spec.
	
	#MicroSynth
- [ ] # Feat: unison, detune e blend nos osciladores
	
	Adicionar a cada oscilador, como no Serum:
	- **Unison:** número de cópias da onda por nota (1 a 8, por exemplo).
	- **Detune:** quanto as cópias se desafinam entre si (espalhadas em volta da nota).
	- **Blend:** volume das cópias desafinadas em relação à central.
	
	Junto com isso, espalhar as cópias no estéreo (o synth hoje é mono por voz) para o som ficar largo.
	
	Atenção à CPU: 8 vozes × 3 osciladores × 8 cópias = 192 osciladores. Medir no Live; talvez limitar o unison máximo.
	
	[a definir] número máximo de cópias, curva do detune (em cents ou relativo à EDO), controle de largura estéreo.
	
	#MicroSynth


## Em Andamento



## Em Revisão



## Concluído

- [x] # Interface: Mod Env em aba dentro do grupo Envelope (UX)
	
	Tirar o Mod Env da aba Matrix e colocá-lo no espaço do grupo Envelope (aba Amp & Filter), com abas internas: **Amp Env** | **Mod Env**.
	
	Cada aba interna mostra o gráfico e os 4 knobs do envelope escolhido; os IDs dos parâmetros não mudam.
	
	A aba Matrix fica só com a lista dos 8 slots (sobra espaço para ela).
	
	#MicroSynth
- [x] # Interface: gráfico das ondas dos osciladores
	
	Mostrar em cada grupo de oscilador (aba Oscillators) um pequeno gráfico com um ciclo da forma de onda escolhida.
	
	Atualiza ao trocar a forma de onda; oscilador desligado aparece apagado.
	
	Base visual para os cards de edição de onda e de unison.
	
	#MicroSynth
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