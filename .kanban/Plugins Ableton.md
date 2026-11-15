---

kanban-plugin: board

---

## Backlog

- [ ] # Feat: licenciamento (Paddle + servidor de licenças)
	
	Como o Matiza será vendido e liberado depois da compra. Planejado em 11/11/2026; fluxo da Paddle detalhado em 12/11/2026.
	
	**Decidido (11/11/2026)**
	- **Venda pela Paddle**, que é o vendedor legal (merchant of record): cobra, cuida dos impostos de cada país e aceita Pix de compradores no Brasil. Taxa de 5% + US$ 0,50 por venda; repasse mensal (mínimo de US$ 100).
	- **Licença = arquivo assinado pela Forja**, validado offline pelo plugin, **sem limite de máquinas**, com o nome do comprador gravado e visível na tela "Sobre" (inibe compartilhamento casual). Modelo parecido com o da u-he e da FabFilter.
	- **Servidor de licenças próprio e pequeno** (o Paddle Billing não gera chaves): recebe o webhook de compra da Paddle, gera o arquivo assinado com a chave privada da Forja e envia por e-mail; permite baixar de novo. Ideia: AWS Lambda + Secrets Manager (chave privada) + SES (e-mail). Serve para os próximos plugins da marca.
	- As licenças são assinadas pela Forja, não pela Paddle: trocar de plataforma de venda no futuro não invalida licenças vendidas.
	- **Demo:** plugin completo, com um ruído curto periódico (como a u-he). Em vez de popup, uma **faixa fixa no cabeçalho** avisando do modo demo, com "Comprar" e "Ativar licença" (janelas modais em DAW abrem atrás do host ou travam a interface). [a definir] fazer a faixa piscar quando o ruído toca pela primeira vez na sessão.
	- Descartados: Moonbase (5% + €1 sobre o total com imposto, licença presa ao formato deles), Lemon Squeezy (em migração para o Stripe Managed Payments) e cobrança própria com Stripe/Mercado Pago (a Forja viraria vendedora e assumiria os impostos de cada país).
	
	**Fluxo de uma venda (12/11/2026)**
	1. Landing page carrega o **Paddle.js**; o botão "Comprar" abre o **checkout em overlay** da Paddle sobre a própria página (o cliente não sai do site).
	2. A Paddle cobra (cartão, PayPal, Pix no Brasil), calcula o imposto e envia o recibo.
	3. A Paddle manda o webhook **`transaction.completed`** para a Lambda, assinado no cabeçalho `Paddle-Signature` (HMAC-SHA256 sobre `ts:corpo-cru`; não alterar o corpo antes de verificar).
	4. A Lambda confere a assinatura, busca o e-mail do cliente na API da Paddle (a transação concluída não traz o e-mail), gera a licença e envia por e-mail com o link de download. Precisa ser idempotente (a Paddle reenvia webhooks em caso de falha).
	5. O cliente abre o Matiza e clica em "Ativar licença".
	6. Repasse mensal da Paddle.
	
	**Configuração na Paddle**
	- **Sandbox primeiro** (conta separada em sandbox-vendors.paddle.com, com cartões de teste, chaves e webhooks próprios; site aprovado na hora). Depois, a conta de produção.
	- Produto "Matiza" com preço único em USD.
	- **Default payment link** (obrigatório mesmo só com compra única): página do site que carrega o Paddle.js, por exemplo a landing page.
	- Notificações: destino com a URL da Lambda, evento `transaction.completed` (e os de reembolso, abaixo); guardar a chave secreta.
	- Chaves: token do navegador (Paddle.js) e chave de API do servidor.
	
	**Aprovação do domínio (bloqueia a primeira venda real)**
	A Paddle revisa cada domínio de onde sai um checkout. A landing page precisa estar no ar, pública (sem login), com:
	- Descrição clara do produto e **preço visível** (login e preço escondido são as causas mais comuns de reprovação).
	- **Termos e condições** citando a Paddle como vendedora legal (merchant of record), **política de reembolso** e **aviso de privacidade**, acessíveis pela navegação do site; página de contato.
	- Política de reembolso com prazo **entre 14 e 90 dias**, sem "todas as vendas são finais" (pode só remeter à política da Paddle).
	- Nome do vendedor no site igual ao da conta na Paddle.
	- Reservar de 1 a 2 semanas para a aprovação.
	
	**Risco: reembolso + licença offline**
	Com reembolso obrigatório de pelo menos 14 dias e licença que funciona offline, alguém pode comprar, pedir reembolso e continuar usando. Mitigação: o servidor também ouve os eventos de reembolso da Paddle e registra a licença, que entra na lista negra embutida nas próximas versões do plugin. Risco aceito para o lançamento.
	
	**Regras técnicas**
	- Sem licença, o plugin abre normalmente e mantém todos os parâmetros: nunca quebrar um set salvo. Só entra o ruído da demo.
	- Verificação da licença fora da thread de áudio; o resultado vira um `std::atomic<bool>` que o `processBlock` só lê.
	- Nenhuma chamada de rede ao abrir o plugin (o plugin nunca precisa falar com o servidor).
	- Um arquivo de licença numa pasta do usuário, compartilhado pelo VST3 e pelo Standalone.
	- Builds alpha/beta para os produtores parceiros com data de validade (chaves com prazo, como as do `KeyGeneration` do JUCE).
	- Lista negra de licenças vazadas ou reembolsadas embutida nas versões seguintes.
	- [a definir] Algoritmo de assinatura: RSA do JUCE (`RSAKey`/`KeyGeneration`) ou Ed25519 com uma biblioteca pequena.
	
	**Antes de começar**
	- [ ] Confirmar com a Paddle se aceita vendedor pessoa física residente no Brasil e quanto custa o repasse até a conta
	- [ ] RF de licenciamento no Spec (arquivo assinado, modo demo, faixa de aviso, regras da thread de áudio)
	- [x] Conta na Paddle criada (12/11/2026)
	
	**Etapas (na ordem)**
	- [ ] Conta sandbox: produto "Matiza", default payment link e checkout de teste numa página simples com Paddle.js
	- [ ] Formato do arquivo de licença (campos: produto, nome, e-mail, pedido, data, versão máxima, validade opcional) e assinatura
	- [ ] Servidor: webhook da Paddle (verificação da assinatura, idempotência), busca do e-mail na API, geração e envio da licença, reenvio por e-mail, registro de reembolsos para a lista negra; testes com o simulador de webhooks do sandbox
	- [ ] Plugin: leitura e verificação da licença, estado demo/licenciado em atômico, tela "Ativar licença" (colar ou abrir o arquivo)
	- [ ] Plugin: ruído da demo e faixa de aviso no cabeçalho; nome do licenciado na tela "Sobre"
	- [ ] Landing page no domínio da Forja com preço, termos, reembolso, privacidade e contato (card próprio de landing page)
	- [ ] Conta de produção: pedir aprovação do domínio, trocar as chaves e o webhook para produção
	- [ ] Testes: licença válida, adulterada, de outro produto, vencida (alpha), lista negra; set salvo com licença abre igual sem licença (só com ruído); compra completa no sandbox de ponta a ponta
	
	#Matiza
