# Spec — Setup da máquina e workspace de plugins

Nov 6, 2026 · @Guilherme

## Objetivo

Ao fim deste setup, o Windows 11 compila um plugin VST3 de teste, o Ableton o carrega, e o Claude Code entende o workspace inteiro por meio da pasta `.claude` na raiz.

- **Workspace:** uma única pasta (`C:\Users\Pichau\Desktop\Projetos de Software\Plugins Ableton`), com uma subpasta por plugin nomeada com o nome do produto (ex.: `Matiza`).
- **Compartilhado entre projetos:** a pasta `.claude` (convenções, skills e template), uma única cópia do JUCE e as ferramentas de validação.
- **Dois tipos de projeto:** VST3 em C++/JUCE e devices Max for Live (patch + JavaScript v8).
- **Tempo estimado:** de 1 a 2 horas, a maior parte em downloads.

A pasta `.claude` e o `.gitignore` já foram criados na raiz do workspace.

## Estrutura do workspace

Tudo o que é compartilhado começa com `_` ou `.`, e qualquer outra pasta na raiz é um projeto de plugin.

```
Plugins Ableton\
├── .claude\                    (contexto do Claude Code para todo o workspace)
│   ├── CLAUDE.md               (convenções, regras e lista de projetos)
│   ├── settings.json           (permissões de comandos)
│   ├── skills\
│   │   ├── novo-plugin\        (cria a pasta de um plugin novo)
│   │   ├── juce-vst3\          (como implementar VST3 com JUCE)
│   │   ├── max-for-live\       (como implementar devices M4L)
│   │   └── build-e-validar\    (build Release + pluginval + instalação)
│   └── templates\
│       └── juce-plugin\        (esqueleto CMake + processador de exemplo)
├── _libs\
│   └── JUCE\                   (submódulo git, uma cópia para todos)
├── _tools\
│   └── pluginval\              (validador de plugins)
├── .gitignore
├── Matiza\                     (projeto VST3: sintetizador microtonal, com o Spec dentro)
└── <ProximoPlugin>\            (cada plugin, sua pasta)
```

**Caminho com espaços:** a raiz fica em `C:\Users\Pichau\Desktop\Projetos de Software\Plugins Ableton`. CMake e MSVC lidam bem com espaços desde que os caminhos estejam entre aspas nos comandos, e o `CLAUDE.md` já instrui o Claude Code a fazer isso. Se o Desktop estiver sincronizado pelo OneDrive, exclua as pastas `build` da sincronização.

