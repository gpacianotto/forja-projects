---
name: max-for-live
description: Padrões para criar devices Max for Live neste workspace: tipos de device, JavaScript com o objeto v8 (Max 9), DSP com gen~, Live API e entrega do .amxd. Use ao trabalhar em qualquer projeto Max for Live.
---

# Implementando devices Max for Live

## Pré-requisitos

- Live 12 Suite (ou Standard com o add-on Max for Live). Desde o Live 12.2 o Max 9 vem embutido,
  com o objeto `v8` (JavaScript moderno).

## Divisão de trabalho

- O patch (`device/<Nome>.amxd`) é visual: o Guilherme monta no editor do Max seguindo a seção **Patch** do Spec.
- Claude escreve e mantém: `js/*.js` (objeto `v8`), trechos `gen/*.genexpr` (codebox do `gen~`) e o Spec.
- Nunca editar o `.amxd` como texto.

## Tipos de device

| Tipo | Quando |
| --- | --- |
| Max MIDI Effect | Transformar ou gerar notas (padrão para começar) |
| Max Audio Effect | Processar áudio |
| Max Instrument | Gerar som a partir de MIDI |

## JavaScript (v8)

- Arquivo externo: no patch, `v8 <nome>.js`, com o arquivo em `js/` e a pasta adicionada ao caminho de busca
  (ou ao lado do `.amxd` antes de congelar).
- Esqueleto:

```js
autowatch = 1;      // recarrega ao salvar o arquivo
inlets = 1;
outlets = 1;

function msg_int(v) { /* recebe inteiros */ }
function list(...args) { /* ex.: [nota, velocity] vindo de midiparse */ }
function bang() { post("ok\n"); }
```

- **JavaScript não roda em tempo real**: mensagens de alta prioridade são adiadas para a fila de baixa prioridade.
  Serve para lógica de MIDI, geradores, transformações de clip e controle do Live. Para timing apertado ou áudio,
  use objetos nativos do Max ou `gen~`.
- Live API: `new LiveAPI("live_set")`, `.get("tempo")`, `.call("fire")`, observers com callback.
  Só funciona depois que o device carregou (`live.thisdevice` → bang).
- MIDI Tools do Live 12 (Generators/Transformations) são um bom alvo para devices em JavaScript.

## Áudio com gen~

- DSP por amostra dentro do `gen~` (codebox em GenExpr). Guardar o código em `gen/<nome>.genexpr`
  e colar no codebox.
- Parâmetros: `Param nome(default, min=…, max=…);` ligados a `live.dial` fora do `gen~`.

## Parâmetros visíveis no Live

- Usar objetos `live.*` (`live.dial`, `live.numbox`, `live.menu`, `live.toggle`): aparecem na automação
  e são salvos com o set. Definir Long Name, Short Name, faixa e unidade no Inspector.

## Entrega

- Antes de distribuir: **Freeze Device** (embute JS e dependências no `.amxd`) e testar num set novo.
- Venda/distribuição: o comprador precisa ter Max for Live.

## Referências

- Tutoriais de v8 no Live: https://adammurray.link/max-for-live/v8-in-live/
- Documentação: Help do Max ("JavaScript in Max", "Live Object Model").