- [ ] # Produto: kick microtonal (segundo produto da Forja)
	
	Sintetizador de kick que segue a afinação microtonal da música (afinação interna igual à do Matiza + MTS-ESP). Nome e branding a definir quando o escopo fechar. Ver [[Plano de produto — Kick microtonal]].
	
	**Decidido (12/11/2026)**
	- Fazer o kick primeiro; bateria completa fica para depois, e só com vozes tonais (toms, percussões, 808 tocável), se houver demanda.
	- Não criar centralizador de afinação próprio: usar o MTS-ESP (card "Feat: seguir a afinação do MTS-ESP").
	
	**Antes de começar**
	- [ ] Validar a demanda: perguntar aos dois produtores parceiros e numa comunidade microtonal (r/microtonal, Xenharmonic Alliance) como afinam o kick hoje e se comprariam um kick synth que segue a afinação
	- [ ] Com validação positiva: Spec do produto (escopo, controles de som, formatos, preço)
	- [ ] Nome e branding (marca Forja Instruments)
	- [ ] Criar a pasta do projeto (skill `novo-plugin`) e registrar na tabela de projetos do `CLAUDE.md`
	
	#Workspace
- [ ] # DSP: harmônicos alinhados à escala (estudo de viabilidade)
	
	Diferencial sonoro do Matiza, decidido em 13/11/2026: o timbre se ajusta à afinação. Com uma escala como 19 EDO, os harmônicos de cada onda são puxados para os graus da escala (ex.: 3º harmônico de 3,00× para 2,95×), e acordes nessa escala soam tão "redondos" quanto no 12-TET (ideia do William Sethares sobre timbre e escala).
	
	**Por que não dá com o wavetable atual (RF13):** um quadro de um ciclo só guarda harmônicos inteiros (2×, 3×, 4×…). Harmônicos realinhados não são inteiros, a onda deixa de se repetir a cada ciclo e não cabe numa tabela de um ciclo.
	
	**Proposta: workflow de wavetable, motor aditivo por baixo**
	1. Análise (fora do áudio): FFT de cada quadro extrai a amplitude de cada harmônico ("receita" do quadro). Reaproveita a FFT que já monta os mipmaps.
	2. Alinhamento (fora do áudio): com os intervalos da afinação ativa (EDO interna, afinação desenhada do RF17 ou tabela do master MTS-ESP do RF18), calcula para onde vai cada harmônico.
	3. Síntese (tempo real, na voz): banco de senoides, uma por harmônico, na frequência alinhada e com a amplitude da receita. A interpolação entre quadros (Position da etapa C) passa a interpolar receitas.
	4. Entrega ao áudio pelo buffer triplo, como nas formas desenhadas (RF15), sem lock e sem alocação.
	
	**Pontos do estudo**
	- CPU: hoje são até 192 osciladores (8 vozes × 3 osc × 8 cópias de unison) a 1,4 % de um núcleo. Aditivo com 64 harmônicos seria até ~12 mil senoides por amostra. Medir num benchmark como o de `Matiza/bench/wavetable/`.
	- [a definir] Técnica de síntese: banco de senoides com SIMD e osciladores por recorrência; IFFT com overlap-add por bloco; ou híbrido (harmônicos de baixo realinhados no aditivo, o resto no wavetable).
	- [a definir] Número máximo de harmônicos e limite de unison com o alinhamento ligado.
	- Com o MTS-ESP, o master entrega 128 frequências, não a estrutura da escala: o builder deriva os intervalos da tabela e refaz as receitas quando ela muda (hash das frequências).
	- Notas fora do pitch class 0: em escalas não equidistantes (RF17), a escala vista de cada nota muda. [a definir] alinhar pela nota tocada ou pela tônica (Ref Note).
	
	**Critérios de aceite (se aprovado)**
	- Alinhamento desligado soa igual ao wavetable atual.
	- Acorde em 19 EDO com alinhamento ligado soa com menos batimento que desligado (comparar em teste cego com os produtores parceiros).
	- Unison no máximo roda no Live sem estalar.
	
	> Tentar: um knob "Align" de 0 a 100 % que interpola entre a posição original e a alinhada dos harmônicos.
	
	Resultado esperado: recomendação (fazer / não fazer / versão simplificada) e plano em etapas no Spec, como no [[Estudo — Wavetable nos osciladores]]. Depende da etapa C (Position) para o workflow completo.
	
	#Matiza
