---
name: build-e-validar
description: Compila um projeto VST3 em Release, roda o pluginval no nível 5, confirma a instalação na pasta VST3 de desenvolvimento e passa o checklist de teste no Ableton. Use antes de considerar qualquer versão estável ou quando o Guilherme pedir para validar um plugin.
---

# Build e validação

Executar dentro da pasta do projeto (`<Projeto>/`). Caminhos sempre entre aspas (há espaços na raiz).

## 1. Build Release

```
cmake -B build
cmake --build build --config Release
```

Zero erros. Warnings novos: corrigir ou explicar ao Guilherme por que ficam.

## 2. Localizar o .vst3

`build/<Produto>_artefacts/Release/VST3/<Produto>.vst3`
Confirmar que também foi copiado para `C:\Program Files\Common Files\VST3\Dev\`.
Se a cópia falhou por permissão, lembrar do passo 6 do setup (icacls na pasta Dev).

## 3. pluginval

```
"../_tools/pluginval/pluginval.exe" --strictness-level 5 --validate-in-process --output-dir "build/pluginval" --validate "build/<Produto>_artefacts/Release/VST3/<Produto>.vst3"
```

- Passou: registrar a versão e a data no README do projeto.
- Falhou: ler o relatório em `build/pluginval`, corrigir a causa (comum: estado não restaurado,
  alocação na thread de áudio, valores NaN, crash com sample rate ou buffer incomuns) e repetir.

## 4. Checklist no Ableton (passar ao Guilherme)

- [ ] Plug-Ins → Rescan; o plugin aparece na categoria certa (instrumento ou efeito).
- [ ] Carrega sem travar e toca/processa som.
- [ ] Todos os parâmetros aparecem em "Configure" e na automação.
- [ ] Salvar o set, fechar o Live, reabrir: parâmetros restaurados.
- [ ] Duplicar a track 4 vezes e conferir o medidor de CPU.
- [ ] Mudar o sample rate (Preferências → Audio) para 48 kHz e 96 kHz: continua afinado e estável.

## 5. Fechar

Atualizar o status na tabela "Projetos" do `.claude/CLAUDE.md` e fazer commit `<Projeto>: valida vX.Y.Z`.