**Versionamento:** o workspace é um único repositório git (monorepo), com o JUCE como submódulo. As pastas `build\` de cada projeto ficam fora do git.

## Ferramentas

Quase tudo se instala pelo winget, que já vem no Windows 11. Só o JUCE e o pluginval são baixados à mão.

| Ferramenta | Para quê | Como instalar |
| --- | --- | --- |
| Visual Studio Community 2026 + workload C++ desktop | Compilador MSVC e Windows SDK | winget (`Microsoft.VisualStudio.Community`) |
| Git | Versionamento e submódulo do JUCE | winget (`Git.Git`) |
| CMake 3.22+ | Gera e compila os projetos JUCE | winget (`Kitware.CMake`) |
| JUCE 8 | Framework dos plugins VST3 | `git submodule` em `_libs\JUCE` |
| AudioPluginHost | Testar plugins sem abrir o Ableton | Compilado a partir do JUCE |
| pluginval | Validar estabilidade antes de usar ou vender | Download do GitHub da Tracktion para `_tools\pluginval` |
| Claude Code | Implementação assistida, lendo a pasta `.claude` | Instalador nativo para Windows (PowerShell) |
| VS Code (opcional) | Editar código e arquivos JavaScript dos devices M4L | winget (`Microsoft.VisualStudioCode`) |
| Max for Live (Max 9) | Devices exclusivos do Ableton | Já vem no Live Suite (ou Standard + add-on) |

Um detalhe do winget: o ID `Microsoft.VisualStudio.Community` instala a versão atual (2026), e `Microsoft.VisualStudio.2022.Community` fixaria a versão antiga ([fonte](https://github.com/dotnet/runtime/pull/131462)). Para o Max for Live, desde o Live 12.2 o Max 9 vem embutido, com o objeto `v8` de JavaScript moderno ([fonte](https://adammurray.link/max-for-live/js-in-live/)).

## Passo a passo do setup

São oito passos, nesta ordem. Os comandos são para o PowerShell, e os passos 1 e 6 exigem abri-lo como administrador.

- [ ] **1. Instalar as ferramentas (PowerShell como administrador)**

```powershell
winget install -e --id Microsoft.VisualStudio.Community --override "--passive --wait --add Microsoft.VisualStudio.Workload.NativeDesktop --includeRecommended"
winget install -e --id Git.Git
winget install -e --id Kitware.CMake
winget install -e --id Microsoft.VisualStudioCode   # opcional
```

Se o instalador do Visual Studio terminar com o código 3010, só é preciso reiniciar o computador. Depois, abra um PowerShell novo para o PATH ser atualizado.

- [ ] **2. Claude Code (já instalado)**

Já instalado. Só confira a versão:

```powershell
claude --version
```

- [ ] **3. Criar o workspace e adicionar o JUCE**

```powershell
cd "C:\Users\Pichau\Desktop\Projetos de Software\Plugins Ableton"
git init
git submodule add https://github.com/juce-framework/JUCE _libs/JUCE
git -C _libs/JUCE tag --list "8.*"        # escolha a tag 8.x mais recente
git -C _libs/JUCE checkout <tag-escolhida>
```

- [ ] **4. Pasta .claude (já criada)**

Feito: a pasta `.claude` e o `.gitignore` já estão na raiz do workspace.

- [ ] **5. Compilar o AudioPluginHost**

```powershell
cmake -S _libs/JUCE -B _tools/juce-build -DJUCE_BUILD_EXTRAS=ON
cmake --build _tools/juce-build --target AudioPluginHost --config Release
```

A primeira compilação demora alguns minutos. O executável sai em `_tools\juce-build\extras\AudioPluginHost\AudioPluginHost_artefacts\Release\`.

- [ ] **6. Liberar a pasta VST3 de desenvolvimento (PowerShell como administrador, uma vez só)**

```powershell
New-Item -ItemType Directory "C:\Program Files\Common Files\VST3\Dev" -Force
icacls "C:\Program Files\Common Files\VST3\Dev" /grant "${env:USERNAME}:(OI)(CI)M"
```

Com isso, cada build copia o `.vst3` para essa pasta sem pedir permissão de administrador, e o Ableton já enxerga o plugin.

- [ ] **7. Baixar o pluginval**

Baixe a versão mais recente para Windows nas [releases do pluginval](https://github.com/Tracktion/pluginval/releases) e extraia em `_tools\pluginval\`, de modo que exista `_tools\pluginval\pluginval.exe`.

- [ ] **8. Configurar o Ableton e fazer o primeiro commit**

* No Live: Preferências → Plug-Ins → ativar "Use VST3 Plug-in System Folders" e clicar em "Rescan".
* Confirmar a edição do Live. Max for Live exige a Suite (ou a Standard com o add-on).
* No workspace: `git add . ; git commit -m "setup do workspace"`.

## Pasta .claude

O Claude Code lê o `CLAUDE.md` ao abrir qualquer sessão no workspace, e carrega cada skill quando a tarefa pede. Assim, cada projeto novo nasce com as mesmas convenções sem repetir contexto.

| Arquivo | Papel |
| --- | --- |
| `CLAUDE.md` | Quem é o autor, estrutura do workspace, quando usar VST3 ou Max for Live, convenções de pasta e nomes, regras da thread de áudio, fluxo de trabalho em etapas e lista de projetos |
| `settings.json` | Libera sem perguntar `cmake`, `ctest`, os comandos git de leitura e commit e o pluginval; bloqueia edições em `_libs` e `git push --force` |
| `skills/novo-plugin` | Cria a pasta de um plugin: pergunta nome e tipo (efeito VST3, instrumento VST3, MIDI M4L, áudio M4L), copia o template, gera os códigos do plugin, cria o Spec do projeto e faz o primeiro build |
| `skills/juce-vst3` | Padrões de implementação JUCE: CMake, parâmetros (APVTS), sintetizadores e vozes, DSP com `juce::dsp`, anti-aliasing, salvar estado |
| `skills/max-for-live` | Como trabalhar com devices M4L: tipos de device, objeto `v8`, `gen~` para áudio, Live API, limites (o JavaScript não roda em tempo real), congelar o device |
| `skills/build-e-validar` | Build Release, pluginval nível 5, cópia para a pasta VST3 e checklist de teste no Ableton |
| `templates/juce-plugin` | `CMakeLists.txt` com o JUCE compartilhado e um processador de ganho de exemplo, que já compila |

**Convenção para Max for Live:** o patch (`.amxd`) é montado por você no editor do Max, porque é um arquivo visual. O Claude escreve e mantém o JavaScript (`js/*.js`, carregado pelo objeto `v8`) e os trechos de `gen~`, e documenta no SPEC quais objetos o patch precisa ter.

## Verificação final

O setup está pronto quando os cinco testes abaixo passarem.

- [ ] `cmake --version` mostra 3.22 ou superior, e `git --version` e `claude --version` respondem.
- [ ] Dentro do workspace, `claude` abre e responde corretamente à pergunta "quais skills este workspace tem?" (prova que leu a pasta `.claude`).
- [ ] Pedir ao Claude Code: "use a skill novo-plugin para criar um efeito VST3 chamado hello-gain". O build termina sem erros.
- [ ] O Ableton lista o Hello Gain depois do rescan, carrega numa track de áudio e o knob de ganho altera o volume.
- [ ] O pluginval passa no nível 5 para o Hello Gain (skill build-e-validar).

Se você tiver o Max for Live: crie um "Max MIDI Effect" vazio no Live, adicione um objeto `v8` com `post("ok")` e confira a mensagem no console do Max.

O `hello-gain` pode ser apagado depois; ele serve só para provar a cadeia inteira antes de começar o MicroSynth (hoje Matiza).

## Questões em aberto

- [ ] Qual edição do Live você tem? Sem a Suite (ou o add-on), a skill max-for-live fica guardada para o futuro.
- [ ] Monorepo ou um repositório por plugin? O monorepo é mais simples agora; para vender, talvez valha separar depois.
- [ ] O Desktop está sincronizado pelo OneDrive? Se estiver, exclua as pastas `build` da sincronização.
- [ ] Repositório remoto (GitHub privado) desde já?
