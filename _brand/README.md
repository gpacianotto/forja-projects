# Marca — Forja Instruments e Matiza

Arquivos oficiais das logos, aprovadas em 12/11/2026:

- **Forja Instruments:** conceito A, **Bigorna** (símbolo) + FORJA desenhada em blocos + INSTRUMENTS.
- **Matiza:** **MTZ refinado** a partir do esboço original (M, T e Z presos numa barra contínua).

As duas usam o mesmo módulo de desenho (hastes e barras com espessura única, cortes a 45°, como barra forjada) e a paleta **Têmpera** da interface.

## Qual arquivo usar

| Onde | Arquivo |
| --- | --- |
| Logo da Forja sobre fundo claro | `forja/forja-horizontal-escura.svg` |
| Logo da Forja sobre fundo escuro | `forja/forja-horizontal-clara.svg` |
| Logo da Forja já com fundo ferro (apresentações) | `forja/forja-horizontal-sobre-ferro.svg` |
| Símbolo da Forja sozinho (assinatura pequena, rodapé) | `forja/forja-simbolo-escuro.svg`, `-claro.svg` ou `-palha.svg` |
| Avatar das redes, favicon do site da Forja | `forja/forja-avatar.svg`, `forja/png/forja-avatar-<tamanho>.png` |
| Cabeçalho do plugin Matiza (fundo escuro) | `matiza/mtz-palha.svg` |
| MTZ sobre fundo claro / escuro | `matiza/mtz-escuro.svg` / `matiza/mtz-claro.svg` |
| Ícone do Matiza (app Standalone, favicon da landing page) | `matiza/matiza-icone.svg`, `matiza/png/matiza-icone-<tamanho>.png` |
| Assinatura "MATIZA by Forja Instruments" (tela Sobre, vídeos, landing page) | `matiza/matiza-assinatura-escura.svg` / `-clara.svg` |

Os SVGs sem a palavra "sobre" têm fundo transparente. Os PNGs estão em `forja/png/` e `matiza/png/` (ícones de 16 a 512 px; logos com 600 e 1600 px de largura).

## Cores (paleta Têmpera)

| Nome | Hex | Uso |
| --- | --- | --- |
| Ferro | `#14161B` | Fundo escuro |
| Texto | `#E9E4DA` | Logo sobre fundo escuro |
| Palha | `#D9A441` | Cor principal; MTZ na interface, símbolo no avatar |
| Tinta | `#121317` | Logo sobre fundo claro |
| Cinza | `#55575E` / `#9A9DA6` | "INSTRUMENTS" e "by Forja Instruments" (claro / escuro) |

Os mesmos valores estão em `Matiza/src/Palette.h`.

## Tamanho mínimo

- **MTZ:** 30 px de altura (testado no cabeçalho do plugin).
- **Símbolo da Forja e ícone do Matiza:** 16 px (favicon); prefira 32 px ou mais.
- **Logo horizontal da Forja:** 320 px de largura; abaixo disso, use só o símbolo (o "INSTRUMENTS" fica ilegível).
- **Área de proteção:** deixe em volta da logo um espaço livre de pelo menos a espessura de uma haste (cerca de 1/3 da altura do MTZ ou da palavra FORJA).

## O que não fazer

- Não distorcer (esticar ou achatar), não girar, não contornar.
- Não trocar as cores fora da paleta acima; sobre foto ou fundo poluído, use a versão com fundo (`-sobre-ferro`).
- Não redesenhar à mão: qualquer ajuste é feito na fonte (abaixo).

## Como ajustar ou gerar de novo

A geometria está em `fonte/forge.py` (módulo comum: altura `H`, haste `S`, barra `B`, espaço `G`, chanfro `CH`) e `fonte/gerar.py` monta todos os arquivos:

```
cd _brand/fonte
pip install shapely fonttools cairosvg
python gerar.py
```

Sem o `cairosvg`, só os SVGs são gerados. O texto "INSTRUMENTS", "MATIZA" e "by Forja Instruments" usa a fonte **Inter SemiBold** convertida em curvas (os arquivos não dependem da fonte instalada). A Inter é livre (SIL Open Font License 1.1): ver `fonte/fonts/OFL.txt`.
