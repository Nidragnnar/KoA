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

## Fase 4 — sistemas de vida

- **Relógio** (`Game/Kingdom/Clock`): das 6:00 às 2:00, 10 minutos a cada 7 s reais, semanas, 4 estações de 28 dias. Para em menus, painéis, conversas e no sono.
- **Dia e noite**: `ke::LightMap` (Engine/Renderer/Lighting) é um framebuffer que começa na cor ambiente e soma as luzes; o resultado multiplica a cena. Tochas, fogueira, janelas das casas reconstruídas e uma luz fraca no herói.
- **Moradores** como entidades EnTT com os componentes do documento: `Posicao`, `Movimento`, `Agenda` (rotina) e `Relacionamento`. Definições em `Data/NPCs/npcs.json`, falas em `Data/Dialogues/dialogos.json`, locais das rotinas em `Data/World/vilarejo.json`.
- **Rotina por horário** e caminho por **A\*** (`Engine/World/Pathfinding`) numa grade de bloqueio refeita quando um objeto some. **Simulação simplificada para quem está longe**: a mais de 380 px do herói, o morador anda em passos maiores, 4 vezes por segundo. Sem caminho possível, vai direto ao destino (para nunca travar).
- **Diálogos**: vale a primeira entrada cujas condições batem (primeiro encontro, já falou hoje, período, estação, nível da casa, amizade). Entradas `uma_vez` viram **memória** de acontecimentos e não se repetem. Amizade: +25 na primeira conversa do dia (100 = um coração, máximo 10).
- **Save v2**: o relógio vai no `world.json`, e há dois arquivos novos, `npcs.json` (conheceu, memória) e `relationships.json` (amizade). Um save v1 é migrado sozinho (`SaveSystem::migrar`).

Ainda não feito nesta fase: necessidades dos moradores, presentes (os gostos e desgostos já estão nos dados, mas faltam itens para dar) e chegada de moradores novos.

## Fase 5 — construção e reino

- **Construções** em `Data/Buildings/construcoes.json`: 8 tipos construíveis (moradia, campo, taverna, mercado, capela, forja, posto de guarda, torre), com 3 níveis, custos, `efeitos` por nível (`populacao_max`, `felicidade`, `seguranca`, `comida_dia`, `ouro_dia`...) e `limite`.
- **Colocação**: `World::podeConstruir` confere borda, água e muro, obstáculos, o herói e os locais das rotinas. A obra nasce com `constructed = false` (o campo do documento) e fica pronta na troca de dia. A prévia segue o herói (fica na frente dele).
- **Economia** (`Game/Economy`): `calcularResumo` soma os efeitos das construções prontas. `simularDia` roda em cada noite: impostos (1, 2 ou 3 ouro por morador), produção, consumo de comida (1 por morador), felicidade caminhando 40% rumo ao alvo, população que cresce (felicidade ≥ 50 e casa sobrando) ou vai embora (fome ou felicidade < 25).
- **Aldeões**: entidades com o componente `Aldeao`, tantas quanto `população − moradores com nome` (até 16). Passeiam entre locais e portas e somem à noite.
- **Eras**: desenvolvimento = soma dos níveis das construções prontas − 1. Limiares 8, 20 e 35.
- **Save v3**: `kingdom.json` guarda a lista de construções (com o tile das que o jogador colocou) e o estado do povo. Os saves v1 e v2 são migrados.

## Fase 6 — RPG e exploração

- **Regiões** (`Data/World/<id>.json` + `.txt`): vilarejo, Floresta Amaldiçoada e Cripta do Cavaleiro. Só a região ativa vive em `World::mapa` e `World::reg`; as outras ficam guardadas e são trocadas com `std::swap` (barato). As saídas são retângulos de tiles com destino e ponto de chegada. Cada região tem escuridão mínima e cor de penumbra próprias, que alimentam o mapa de luz. Os ids dos objetos fora do vilarejo levam o prefixo `regiao:`, para o save não confundir.
- **Inimigos** (`Data/Enemies/inimigos.json`, letra no mapa): componente `Inimigo` com vagar, perseguir (visão), recuo ao levar golpe, dano por contato com pausa, e a arrancada periódica do chefe. Renascem a cada manhã; o chefe derrotado não volta (marco `chefe_<id>`).
- **Combate**: J golpeia uma área na frente do herói, e cada golpe acerta cada inimigo uma vez só. Dano = ataque − defesa (mínimo 1). O herói tem 100 de vida, pisca invulnerável por 0,9 s depois de ser atingido e recua. H come 3 de comida e recupera 25 de vida. Derrotado, acorda em casa no dia seguinte (`World::reviver`).
- **Equipamento** (`Data/Items/equipamentos.json`): armas e armaduras com nível mínimo de forja e custo. É feito na porta da forja e equipado se for melhor que o atual.
- **Missões** (`Data/Quests/missoes.json`): o morador oferece a missão no fim da conversa (com requisito opcional). Objetivos de derrotar, construir, ter ou visitar, e recompensa na hora (recursos, item, amizade). Diário em L.
- **Save v4**: `player.json` ganha região, vida e equipamento; arquivo novo `quests.json` (missões, criaturas derrotadas, regiões visitadas, marcos). Os objetos de todas as regiões são salvos. Saves v1 a v3 são migrados.

## Fase 7 — progressão visual

- **Arte por era derivada da arte da era 1** (`Tools/AssetGen/Eras.cpp`): depois de importar a arte do NidraIA, cada sprite de terreno, vegetação, pedra, tocha e construção ganha versões `_e1`, `_e2` e `_e3`, recoloridas em HSV. Verdes mais vivos; nas construções o musgo some, os tons frios esquentam e as paredes clareiam; pedra embranquece; tochas viram chama azul na era 3; a grama ganha flores e as árvores florescem e brilham. Assim o estilo da arte importada acompanha a evolução. A forma das construções continua vindo do **nível** delas.
- **Transformação em onda** (`World::eraNoTile`): cada tile tem um atraso proporcional à distância da casa do herdeiro (com um pouco de acaso). Durante os ~8 s de transição de era, a mudança se espalha a partir da casa. Fora do vilarejo a terra segue amaldiçoada; a floresta só clareia (`cura_com_era`).
- **Troca de terreno e de objetos por era**: no Renascido, a lama vira calçamento; o poço vira fonte (`TrocaPorEra`); enfeites de `vilarejo.json` (`Enfeite`) aparecem quando a onda chega a eles: lampiões (era 2), estandartes (3), cristais de luz azul (4).
- **Atmosfera**: cinzas na era 1; vagalumes à noite a partir do Despertar; pétalas e borboletas no Renascido; poeira mágica e o dragão na Alta Fantasia.
- **Anúncio de era** (`EventoJogo::EraMudou`) uma vez por virada; os moradores ganham falas por era (`era_min` nos diálogos).
- **Áudio** (`Engine/Audio`): `Sintetizador` puro (testado sem placa de som) gera música generativa e efeitos, sem arquivos. Uma escala, andamento e instrumentação por era, mais uma trilha de perigo na floresta e na cripta. `AudioDevice` liga ao SDL3 (sem dispositivo, o jogo segue mudo). M liga e desliga o som. Os scripts de teste usam `SDL_AUDIODRIVER=dummy`.

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