- [ ] # Interface: modulação animada nos knobs e nos gráficos
	
	Hoje o anel da modulação (RF10/RF14) mostra só o alcance, parado. No Serum, o knob mostra o valor modulado se mexendo em tempo real. Os gráficos que dependem de knobs modulados (curva do filtro do RF9, por exemplo) também não reagem à modulação. Os concorrentes têm isso, e ajuda a entender o que a modulação está fazendo.
	
	**Proposta**
	- **Knob:** um marcador (ponto ou trecho mais claro do anel) na posição do valor modulado atual, andando dentro do alcance. Com vários slots no mesmo knob (RF14), um marcador para o valor final (soma dos slots).
	- **Gráfico do filtro:** a curva desenhada com Cutoff e Resonance modulados, não com o valor do knob. [a definir] manter a curva do valor base apagada por trás.
	- [a definir] Outros gráficos afetados por destinos da matriz (ondas dos osciladores com Level, steps com Pitch).
	
	**Como fazer**
	- O valor modulado só existe no áudio (matriz calculada por voz a cada bloco). No fim de cada bloco, o processador publica em atômicos o valor final de cada destino da **última voz tocada** (como o Serum e o ponto do envelope do RF9).
	- A interface lê com o timer que já existe (30 Hz, como no RF16) e só redesenha o que mudou.
	- Sem nota tocando: marcador some (ou fica no valor base). LFO com trigger livre: [a definir] mostrar o movimento mesmo sem nota.
	- Custo: um atômico por destino (hoje 10 + 4 vagas), escrito uma vez por bloco.
	
	**Etapas**
	- [ ] RF no Spec
	- [ ] Processador: valores modulados da última voz em atômicos
	- [ ] Knobs: marcador do valor modulado
	- [ ] Gráfico do filtro com os valores modulados
	- [ ] Build Debug/Release sem warnings, testes, pluginval nível 5
	- [ ] Teste no Ableton: LFO 1 → Cutoff (knob e curva andando juntos), dois slots no mesmo knob, sem nota tocando, CPU da interface
	
	#Matiza
- [ ] # Interface: gráfico do unison (Unison, Detune, Blend, Width)
	
	O unison (RF12) já funciona, mas os 4 knobs não mostram o que estão fazendo. No Serum, ao mexer neles aparece um gráfico que explica o efeito. Pedido em 13/11/2026.
	
	**Proposta**
	- Ao arrastar (ou passar o mouse em) Unison, Detune, Blend ou Width, um gráfico aparece **por cima do gráfico da onda do oscilador** e some ~1 s depois de soltar.
	- Uma barra por cópia sobre um eixo de afinação (cents em volta da nota):
		- **Unison:** número de barras.
		- **Detune:** espalhamento das barras para os lados.
		- **Blend:** altura das barras laterais em relação às centrais.
		- **Width:** [a definir] cor ou deslocamento vertical das barras indo da esquerda para a direita (pan), ou um segundo eixo L/R.
	- Bônus do Matiza: linhas da escala atual (steps da EDO, afinação desenhada ou MTS-ESP) atrás das barras, mostrando o detune em relação aos graus da escala.
	
	**Espaço na tela**
	- Por cima do gráfico da onda, não precisa de área nova. O tamanho mínimo da janela hoje é 840 × 560.
	- Se o gráfico da onda ficar pequeno demais para ler as barras no tamanho mínimo: **avisar o Guilherme antes** de aumentar o tamanho mínimo do plugin (ele topa aumentar, mas quer ser consultado).
	
	**Etapas**
	- [ ] RF no Spec
	- [ ] Cálculo das posições, volumes e pans das cópias reaproveitando `src/Unison.h` (mesma conta do áudio, sem duplicar)
	- [ ] Componente do gráfico e sobreposição no gráfico da onda, com aparecer/sumir
	- [ ] Linhas da escala atrás das barras
	- [ ] Build Debug/Release sem warnings, testes, pluginval nível 5
	- [ ] Teste no Ableton: os 4 knobs nos 3 osciladores, automação do Live (o gráfico não deve aparecer só por automação [a definir]), tamanho mínimo da janela
	
	#Matiza
- [ ] # Feat: landing page v0 (site da Forja Instruments)
	
	Primeira versão do site, com a página do Matiza. Além de vender, é **pré-requisito da Paddle**: a aprovação do domínio bloqueia a primeira venda real (ver card "Feat: licenciamento", seção "Aprovação do domínio"). Pôr uma versão simples no ar cedo, mesmo antes das demos dos produtores.
	
	Estrutura e conteúdo já propostos em [[Plano de produto — Matiza, by Forja Instruments]], seção "Nossa landing page e modelo de venda".
	
	**v0 (mínimo para a revisão da Paddle)**
	- Página do Matiza pública (sem login): descrição clara, screenshot, **preço visível**, botão "Comprar" com checkout em overlay do Paddle.js (sandbox primeiro).
	- Páginas de **termos** (citando a Paddle como merchant of record), **reembolso** (14 a 90 dias), **privacidade** e **contato**, acessíveis pela navegação.
	- Nome do vendedor igual ao da conta na Paddle.
	- Domínio próprio da Forja [a definir].
	- Idioma principal **inglês** (venda em USD), português como opção.
	
	**Depois da v0:** players das 4 demos dos produtores (com a afinação de cada faixa), player "12 EDO × outra afinação", seções de funcionalidades, FAQ, newsletter, download da demo.
	
	**Stack [a definir]** (conversado em 13/11/2026)
	- **Next.js:** primeira ideia do Guilherme; SSG bom para SEO; faz mais sentido se o site ganhar área logada (conta, downloads).
	- **Astro + ilhas React:** HTML estático sem JS por padrão (melhor Core Web Vitals), React só nas partes interativas (players, checkout, demo de afinação); i18n nativo.
	- Hospedagem estática (Cloudflare Pages, Vercel ou Netlify); áudio comprimido no CDN. Servidor de licenças separado (card de licenciamento).
	
	**Etapas**
	- [ ] Decidir a stack e o domínio
	- [ ] Projeto do site no repositório [a definir] (pasta própria no workspace ou repositório separado)
	- [ ] Página do Matiza com preço e checkout do sandbox
	- [ ] Termos, reembolso, privacidade e contato
	- [ ] Deploy no domínio
	- [ ] Pedir aprovação do domínio na Paddle (conta de produção)
	
	#Matiza


