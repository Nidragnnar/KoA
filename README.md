# Kingdom of Ashes: The Last Crown

Simulação de vida + gestão de reino + RPG, em pixel art top-down. Você é o último herdeiro de uma
linhagem real e volta a um reino em ruínas para reconstruí-lo. O mundo começa **Dark Fantasy** e evolui,
era a era, até a **Alta Fantasia**.

Engine própria em **C++20 + SDL3 + OpenGL 3.3** (Kingdom Engine), com EnTT, Dear ImGui, nlohmann/json e GoogleTest.

## Versão 1.2

A 2.0 está em andamento (identidade e plano em `docs/VISAO_2.0.md`). A fase 9 trouxe as Crônicas do Reino
(tecla C), os títulos do herdeiro, o mercador viajante, três monumentos e o começo do mistério da Queda das Cinzas.
A fase 10 trouxe as estações que mudam a economia, as noites raras (Lua de Sangue, aurora, meteoros) e o
Festival da Colheita, que cresce com a era.

As 8 fases do plano de arquitetura da 1.0 estão feitas:

1. Engine (janela, render em lotes, câmera, entrada, recursos, depuração)
2. Mundo (tilemap em chunks, colisão, câmera)
3. Gameplay básico (coleta, inventário, reconstruir a casa)
4. Vida (relógio, dias, moradores com rotina, diálogos, amizade)
5. Reino (obras, economia diária, população, felicidade, impostos)
6. RPG (floresta, cripta, combate, missões, forja, o Cavaleiro Caído)
7. Progressão visual (4 eras em onda, enfeites, música por era, o dragão)
8. Finalização (3 espaços de save, opções, guia dos primeiros dias, balanceamento, recursos que renascem, mercado, ícone)

| Controle | Ação |
|---|---|
| W A S D / setas | andar |
| E / Espaço | interagir (segure para continuar golpeando) |
| J | atacar |
| H | comer |
| B / K / L / C | obras / reino / diário / Crônicas |
| Esc | menu (salvar, opções, controles, título, sair) |
| F5 | salvar |
| F11 | tela cheia |
| M | som |
| F1 | ferramentas de depuração (ImGui) |

## Compilar

Precisa do Visual Studio 2026 com C++. As bibliotecas são baixadas na primeira configuração.

```powershell
.\ferramentas\compilar.ps1 debug     # compila, regenera a arte e roda os testes
.\ferramentas\roteiro.ps1            # joga sozinho do título à Alta Fantasia e tira capturas (capturas\roteiro)
.\ferramentas\balanceamento.ps1      # simula 60 dias em 4 ritmos e grava docs\BALANCEAMENTO.md
.\ferramentas\empacotar.ps1 1.0      # zip para os amigos, testado depois de extraído
```

## Pastas

- `Engine/`: a Kingdom Engine (núcleo, plataforma, render, mundo, física, áudio, recursos, UI, depuração)
- `Game/`: as regras e as cenas do jogo
- `Data/`: construções, recursos, moradores, diálogos, missões, inimigos, Crônicas/títulos/fragmentos (`Data/Lore`) e mapas (`Data/World/*.txt`, um caractere por tile, editáveis no Bloco de Notas)
- `Assets/`: atlas de sprites, arte do NidraIA (`Assets/Arte`), fonte, ícone e shaders
- `Tools/AssetGen`: monta o atlas (arte importada + procedural + versões por era) e o ícone
- `Tools/Balanceamento`: jogador automático que mede o ritmo da economia
- `Tests/`: testes da engine e do jogo
- `docs/ARQUITETURA.md`: como o documento de arquitetura foi seguido e onde houve desvio
- `docs/BALANCEAMENTO.md`: resultado da simulação de economia
