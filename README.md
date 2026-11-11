# Kingdom of Ashes: The Last Crown

Simulação de vida + gestão de reino + RPG, em pixel art top-down. Você é o último herdeiro de uma
linhagem real e volta a um reino em ruínas para reconstruí-lo. O mundo começa **Dark Fantasy** e evolui,
era a era, até a **Alta Fantasia**.

Engine própria em **C++20 + SDL3 + OpenGL 3.3** (Kingdom Engine), com EnTT, Dear ImGui, nlohmann/json e GoogleTest.

## Protótipo 0.1 — The Forgotten Kingdom

Entrar no jogo, explorar o vilarejo em ruínas, coletar madeira, pedra e ouro e reconstruir a primeira casa.

| Controle | Ação |
|---|---|
| W A S D / setas | andar |
| E / Espaço | interagir (segure para continuar golpeando) |
| Esc | menu |
| F5 | salvar |
| F11 | tela cheia |
| F1 | ferramentas de depuração (ImGui) |

## Compilar

Precisa do Visual Studio 2026 com C++. As bibliotecas são baixadas na primeira configuração.

```powershell
.\ferramentas\compilar.ps1 debug     # compila, regenera a arte e roda os testes
.\ferramentas\roteiro.ps1            # joga sozinho o caminho do 0.1 e tira capturas (capturas\roteiro)
.\ferramentas\empacotar.ps1 0.1      # zip para os amigos, testado depois de extraído
```

## Pastas

- `Engine/`: a Kingdom Engine (núcleo, plataforma, render, mundo, física, recursos, UI, depuração)
- `Game/`: as regras e as cenas do jogo
- `Data/`: construções, recursos e o mapa do vilarejo (`Data/World/vilarejo.txt`, um caractere por tile, editável no Bloco de Notas)
- `Assets/`: atlas de sprites, fonte e shaders
- `Tools/AssetGen`: gera a pixel art provisória
- `Tests/`: testes da engine e do jogo
- `docs/ARQUITETURA.md`: como o documento de arquitetura foi seguido e onde houve desvio