## Em Andamento



## Em Revisão



## Concluído

- [x] # Feat: seguir a afinação do MTS-ESP
	
	Fazer o Matiza seguir a afinação de um plugin "master" do MTS-ESP (ODDSound), o padrão aberto que sincroniza a afinação entre plugins. Com isso o Matiza fica afinado junto com Serum, synths da u-he e dezenas de outros, sem configurar nada. Ver [[Plano de produto — Kick microtonal]], seção 4.
	
	**Decidido (12/11/2026)**
	- Adotar o MTS-ESP em vez de criar um centralizador de afinação próprio.
	- Vem antes do segundo produto (Kick), que também nasce com MTS-ESP.
	
	**Proposta**
	- Botão **Follow MTS-ESP** na aba Tuning, ligado por padrão: com master na sessão segue o master; sem master, afinação interna (decidido em 12/11/2026).
	- Com MTS-ESP ativo: knobs de afinação interna desabilitados (como com o Lock); gráfico de steps mostra a afinação vinda do master; notas filtradas pelo master (`MTS_ShouldFilterNote`) não tocam.
	- Indicador "MTS-ESP: conectado / sem master" na aba Tuning.
	- Nota segurada acompanha o master quando ele muda a afinação (decidido em 12/11/2026); a afinação interna continua como no RF3.
	- Afinação desenhada (RF17) fica guardada e inativa enquanto o master está ativo (decidido em 12/11/2026).
	
	**Etapas**
	- [x] RF18 no Spec (12/11/2026)
	- [x] Biblioteca cliente (`libMTSClient.h/.cpp`, licença 0BSD) no projeto e no CMake (baixada e lida em 12/11/2026, em `third_party/MTS-ESP/`; biblioteca estática `MTSClient` no CMake e cliente registrado no processador em 15/11/2026)
	- [x] `TuningEngine` com fonte de afinação: interna ou MTS-ESP (`MTS_NoteToFrequency`), tabela copiada pela thread de áudio a cada bloco (regra do RF18); parâmetro `mtsFollow` (sem botão ainda), notas filtradas e 9 testes (15/11/2026)
	- [x] Interface: botão Follow MTS-ESP, indicador, knobs internos desabilitados e gráfico de steps com a escala do master; 8 testes (15/11/2026)
	- [x] Testes com o MTS-ESP Mini (gratuito) no Ableton: carregar um .scl e conferir no Tuner do Live (motor, "master sai" e interface ok, testado pelo Guilherme em 15/11/2026; arquivos em `Matiza/tests/tunings/`)
	- [x] Build Debug/Release sem warnings, pluginval nível 5 (Debug e Release ok em 15/11/2026)
	
	#Matiza

- [x] # Infra: tirar o token do GitHub da URL do remote
	
	A URL do `origin` tem um token pessoal do GitHub escrito por extenso (fica em `.git/config`, legível por qualquer ferramenta que abra o repositório). O token foi visto numa sessão remota em 11/11/2026: tratar como exposto.
	
	> Não colar o token em nenhum arquivo do repositório (este quadro também vai para o GitHub).
	
	**Etapas**
	- [x] Revogar o token atual em GitHub → Settings → Developer settings → Personal access tokens
	- [x] `git remote set-url origin https://github.com/gpacianotto/forja-projects` (12/11/2026; nenhum outro token em `.git/config`)
	- [x] Login pelo Git Credential Manager (Git for Windows), que guarda a credencial no cofre do Windows; se precisar de token, criar um novo só com acesso a este repositório. O GCM tinha guardado o token revogado: `git credential-manager github logout gpacianotto` e um `git push` para logar de novo pelo navegador
	- [x] Conferir: `git remote -v` sem token e `git push` funcionando (12/11/2026, push autenticado pelo GCM)
	- [x] Conferir se o submódulo `_libs/JUCE` aparece como modificado no Git do Windows (`git status`); daqui do Linux aparece só por causa das quebras de linha CRLF (no Windows está limpo, 12/11/2026)
	
	#Workspace
- [x] # Interface: logos no Matiza (cabeçalho, tela Sobre e ícone)
	
	Levar as logos aprovadas em 12/11/2026 para dentro do plugin. Os arquivos estão em `_brand/` (ver `_brand/README.md`); não redesenhar no código.
	
	**O que entra**
	- **Cabeçalho:** trocar o texto "Matiza" (`PluginEditor::paint`, 22 pt, num espaço de 140 × 36 px, `titleWidth`/`headerHeight`) pelo MTZ (`_brand/matiza/mtz-palha.svg`), na cor `Palette::primary`, alinhado à esquerda e centralizado na altura. Testado: continua legível com 30 px de altura (tamanho mínimo).
	- **Tela "Sobre"** (pendência do card de rebranding): janela simples aberta por um botão discreto no cabeçalho, com a assinatura "MATIZA by Forja Instruments" (`_brand/matiza/matiza-assinatura-clara.svg`), versão (`JucePlugin_VersionString`), créditos (JUCE e a fonte Inter, licença OFL) e um espaço para o nome do licenciado (card de licenciamento). Sem janela modal do sistema: um componente por cima do editor, fechado com clique fora ou Esc.
	- **Ícone do app Standalone:** `ICON_BIG` e `ICON_SMALL` no `juce_add_plugin` com `_brand/matiza/png/matiza-icone-512.png` e `-64.png`.
	
	**Como fazer**
	- Copiar os arquivos usados para `Matiza/assets/` (o plugin não depende de `_brand/` no build) e embutir com `juce_add_binary_data` no CMake.
	- Carregar o SVG com `juce::Drawable::createFromSVG` uma vez (no construtor, nunca no `paint`), trocar a cor com `replaceColour` para seguir a `Palette`, e desenhar com `drawWithin` mantendo a proporção.
	- Conferir que as alças das fontes de modulação no cabeçalho continuam cabendo.
	
	**Etapas**
	- [x] `Matiza/assets/` com os arquivos e `juce_add_binary_data` no CMake (`MatizaAssets`)
	- [x] MTZ no cabeçalho (30 px de altura; clicar abre a tela Sobre)
	- [x] Tela "Sobre" com a assinatura, versão e créditos (`src/AboutOverlay`; texto "License: not activated" reservado para o licenciamento)
	- [x] Ícone do Standalone
	- [x] Build Debug/Release sem warnings, 97 testes, pluginval nível 5 (12/11/2026)
	- [x] Teste no Ableton (aprovado em 12/11/2026): cabeçalho no tamanho padrão, mínimo e máximo; abrir e fechar a tela Sobre; ícone do Standalone no Windows
	
	#Matiza
