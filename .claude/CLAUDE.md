# Workspace de plugins de áudio para Ableton Live

Autor: Guilherme. Engenheiro de software (Java/Spring, React, Python), iniciante em C++ e DSP.
Objetivo: criar plugins para uso próprio no Ableton Live 12 e, se funcionarem bem, comercializá-los.
Sistema: Windows 11, Visual Studio Community 2026 (MSVC), CMake 3.22+, JUCE 8.

Responda sempre em português do Brasil. Explique conceitos de C++ e DSP quando aparecerem pela
primeira vez, comparando com Java quando ajudar. Prefira código claro a código "esperto".

## Estrutura do workspace

Raiz: `C:\Users\Pichau\Desktop\Projetos de Software\Plugins Ableton`
O caminho tem espaços: SEMPRE coloque caminhos entre aspas em comandos de shell e no CMake.

```
Plugins Ableton/
├── .claude/            contexto compartilhado (este arquivo, skills, templates)
├── .kanban/            quadro kanban (vault do Obsidian) — ver seção "Kanban"
├── _libs/JUCE/         submódulo git do JUCE 8 — NUNCA editar
├── _tools/
│   ├── juce-build/     build do AudioPluginHost
│   └── pluginval/      pluginval.exe
└── <Projeto>/          uma pasta por plugin, com o nome do produto (ex.: MicroSynth)
```

Regra: pastas que começam com `_` ou `.` são compartilhadas; qualquer outra pasta na raiz é um projeto.

## Kanban

As tarefas de todos os projetos ficam no quadro `.kanban/Plugins Ableton.md` (plugin Kanban do
Obsidian; o Guilherme abre `.kanban/` como vault). Colunas: Backlog → Em Andamento → Em Revisão →
Concluído. Cada card tem título `# <Tipo>: <resumo>` e a tag do projeto (`#MicroSynth`).
Para ler ou editar cards, use a skill `kanban` (o formato é sensível: indentação com TAB e bloco de
configuração no fim). Nunca edite `.kanban/.obsidian/`. Card só vai para Concluído com aprovação do
Guilherme.

## VST3 ou Max for Live?

| Use VST3 (C++/JUCE) quando… | Use Max for Live quando… |
| --- | --- |
| O plugin gera ou processa áudio com DSP próprio | A ideia é uma ferramenta MIDI, gerador ou transformador de notas |
| Deve funcionar em outras DAWs | Precisa controlar o Live (Live API: clips, tracks, cenas) |
| Será vendido para o público geral | O público é só de usuários do Live Suite |
| Desempenho e estabilidade são críticos | Prototipagem rápida de uma ideia |

Em caso de dúvida, pergunte ao Guilherme antes de criar o projeto.

## Layout de um projeto

**VST3 (JUCE):**
```
<Projeto>/
├── CMakeLists.txt      usa ../_libs/JUCE (ver .claude/templates/juce-plugin)
├── src/                PluginProcessor, PluginEditor, classes de DSP
├── tests/              testes unitários (lógica pura, sem JUCE quando possível)
├── Spec*.md            especificação do plugin — fonte da verdade
└── README.md
```

**Max for Live:**
```
<Projeto>/
├── device/<Nome>.amxd  patch (montado pelo Guilherme no editor do Max)
├── js/                 código JavaScript carregado pelo objeto v8
├── gen/                trechos de gen~ (codebox), quando houver
├── Spec*.md            especificação + lista de objetos que o patch precisa
└── README.md
```

A especificação de cada projeto é o arquivo `Spec*.md` na raiz da pasta do projeto
(ou `docs/SPEC.md`, se existir). Leia antes de qualquer mudança.

## Convenções

- Nome da pasta do projeto = nome do produto em PascalCase (`MicroSynth`).
- `PLUGIN_MANUFACTURER_CODE`: `Gmsy` (provisório, igual para todos os plugins).
- `PLUGIN_CODE`: 4 caracteres únicos por plugin, com ao menos uma maiúscula (ex.: `Mcsy`). Registrar na tabela de projetos abaixo.
- IDs de parâmetros: camelCase e estáveis para sempre (mudar quebra sets já salvos no Ableton).
- Build em `<Projeto>/build/` (fora do git). Plugins de desenvolvimento vão para `C:\Program Files\Common Files\VST3\Dev`.
- Commits pequenos, mensagem em português, no formato `<Projeto>: <o que mudou>`.

## Regras da thread de áudio (VST3)

Dentro de `processBlock`, `renderNextBlock` e de qualquer código chamado por eles:
- Sem alocação de memória (`new`, `push_back`, `juce::String`…), sem locks, sem I/O, sem log.
- Ler parâmetros via ponteiros de `getRawParameterValue()` (atômicos), guardados no construtor.
- Suavizar mudanças de parâmetro com `juce::SmoothedValue` para evitar estalos.
- Alocar tudo em `prepareToPlay`.
- Osciladores com formas de onda ricas (serra, quadrada) usam PolyBLEP ou outra técnica anti-aliasing.

## Fluxo de trabalho

1. Ler o Spec do projeto antes de qualquer mudança. Se não existir, criar com o Guilherme.
2. Trabalhar em etapas pequenas, seguindo o plano de implementação do Spec.
3. Compilar depois de cada mudança (`cmake --build build --config Debug`) e corrigir erros e warnings.
4. Lógica pura (afinação, matemática de DSP) ganha testes unitários.
5. Ao terminar uma etapa: dizer ao Guilherme o que testar no Ableton; só então considerar concluída.
6. Antes de considerar uma versão estável: skill `build-e-validar` (pluginval nível 5).
7. Ao criar ou mudar o status de um projeto, atualizar a tabela de projetos abaixo.
8. Se a tarefa tiver card no kanban, movê-lo junto com o trabalho (skill `kanban`): Em Andamento ao
   começar, Em Revisão ao entregar para teste, Concluído só após o OK do Guilherme.

## Skills disponíveis

- `novo-plugin` — cria a pasta de um plugin novo (VST3 ou Max for Live).
- `juce-vst3` — padrões de implementação com JUCE.
- `max-for-live` — padrões de devices Max for Live com v8 e gen~.
- `build-e-validar` — build Release, pluginval e instalação.
- `kanban` — criar, mover, detalhar e concluir cards do quadro `.kanban/`.

## Comandos úteis (dentro da pasta do projeto)

```
cmake -B build                              # gerar (Visual Studio é o gerador padrão)
cmake --build build --config Debug          # compilar para desenvolvimento
cmake --build build --config Release        # compilar para uso/validação
ctest --test-dir build -C Debug             # rodar testes
"../_tools/pluginval/pluginval.exe" --strictness-level 5 --validate "<caminho do .vst3>"
```

## Projetos

| Pasta | Produto | Tipo | PLUGIN_CODE | Status |
| --- | --- | --- | --- | --- |
| MicroSynth | MicroSynth | VST3 instrumento | Mcsy | MVP v0.1.0 concluído e validado (pluginval nível 5 + testes no Live); próximo: roadmap pós-MVP |
| HelloGain | HelloGain | VST3 efeito | Hlgn | Teste do setup (descartável); v0.1.0 validada (pluginval nível 5) |
