# Plano de produto — Kick microtonal (Forja Instruments)

Nov 12, 2026 · @Guilherme

> Nome e branding ainda não definidos: decidir quando o escopo fechar. Neste documento o produto é chamado de "Kick".

## Resumo

Segundo produto da Forja Instruments: um **sintetizador de kick que segue a afinação microtonal da música**. O problema: quem compõe em EDOs (24, 19, 31…) não tem como afinar o kick com o resto da track, porque os sintetizadores de bateria só conhecem as 12 notas.

| Ideia | Veredito | Por quê |
| --- | --- | --- |
| Sintetizador de kick microtonal | **Fazer** (depois de validar com produtores) | Afinar o kick é prática padrão na música eletrônica, e não há kick synth microtonal no mercado |
| Bateria completa microtonal | **Ainda não** | Só elementos com altura definida (kick, toms, percussões) ganham com afinação; caixa e hi-hat quase nunca são afinados |
| Centralizador de afinação próprio | **Não fazer: adotar o MTS-ESP** | O padrão já existe, é aberto, gratuito para o usuário e suportado por dezenas de plugins |

## 1. O problema e o espaço no mercado

- Afinar o kick com o tom da música é regra na música eletrônica. Resumo comum em fóruns: hi-hats geralmente não, caixas geralmente não, percussões e toms sim, kick sempre.
- No Ableton Live 12, os sistemas de afinação (Tuning Systems) funcionam com todos os instrumentos nativos, **menos os Drum Synths**. O próprio Live deixa o kick de fora da afinação microtonal.
- Na lista de plugins microtonais da Xen Wiki (referência da comunidade), o único item de percussão é o Chromaphone 3 (percussão por modelagem física, afinação parcial por arquivo .scl). **Nenhum kick synth ou bateria eletrônica.**

**Concorrência real:** não é outro kick synth, é o produtor montando o kick num synth genérico que aceita microtonal (ex.: Serum com MTS-ESP) ou usando samples. O Kick precisa ser claramente melhor nisso.

## 2. Escopo inicial (proposta)

- **Afinação igual à do Matiza:** EDO, frequência base e nota de referência; a fundamental do kick escolhida como **passo da escala** (e oitava), não como nota de piano.
- **Seguir MTS-ESP** (ver seção 4): com um master na sessão, o Kick segue a afinação do projeto sem configurar nada.
- **Visual do passo:** mostrar em qual passo da EDO a cauda (fundamental) do kick está soando, reaproveitando o gráfico de steps do Matiza.
- **Som de kick:** oscilador com envelope de pitch (o "clique" que desce até a fundamental), envelope de volume, camada de ataque/transiente, saturação. [a definir] lista final de controles.
- MIDI: tocar notas muda o passo (para linhas de kick melódicas, estilo 808); uma nota fixa toca o passo escolhido.
- [a definir] presets, formatos (VST3 + Standalone no Windows, como o Matiza), preço.

**Reaproveitamento do Matiza:** `TuningEngine`, gráfico de steps, licenciamento (Paddle + servidor de licenças), paleta e base da interface.

## 3. Bateria completa: caminho possível, não agora

- Ao lado do kick, alguns produtores também afinam toms no tom da música; bem menos gente afina a caixa, tratada como de altura indefinida.
- Palmas, caixas e hi-hats não têm uma fundamental tão clara quanto kicks e toms (ajustar o pitch deles ajuda na mixagem, mas não precisa seguir a escala).
- **Evolução sugerida:** depois do kick, se houver demanda, acrescentar **vozes tonais** (toms, percussões, "808 tocável") em vez de uma bateria completa.

## 4. Centralizador de afinação: usar o MTS-ESP

O "centralizador" pensado (configurar a afinação uma vez e todos os plugins seguirem) já existe: é o **MTS-ESP**, da ODDSound.

- Um plugin "master" define a afinação; os plugins "clientes" seguem automaticamente, sem configuração.
- O master **MTS-ESP Mini é gratuito** (carrega .scl, .kbm e .tun). Há masters pagos (MTS-ESP Suite, Entonal Studio, Infinitone DMT) e gratuitos (Surge XT).
- Para o desenvolvedor: biblioteca aberta; basta incluir `libMTSClient.h` e `libMTSClient.cpp` no build. Segundo a ODDSound, a integração costuma levar menos de meia hora.
- Já suportam: u-he, Xfer Records (Serum), Audio Damage, Audiorealism, TAL e cerca de 60 instrumentos na lista da Xen Wiki.
- O Microtuner (dispositivo Max for Live do Ableton) sincroniza a afinação com plugins clientes do MTS-ESP.
- Cuidado: todos os plugins precisam rodar no mesmo processo. DAWs que isolam plugins (Bitwig, FL Studio) precisam de configuração; o Ableton roda tudo no mesmo processo.

**Por que adotar em vez de criar:** um centralizador da Forja só funcionaria com Matiza e Kick. Com o MTS-ESP, os dois seguem a mesma afinação do Serum, dos synths da u-he e de dezenas de outros, o que vira argumento de venda. Um master próprio só faria sentido no futuro com diferencial de experiência (focado em EDO, com o gráfico de steps), não de sincronização.

## 5. Próximos passos

1. **MTS-ESP no Matiza** (card próprio): seletor "afinação interna" × "seguir MTS-ESP". Pouco trabalho, vem antes do Kick e já entra na landing page do Matiza.
2. **Validar a demanda do Kick** antes de construir (cerca de uma semana): perguntar aos dois produtores parceiros e numa comunidade microtonal (r/microtonal, Xenharmonic Alliance):
   - Como vocês afinam o kick hoje em músicas microtonais?
   - Comprariam um kick synth que segue a afinação (interna ou MTS-ESP)?
   - Que outros elementos da bateria vocês afinam?
3. Com a validação positiva: escrever o Spec do Kick, decidir nome e branding (marca Forja) e criar a pasta do projeto.

## Fontes

- Ableton — Tuning Systems FAQ (Drum Synths não seguem as afinações): https://help.ableton.com/hc/en-us/articles/11535414344476
- Ableton — Microtuner (Lead/Follow e MTS-ESP): https://www.ableton.com/en/packs/microtuner/
- Xen Wiki — List of microtonal software plugins: https://en.xen.wiki/w/Infinitone_DMT
- Ableton Forum — Tuning sampled kick drums: https://forum.ableton.com/viewtopic.php?p=1248998
- Gearspace — Pitching drums: https://gearspace.com/threads/pitching-drums.1055835/
- Attack Magazine — How to tune kick, snare and tom drum samples: https://www.attackmagazine.com/technique/tutorials/how-to-tune-kick-snare-tom-drum-samples/2/
- ODDSound — MTS-ESP library (GitHub): https://github.com/ODDSound/MTS-ESP
- KVR — MTS-ESP Mini: https://www.kvraudio.com/product/mts-esp-mini-by-oddsound
- KVR — ODDSound MTS-ESP (anúncio, integração em menos de meia hora): https://www.kvraudio.com/forum/viewtopic.php?p=8068981
- MusicRadar — MTS-ESP Suite (desenvolvedores que suportam): https://www.musicradar.com/news/oddsound-and-aphex-twin-have-created-the-mts-esp-suite-a-microtuning-master-plugin-that-promises-a-new-dimension-in-composition