- [x] # Feat: afinação desenhada no gráfico dos steps
	
	Fase 2 do gráfico dos steps: arrastar as linhas com o mouse para criar uma afinação personalizada, nas duas visões (Circle e Line). Depende do card "Interface: gráfico dos steps da EDO na aba Tuning".
	
	Ver RF17 no Spec.
	
	**Decidido (11/11/2026)**
	- Mudar o EDO por qualquer caminho (knob, automação do Ableton, preset) descarta a afinação desenhada e volta às linhas equidistantes com o novo número de linhas.
	- O Lock da aba Tuning também trava o gráfico: sem alças, sem arrasto.
	- Step 0 fixo em 0 ¢, oitava sempre 1200 ¢, linhas não se cruzam; botão Reset (equal).
	- Salva no estado do plugin, não automatizável.
	
	**Esboço**
	
	![[Esboço — Tuning Steps — Fase 2 (arrastar).png]]
	
	**Decidido (12/11/2026)**
	- Transposição em steps dos osciladores, Pitch da matriz e pitch bend seguem a tabela desenhada (ver RF17 no Spec).
	
	**Etapas**
	- [x] `TuningTable` (lógica pura): tabela por step, mover linha sem cruzar vizinhas, texto do estado; `TuningEngine` lê a tabela; 8 testes novos (97 no total): tabela igual = afinação de hoje; 7 EDO step 4 em 702 ¢ = 660 Hz
	- [x] Voz: transposição em steps e Pitch da matriz pela tabela; ponto do gráfico pela tabela
	- [x] Processador: tabela no buffer triplo, salva no estado (`tuningTable`), descartada quando o EDO muda
	- [x] Interface: alças nas duas visões, arrasto com texto (cents, posição igual, vizinhas), Reset (equal), "custom" no centro, Lock
	- [x] Build Debug/Release sem warnings, ctest, pluginval nível 5 (12/11/2026)
	- [x] Teste no Ableton (teste 18 do Spec), aprovado em 12/11/2026
	
	> Tentar: importar e exportar arquivos Scala (.scl).
	
	#Matiza
- [x] # Interface: gráfico dos steps da EDO na aba Tuning
	
	Abaixo dos knobs da aba Tuning, um painel **Steps** que mostra onde caem os passos da EDO dentro da oitava, em duas sub-abas: **Circle** (radial, uma volta = uma oitava) e **Line** (a mesma oitava na horizontal). Ao tocar uma nota, a linha do step dela pulsa.
	
	Esta é a fase 1 (só visualização). A fase 2 (arrastar as linhas) é o card "Feat: afinação desenhada no gráfico dos steps".
	
	Ver RF16 no Spec.
	
	**Decidido (11/11/2026)**
	- Espaçamento em cents (o que o ouvido percebe); Hz só no texto e na régua de cima da Line.
	- Line mostra uma oitava só.
	- Referência 12-TET opcional: botão **12-TET** no painel, desligado por padrão, salvo no estado (não é parâmetro).
	- O ponto da nota acompanha pitch bend e glide, sempre ligado (custo de CPU desprezível: 8 escritas atômicas por bloco, então não precisa de botão). Modulação de pitch da matriz não entra, para o gráfico não tremer com vibrato.
	
	**Esboços**
	
	![[Esboço — Tuning Steps — Circle.png]]
	
	![[Esboço — Tuning Steps — Line.png]]
	
	**Etapas**
	- [x] RF16 no Spec
	- [x] `src/StepMath.h`: step, oitava e cents de uma nota (mod positivo abaixo da Ref Note), posições, números visíveis, brilho; 11 testes novos (89 no total, passando no Linux)
	- [x] Processador publica por voz, em atômicos, se está segurada, a nota e a altura atual em cents (com glide e bend): `MatizaVoice::getPitchSnapshot`, `PluginProcessor::voicePitchDisplay`
	- [x] `src/TuningStepsView`: sub-abas Circle/Line, linhas, números (um aceso esconde vizinhos por cima), step 0 em bronze, lista das notas seguradas
	- [x] Notas acesas com brilho que se apaga (~300 ms) e ponto que anda com bend/glide (timer de 30 Hz)
	- [x] Botão 12-TET e sub-aba salvos no estado do plugin (`stepsTwelveTet`, `stepsView`)
	- [x] Aba Tuning: grupo Steps abaixo dos knobs (`TabPage::addRow` com altura máxima por linha; nota com altura fixa)
	- [x] Pré-visualização renderizada com o JUCE no Linux (stub do processador): Circle e Line em 19 e 72 EDO
	- [x] Build Debug/Release no Windows sem warnings (corrigido o C4459 de `maxHeight` em `TabPage::addRow`), 89 testes, pluginval nível 5 (12/11/2026)
	- [x] Ajustes do teste (12/11/2026): miolo do Circle com duas linhas de altura fixa ("N EDO" e "x ¢", sem sobreposição); tamanho mínimo da janela = tamanho inicial (840 × 560)
	- [x] Teste no Ableton: 12 e 19 EDO nas duas visões, acorde, pitch bend, glide no modo Mono, EDO 72, redimensionar a janela
	
	#Matiza
