---
name: kanban
description: Lê e edita o quadro kanban do workspace (.kanban/Plugins Ableton.md, plugin Kanban do Obsidian) — criar, mover, editar, detalhar e concluir cards. Use quando o Guilherme falar de cards, tarefas, backlog, quadro ou kanban, ao começar ou terminar uma tarefa que tenha card, ou quando pedir "o que tem para fazer".
---

# Kanban

O quadro é **um único arquivo Markdown** lido pelo plugin Kanban do Obsidian (v2.x):

`.kanban/Plugins Ableton.md`

A pasta `.kanban/` é um vault do Obsidian. **Nunca edite `.kanban/.obsidian/`** (configuração do
Obsidian e código do plugin). O Guilherme pode estar com o quadro aberto no Obsidian: sempre leia o
arquivo de novo imediatamente antes de editar, e edite com `Edit` (troca pontual), nunca reescrevendo
o arquivo inteiro.

## Formato do arquivo (não quebrar)

```
---

kanban-plugin: board

---

## Backlog

- [ ] # Título do card
	
	Descrição.
	
	#MicroSynth


## Em Andamento



## Em Revisão



## Concluído





%% kanban:settings
```
{...json...}
```
%%
```

Regras:
- O frontmatter (`kanban-plugin: board`) e o bloco final `%% kanban:settings ... %%` devem ficar
  intactos. Não mexa no JSON de configuração, salvo pedido explícito.
- Cada `## ` é uma coluna (lane). Colunas atuais, em ordem: **Backlog → Em Andamento → Em Revisão →
  Concluído**. Não crie, renomeie nem reordene colunas sem pedido.
- Cada card começa com `- [ ] ` no início da linha. A primeira linha é o título, no padrão
  `# <Tipo>: <resumo>`.
- **Linhas seguintes do card são indentadas com um TAB** (`\t`), não espaços. Linhas "em branco"
  dentro do card são um TAB sozinho (`\t`), para o card não ser cortado.
- O card termina onde começa o próximo `- [ ] ` / `- [x] ` ou a próxima coluna.
- Arquivo em UTF-8 sem BOM, quebras de linha LF. Preserve acentos.
- Dentro do card vale Markdown normal: parágrafos, `> ` para observações, sublistas `- [ ]` (indentadas
  com tab) como checklist, links `[[...]]`.

## Convenções dos cards

- **Título:** `# <Tipo>: <resumo curto>`. Tipos em uso: `Feat` (funcionalidade), `Interface` (UI do
  plugin). Use também `Fix` (bug), `DSP` (algoritmo de áudio), `Infra` (build, CMake, validação, ferramentas),
  `Docs` (Spec, README) quando fizer sentido.
- **Descrição:** o que fazer e por quê, em português. Ideias incertas vão como `> Tentar: ...`.
  Pontos em aberto como `[a definir]`.
- **Tag do projeto:** última linha do card, `#<PastaDoProjeto>` (ex.: `#MicroSynth`). Todo card tem
  exatamente uma tag de projeto; tarefas do workspace inteiro usam `#Workspace`.
  Ao usar uma tag de projeto nova, avise o Guilherme que ele pode dar uma cor a ela nas configurações
  do quadro no Obsidian (não edite o JSON por conta própria).
- **Checkbox:** `- [ ]` em todas as colunas exceto **Concluído**, onde vira `- [x]`.

## Operações

**Listar / "o que tem para fazer":** leia o arquivo e resuma por coluna (título + tag), sem colar o
Markdown cru. Filtre por projeto se ele pedir.

**Criar card:** adicione ao fim da coluna **Backlog** (ou onde ele pedir), no formato acima. Se o
pedido for vago, escreva a descrição com o que se sabe e marque o resto com `[a definir]`, em vez de
inventar requisitos.

**Começar uma tarefa:** mova o card (o bloco inteiro, com todas as linhas indentadas) para
**Em Andamento**. Ao mover, coloque-o no fim da coluna de destino, mantendo uma linha em branco antes
do próximo `## `.

**Detalhar:** ao planejar a implementação, pode acrescentar ao card um checklist de etapas
(sublista `- [ ]` indentada com tab) e marcar `- [x]` conforme avança. Requisitos de verdade vão para
o `Spec*.md` do projeto; o card só referencia (ex.: "ver RF5 no Spec").

**Terminar a implementação:** depois de compilar e passar os testes, mova para **Em Revisão** e diga
ao Guilherme o que testar no Ableton (fluxo do CLAUDE.md).

**Concluir:** só mova para **Concluído** (com `- [x]`) quando o Guilherme confirmar que testou e
aprovou. Nunca conclua por conta própria.

**Remover card:** só com pedido explícito. Prefira mover para Concluído ou perguntar.

## Integração com o fluxo de trabalho

- Ao começar trabalho num projeto, verifique se existe card correspondente; se existir, mova para
  Em Andamento. Se não existir e a tarefa não for trivial, pergunte se deve criar um.
- Mudanças no quadro entram no mesmo commit da tarefa, ou em commit próprio
  `Kanban: <o que mudou>`.
- Depois de editar, releia o trecho alterado e confira os TABs (`cat -A` mostra `^I`) e que o bloco
  `%% kanban:settings` continua no fim.
