---
name: novo-plugin
description: Cria a pasta de um novo projeto de plugin no workspace (VST3 com JUCE ou device Max for Live), com template, Spec inicial e primeiro build. Use quando o Guilherme pedir para começar um plugin novo.
---

# Novo plugin

## 1. Perguntar (uma mensagem só)

- Nome do produto (PascalCase, vira o nome da pasta). Ex.: `TapeWobble`.
- Tipo: **efeito VST3**, **instrumento VST3**, **efeito MIDI Max for Live** ou **efeito de áudio Max for Live**.
  Se ele não souber, use a tabela "VST3 ou Max for Live?" do CLAUDE.md para sugerir.
- Ideia em uma ou duas frases (vai para o Spec).

## 2. Escolher o PLUGIN_CODE

4 caracteres, ao menos uma maiúscula, diferente dos já listados na tabela de projetos do CLAUDE.md.
Derive do nome (ex.: TapeWobble → `Tpwb`).

## 3a. Projeto VST3

1. Copiar `.claude/templates/juce-plugin/` para `<Nome>/`.
2. No `CMakeLists.txt`, substituir:
   - `@PRODUCT_NAME@` → nome do produto
   - `@PLUGIN_CODE@` → código escolhido
   - Instrumento: `IS_SYNTH TRUE` e `NEEDS_MIDI_INPUT TRUE`; e no processador, trocar o barramento de entrada
     por apenas saída estéreo (ver skill `juce-vst3`).
3. Criar `Spec — <Nome>.md` na raiz do projeto a partir da ideia, com as seções: Visão geral, Escopo do MVP,
   Parâmetros (tabela), Arquitetura, Plano de implementação (etapas com critério de aceite), Testes.
   Rascunhe e peça para o Guilherme revisar antes de implementar.
4. Primeiro build, dentro da pasta do projeto:
   `cmake -B build` e `cmake --build build --config Debug`.
5. Dizer ao Guilherme: dar "Rescan" nos plug-ins do Ableton e carregar o plugin.

## 3b. Projeto Max for Live

1. Criar `<Nome>/device/`, `<Nome>/js/` e, se houver DSP, `<Nome>/gen/`.
2. Criar `js/<nome>.js` com o esqueleto da skill `max-for-live`.
3. Criar o Spec como no VST3, incluindo a seção **Patch**: lista de objetos que o Guilherme deve colocar
   no patch e como conectá-los (o `.amxd` é montado por ele no editor do Max).
4. Explicar como criar o device vazio no Live (browser → Max for Live → "Max MIDI Effect" ou
   "Max Audio Effect"), salvar como `device/<Nome>.amxd` e adicionar o objeto `v8 <nome>.js`.

## 4. Fechar

- Acrescentar a linha do projeto na tabela "Projetos" do `.claude/CLAUDE.md`.
- Commit: `<Nome>: cria projeto`.