- [x] # Interface: gráfico do LFO com edição manual da onda
	
	Mostrar a forma de cada LFO num gráfico de linhas, com ponto da fase atual.
	
	Permitir desenhar a onda à mão, como no Serum: pontos que o usuário adiciona, arrasta e apaga, ligados por retas ou curvas (arrastar o meio do segmento curva a linha).
	
	As formas prontas (Sine, Triangle, Saw, Square, S&H) continuam como presets: escolher uma carrega os pontos dela, que podem ser editados depois.
	
	Atenção: a forma desenhada não cabe num parâmetro do host. Ela é salva no estado do plugin (junto com o set), mas não é automatizável. Manter o parâmetro `lfoNShape` para compatibilidade.
	
	Decidido (10/11/2026): até 32 pontos; segmentos retos ou curvos (arrastar o meio); grade 1/4, 1/8, 1/16 com snap opcional; formas prontas carregam pontos editáveis. Feito junto com a etapa B do wavetable (onda desenhada nos osciladores), com o mesmo editor.
	
	Ver RF15 no Spec.
	
	**Etapas**
	- [x] RF15 no Spec
	- [x] `ShapePoints`: até 32 pontos, curvas, formas prontas como pontos, texto do estado
	- [x] `TripleBuffer` + `CustomShapes`: tabelas montadas na interface e entregues ao áudio sem lock
	- [x] Mipmaps de formas desenhadas com FFT (sem DC); opção Custom no LFO e nos osciladores
	- [x] `ShapeEditor`: pontos, curvas, grade e snap; à esquerda dos controles nos LFOs (com ponto da fase), balão nos osciladores
	- [x] Pontos salvos no estado do plugin
	- [x] Build Debug/Release sem warnings, 78 testes (9 novos), pluginval nível 5
	- [x] Teste no Ableton: desenhar no LFO e no oscilador, salvar e reabrir o set
	
	#Matiza
- [x] # Interface: controlar várias modulações no mesmo knob (UX)
	
	Hoje, com mais de um slot no mesmo knob, o anel laranja mostra o alcance somado e arrastar ajusta só o primeiro slot.
	
	Melhorar para controlar cada modulação separadamente. Opções a avaliar:
	- Um anel por slot, em raios diferentes (concêntricos), cada um com a cor da sua fonte.
	- Seletor no knob (ex.: pequenos pontos/abas por fonte) que escolhe qual slot o anel ajusta.
	- Clique direito no knob abre um menu com a quantidade de cada slot ligado a ele.
	
	Dar cores diferentes às fontes (alças e anéis) ajuda em qualquer opção.
	
	Decidido (10/11/2026): um anel por slot (concêntricos, na cor da fonte) + menu no clique direito com cada slot (inverter, zerar, remover). Cores das fontes nos tons da Têmpera.
	
	Ver RF14 no Spec.
	
	**Etapas**
	- [x] RF14 no Spec
	- [x] Cores das 8 fontes na paleta (`Palette::modSources`), usadas nas alças do cabeçalho e nos anéis
	- [x] Um anel por slot, concêntricos; o knob encolhe para caber; arrastar um anel muda só o seu slot
	- [x] Clique direito no knob ou num anel: menu com cada slot (Invert, Set amount to 0 %, Remove)
	- [x] Build Debug/Release sem warnings, 69 testes, pluginval nível 5
	- [x] Teste no Ableton: aparência dos anéis e das cores, arrastar cada anel, menu
	
	#Matiza
- [x] # DSP: motor wavetable nos osciladores (etapa A)
	
	Trocar o oscilador PolyBLEP por wavetable com mipmaps (2 por oitava) nas 4 formas atuais (seno, serra, quadrada, triângulo). Mesmos parâmetros e IDs; a interface não muda.
	
	Objetivo: menos CPU (folga para o unison) e menos aliasing nas notas agudas. Ver [[Estudo — Wavetable nos osciladores]] e RF13 no Spec.
	
	Próximas etapas (cards próprios depois): B. onda desenhada (junto com o editor do LFO); C. .wav do Serum + Position.
	
	**Etapas**
	- [x] RF13 no Spec
	- [x] `src/Wavetables.h/.cpp`: 4 formas × 21 mipmaps (2 por oitava), montados uma vez fora do áudio, com 5 testes
	- [x] `Oscillator` lê as tabelas com interpolação linear; mesma interface, voz e gráfico sem mudança
	- [x] CPU medida (192 osciladores): 1,4 % de um núcleo, contra 5 % a 10 % do PolyBLEP; aliasing da serra em 3520 Hz: −61 dB contra −24 dB
	- [x] Build Debug/Release sem warnings, 69 testes, pluginval nível 5
	- [x] Teste no Ableton: notas graves soam iguais; agudas sem "notas fantasmas"; CPU com unison no máximo
	
	#Matiza
- [x] # Feat: editar a forma de onda dos osciladores (estudo de viabilidade)
	
	Permitir que o usuário desenhe ou edite a onda de cada oscilador, como no editor de wavetable do Serum.
	
	Primeiro passo é um **estudo**: é possível e viável? Pontos a investigar:
	- Técnica: trocar o oscilador PolyBLEP por **wavetable** (tabela com um ciclo da onda), com versões filtradas por faixa de frequência (mipmaps) para evitar aliasing em notas agudas.
	- Custo de CPU e memória com 8 vozes × 3 osciladores (× unison, se o card de unison vier antes).
	- Microtonal: a wavetable funciona em qualquer frequência, então a afinação por EDO não é afetada.
	- Onde salvar a onda desenhada (estado do plugin, como a forma manual do LFO) e importação de arquivos .wav de wavetable.
	
	Resultado esperado do estudo: recomendação (fazer / não fazer / fazer versão simplificada) e, se sim, um plano em etapas no Spec.
	
	**Resultado (10/11/2026):** ver [[Estudo — Wavetable nos osciladores]].
	- [x] Benchmark PolyBLEP × wavetable (`Matiza/bench/wavetable/`): wavetable gasta 4 a 5× menos CPU e tem 30 a 40 dB menos aliasing nas notas agudas
	- [x] Recomendação: fazer, em 3 etapas (A. motor wavetable; B. onda desenhada; C. .wav do Serum + Position)
	- [x] Decisão do Guilherme: aprovado, começar pela etapa A (10/11/2026)
	
	#Matiza
