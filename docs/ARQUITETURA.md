# Kingdom of Ashes — arquitetura técnica (registro de decisões)

Fonte de verdade: o documento "Arquitetura técnica em C++ — versão 1.0" do Nidragnnar.
Este arquivo registra **como** ele foi seguido e **onde** houve desvio, e por quê.

## Camadas

| Camada | Alvo CMake | Pasta | Depende de |
|---|---|---|---|
| Engine pura (testável sem janela) | `KingdomEngineCore` | `Engine/Core`, `World`, `Physics`, `Platform/FileSystem`, `Renderer/Animation`, `Renderer/ParticleSystem` | glm, EnTT, nlohmann/json |
| Engine de plataforma e render | `KingdomEngine` | `Engine/Core/Application`, `Platform`, `Renderer`, `Resources`, `UI`, `Debug` | SDL3, OpenGL 3.3 (glad), stb, Dear ImGui |
| Regras do jogo (sem janela) | `KingdomGame` | `Game/Inventory`, `Data`, `Kingdom`, `Buildings`, `Player`, `World`, `Exploration`, `Save` | KingdomEngineCore |
| Jogo (exe) | `KingdomOfAshes` | `Game/Main.cpp`, `App`, `Scenes`, `Render`, `UI`, `Demo` | tudo acima |
| Ferramenta | `AssetGen` | `Tools/AssetGen` | stb, json |
| Testes | `EngineTests`, `GameTests` | `Tests/` | GoogleTest |

## Decisões do documento, como implementadas

- **Game loop**: passo fixo de 60 Hz com acumulador (`ke::FixedTimestep`), render sincronizado com o monitor.
- **Resolução interna 640×360**, pixel-perfect, nearest-neighbor, escala inteira quando cabe (`calcularLetterbox`).
- **Sprite batching**: `Renderer2D` junta até 16 384 quadriláteros por textura numa chamada.
- **Chunks de 32×32** no `TileMap`; o render só percorre os tiles visíveis.
- **ECS (EnTT)** só para objetos dinâmicos (árvores, rochas, baús, tochas, ruínas). Construções e definições são dados comuns, como o documento pede.
- **BuildingData** tem `id`, `name`, `level`, `unlocked`, `constructed`. Os custos vêm de `Data/Buildings/construcoes.json`.
- **Nível separado da aparência**: `spritesAparencia()` procura `casa_n{nível}_e{era}` e cai para `casa_n{nível}`. Para dar visual próprio a uma era, basta acrescentar o sprite ao atlas.
- **KingdomEra** (`DarkAge`, `Restoration`, `Prosperity`, `HighFantasy`) com `eraVisual` contínua: a troca leva cerca de 8 s por era, nunca de uma vez.
- **Saves** em `Saves/Save_001/{metadata,player,inventory,kingdom,world}.json`, com versão do formato, backup em `Save_001.bak` antes de sobrescrever, gravação atômica (.tmp → rename), validação (negativos viram 0, posição inválida volta ao início), queda para o backup se o principal estiver corrompido e um ponto de migração (`SaveSystem::migrar`). Só se salvam as **alterações** do mundo.
- **Dear ImGui** (F1): FPS, chamadas de desenho, colisões, inventário, forçar era, nível das construções, salvar/recarregar.

## Desvios (e por quê)

| Documento | Feito | Motivo |
|---|---|---|
| vcpkg | CMake FetchContent com versões fixas | o vcpkg não está instalado; o FetchContent é o mesmo esquema que já funciona no Roguelike e não exige instalar nada |
| Box2D | não incluído | o documento diz "quando necessário"; a colisão AABB × tiles basta para o 0.1 |
| GoogleTest | GoogleTest v1.17.0 | igual |
| Movimento em 4 direções | sprites em 4 direções, mas com movimento diagonal | igual ao jogo de referência; a direção do sprite segue o eixo dominante. Fácil de travar em 4 se preferir |
| Era muda tiles, prédios, NPCs... | no 0.1 a era muda **só a cor da tela** (saturação, tinta, vinheta), a névoa e as cinzas | a arte das outras eras é da fase 7 do plano; a infraestrutura (era contínua + sprites por era) já existe |
| Assets/ com sprites prontos | arte provisória gerada por código (`AssetGen`) num atlas PNG + JSON | ainda não há pixel art feita à mão; qualquer sprite pode ser trocado editando o PNG, sem mexer no código |

## Versões das bibliotecas

SDL 3.2.30 · glm 1.0.1 · EnTT 3.15.0 · nlohmann/json 3.12.0 · Dear ImGui 1.92.6 · GoogleTest 1.17.0 · stb (commit 4e9b368) · glad (OpenGL 3.3 core, no repositório)