- [x] # Feat: unison, detune e blend nos osciladores
	
	Adicionar a cada oscilador, como no Serum:
	- **Unison:** número de cópias da onda por nota (1 a 8, por exemplo).
	- **Detune:** quanto as cópias se desafinam entre si (espalhadas em volta da nota).
	- **Blend:** volume das cópias desafinadas em relação à central.
	
	Junto com isso, espalhar as cópias no estéreo (o synth hoje é mono por voz) para o som ficar largo.
	
	Atenção à CPU: 8 vozes × 3 osciladores × 8 cópias = 192 osciladores. Medir no Live; talvez limitar o unison máximo.
	
	Decidido (10/11/2026): até 8 cópias; detune em cents (0 a 100), espalhado de forma simétrica; blend 0 a 100 %; knob Width (0 a 100 %) por oscilador.
	
	Ver RF12 no Spec.
	
	**Etapas**
	- [x] RF12 no Spec (parâmetros, normalização de volume, fase aleatória, voz estéreo)
	- [x] `src/Unison.h`: desvio, volume e pan de cada cópia, com 7 testes
	- [x] Voz estéreo: 8 cópias por oscilador, filtro em 2 canais, Blend/Width suavizados
	- [x] Parâmetros `oscNUnison`, `oscNDetune`, `oscNBlend`, `oscNWidth` e 4 knobs por oscilador
	- [x] Build Debug/Release sem warnings, 63 testes, pluginval nível 5
	- [x] Teste no Ableton: som, estéreo e CPU com 8 vozes × 3 osciladores × 8 cópias
	
	#Matiza
- [x] # Rebranding: MicroSynth → Matiza (Forja Instruments)
	
	O synth passa a se chamar **Matiza**, e a marca guarda-chuva é **Forja Instruments**. Ver [[Plano de produto — Matiza, by Forja Instruments]] (em `.kanban/docs/`, seção "Lacunas", item "Nome e códigos definitivos").
	
	Precisa acontecer **antes da versão alpha**.
	
	> Atenção: sets salvos com o plugin antigo não vão carregar o novo (os códigos de fabricante e de plugin mudam). Aceitável porque a alpha ainda não foi distribuída.
	
	**CMake (`CMakeLists.txt`)**
	- [x] `project(MicroSynth …)` → `project(Matiza …)`
	- [x] Target `juce_add_plugin(MicroSynth` → `Matiza` (e `target_sources`, `target_include_directories`, `target_compile_definitions`, `target_link_libraries`)
	- [x] `PRODUCT_NAME "Matiza"`
	- [x] `COMPANY_NAME "Forja Instruments"` (hoje `"Guilherme"`)
	- [x] `PLUGIN_MANUFACTURER_CODE Forj` (hoje `Gmsy`)
	- [x] `PLUGIN_CODE Mtza` (hoje `Mcsy`)
	- [x] Adicionar `BUNDLE_ID com.forjainstruments.matiza` (hoje não existe; o JUCE usa o padrão `com.yourcompany.…`)
	- [x] Opção `MICROSYNTH_BUILD_TESTS` → `MATIZA_BUILD_TESTS`
	- [x] Target de testes `MicroSynthTests` → `MatizaTests` (`add_executable`, `target_*`, `catch_discover_tests`)
	
	**Código (`src/`)**
	- [x] Classe `MicroSynthesiser` → `MatizaSynthesiser`, arquivo `src/MicroSynthesiser.h` → `MatizaSynthesiser.h` (`git mv`); atualizar o `#include` e o membro em `PluginProcessor.h`
	- [x] Título no cabeçalho da interface: `g.drawText ("MicroSynth", …)` e comentário do `titleWidth` em `PluginEditor.cpp` (conferir se 140 px ainda cabe "Matiza")
	- [x] Comentários com o nome antigo: `Controls.h`, `Lfo.h`, `MicroSound.h`, `MicroVoice.h`, `PluginEditor.h`, `PluginProcessor.h`, `TuningEngine.h`
	- [x] Renomear também `MicroVoice` e `MicroSound` → `MatizaVoice` e `MatizaSound` (feito em 10/11/2026, depois do card)
	- [ ] Tela "sobre" e logo: não existem hoje (o nome aparece só como texto no cabeçalho). [a definir] se entram neste card ou em card próprio de identidade visual
	- [x] Estado/presets: o estado é salvo com a tag neutra `PARAMS` e não há arquivos de preset com o nome; só conferir após o build
	
	**Pasta e workspace**
	- [x] Apagar `MicroSynth/build/` e então `git mv MicroSynth Matiza`
	- [x] `.claude/CLAUDE.md`: tabela "Projetos" (pasta, produto, `PLUGIN_CODE Mtza`), exemplos de nome de pasta e de tag (`MicroSynth`, `#MicroSynth`), e convenção `PLUGIN_MANUFACTURER_CODE` (`Gmsy` provisório → `Forj`)
	- [x] `.claude/templates/juce-plugin/CMakeLists.txt`: `COMPANY_NAME "Forja Instruments"`, `PLUGIN_MANUFACTURER_CODE Forj`
	- [x] `.claude/skills/kanban/SKILL.md`: exemplos `#MicroSynth` → `#Matiza`
	- [x] Kanban: trocar a tag `#MicroSynth` → `#Matiza` nos cards (13 hoje); a cor da tag (`tag-colors` nas configurações do quadro) também cita `#MicroSynth` — ajustar pelo Obsidian
	- [x] `Spec — Setup da máquina e workspace de plugins.md` (raiz): árvore de pastas (linha 37) e menção na linha 156
	- [ ] [a definir] `HelloGain` (CMakeLists com `Guilherme`/`Gmsy`, e o Spec dele cita o MicroSynth): atualizar ou deixar como está, já que é descartável
	
	**Identidade visual**
	- [x] Nova paleta de cores da Matiza: **Têmpera** (cores do aço aquecido na forja), escolhida em 10/11/2026
	- [x] Aplicar a paleta: `src/Palette.h` centraliza as cores e substitui o esquema Midnight do JUCE, o laranja da modulação e o ciano dos gráficos
	- [x] Conferir o visual no Ableton (contraste dos knobs, abas, matriz e gráficos)
	- [ ] [a definir] cores por fonte de modulação (card do Backlog) usando os tons da têmpera
	
	**Spec e README**
	- [x] `Spec — Sintetizador Microtonal (JUCE VST3).md`: "Nome provisório" → nome definitivo; tabela de classes (`MicroSynthesiser`); árvore `microsynth/`; bloco do CMake de exemplo; caminhos `MicroSynth.vst3` / `MicroSynth_artefacts`; plano de etapas e testes; marcar como resolvida a pendência "Nome definitivo do produto e códigos"
	- [x] `README.md`: título e caminho do Standalone (`build/Matiza_artefacts/Debug/Standalone/Matiza.exe`)
	
	**Build e validação**
	- [x] Build limpo: `cmake -B build` + Debug e Release
	- [x] Testes (`ctest`) passando
	- [x] pluginval nível 5 (skill `build-e-validar`)
	- [x] Remover `MicroSynth.vst3` antigo de `C:\Program Files\Common Files\VST3\Dev`
	- [x] Rescan dos plug-ins no Ableton
	
	**Critério de aceite**
	- [x] O Ableton lista "Matiza" do fabricante "Forja Instruments"
	- [x] Nenhuma ocorrência de `MicroSynth`/`Microsynth`/`microsynth`/`Gmsy`/`Mcsy` no código (exceto histórico do git e o próprio plano); "Guilherme" só como autor, não como `COMPANY_NAME`
	- [x] Testes passam
	
	#Matiza
- [x] # Interface: Mod Env em aba dentro do grupo Envelope (UX)
	
	Tirar o Mod Env da aba Matrix e colocá-lo no espaço do grupo Envelope (aba Amp & Filter), com abas internas: **Amp Env** | **Mod Env**.
	
	Cada aba interna mostra o gráfico e os 4 knobs do envelope escolhido; os IDs dos parâmetros não mudam.
	
	A aba Matrix fica só com a lista dos 8 slots (sobra espaço para ela).
	
	#Matiza
- [x] # Interface: gráfico das ondas dos osciladores
	
	Mostrar em cada grupo de oscilador (aba Oscillators) um pequeno gráfico com um ciclo da forma de onda escolhida.
	
	Atualiza ao trocar a forma de onda; oscilador desligado aparece apagado.
	
	Base visual para os cards de edição de onda e de unison.
	
	#Matiza
- [x] # Feat: LFO
	
	Adicionar **3 LFOs** como fontes de modulação da matriz (já reservados na lista de fontes: LFO 1, LFO 2, LFO 3).
	
	Implementar **depois** do card "Feat: matriz de modulação" (o LFO só tem efeito ligado a um destino).
	
	Formas: seno, triângulo, serra subindo e descendo, quadrada, sample & hold. Rate em Hz ou sincronizado ao tempo do Ableton (1/32 a 4 compassos, com tercinas e pontuadas). Trigger livre (fase comum, presa ao tempo da música com sync) ou por nota (reinicia a cada nota). Nova aba LFOs.
	
	Ver RF11 no Spec.
	
	#Matiza
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
	
	#Matiza
- [x] # Interface: gráfico visual do filtro e do envelope
	
	Mostrar na aba Amp & Filter:
	- Filtro: curva de resposta de frequência do passa-baixa (eixo em escala log), que se move com Cutoff e Resonance.
	- Envelope: desenho do ADSR (attack, decay, sustain, release) que muda conforme os knobs.
	
	Os gráficos atualizam em tempo real, inclusive com automação do Ableton.
	
	> Tentar: mostrar a posição atual do envelope da última nota tocada (ponto andando na curva).
	
	Ver RF9 no Spec.
	
	#Matiza
- [x] # Feat: Modo Mono (monofônico × polifônico)
	
	Adicionar seletor de modo de vozes: **Poly** (8 vozes, comportamento atual) ou **Mono** (uma nota por vez).
	
	No modo Mono: prioridade para a última nota tocada; ao soltar a nota, volta para a anterior ainda pressionada; legato (troca de nota sem redisparar o envelope) e glide (portamento) ajustável.
	
	Ver RF7 no Spec.
	
	#Matiza
- [x] # Feat: Múltiplos Osciladores
	
	3 osciladores fixos por voz. Cada um com: liga/desliga, forma de onda, volume, oitava, transposição em passos da EDO e desafinação fina (cents).
	
	O oscilador 1 usa o parâmetro `wave` atual; osciladores 2 e 3 começam desligados, para sets antigos soarem igual.
	
	Criar área específica na interface para os osciladores.
	
	Ver RF6 no Spec.
	
	#Matiza
- [x] # Interface: criar mecanismo de abas no vst
	
	Criar abas no vst para o usuário navegar. Afinação fica numa aba separada.
	
	Na aba de afinação, adicionar um seletor (cadeado) que trava os parâmetros de afinação, para evitar que o usuário mexa sem querer.
	
	Ver RF8 no Spec.
	
	#Matiza




%% kanban:settings
```
{"kanban-plugin":"board","list-collapse":[false,false,false,true],"show-checkboxes":true,"new-line-trigger":"enter","hide-card-count":false,"full-list-lane-width":true,"lane-width":350,"new-note-template":"Projetos de Software/Untitled Kanban.md","tag-sort":[{"tag":"#MicroSynth"}],"tag-colors":[{"tagKey":"#Matiza","color":"rgba(255, 0, 0, 1)","backgroundColor":"rgba(255, 255, 255, 1)"}],"move-tags":true,"tag-action":"kanban"}
```
%%