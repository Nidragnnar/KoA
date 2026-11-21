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

## Fase 8 — finalização (versão 1.0)

- **Salvamento completo**: 3 espaços (`Save_001` a `Save_003`); o título mostra Continuar (o save mais recente pelo `metadata.json`), Novo Jogo e Carregar, com resumo de cada espaço (dia, era, horas jogadas, data) e confirmação antes de substituir. Objetos esgotados guardam o dia (`dia_esgotado`); a versão do save continua 4 (o campo novo é opcional).
- **Interface**: menu de Opções (volume da música e dos efeitos, tela cheia) gravado em `config.json`, ao lado da pasta Saves; tela de Controles; ambos no título e na pausa. Guia "Próximo passo" no HUD durante os primeiros dias (`World::proximoPasso`), até a primeira ida à floresta. Ícone da janela e do executável gerados pelo AssetGen (`Assets/UI/icone.png`, `Game/icone.ico`).
- **Balanceamento** (`Tools/Balanceamento`, resultado em `docs/BALANCEAMENTO.md`): um jogador automático joga 60 dias em ritmos diferentes. A simulação mostrou pedra como gargalo e ouro sobrando, então: árvores e rochas renascem (3 e 4 dias), o mercado vende lotes de pedra e madeira (mais barato a cada nível), o ouro diário da taverna e do mercado caiu, a horta rende menos, os limiares de era subiram para 8/22/40 e a Alta Fantasia exige o Cavaleiro Caído derrotado (`Kingdom::magiaLiberta`).
- **Otimização**: o `Application` registra no log, a cada 10 s, o FPS e o tempo de CPU por quadro (média e pior caso, sem a espera da sincronia vertical).
- **Testes**: 79 (renascimento, mercado, chefe e Alta Fantasia, guia), mais o roteiro ponta a ponta que agora passa pelo título com espaços de save.

## Fase 9 — Crônicas e memória do reino (versão 1.1)

O plano da 2.0 está em `docs/VISAO_2.0.md`.

- **Marcos e Crônicas** (`World::marcar`): todo acontecimento importante vira um marco único (`casa_reconstruida`, `construiu:taverna`, `missao:limpar_estrada`, `pista:...`, `titulo:...`). Se houver página em `Data/Lore/cronicas.json` (própria ou do grupo, `construiu:*`), ela entra em `World::cronicas` com dia e era. A cada marco os títulos são conferidos e os fragmentos do mapa são atualizados.
- **Lore** (`Game/Lore`): definições de títulos (requisitos combináveis: marcos, obras, tipos de obra, abates, população, era, fragmentos), fragmentos (região, tile, marco para aparecer, texto) e itens de loja; `preencher` troca `{nome}` e `{titulo}`.
- **Fragmentos no mapa**: componente `Pista`; `atualizarPistas` mantém na região ativa só os visíveis e ainda não lidos. Interação nova `TipoInteracao::Ler` e evento `LerPista`, que abre a leitura em papel.
- **Visitante**: um morador com `visita` em `npcs.json` só aparece depois do marco e a cada N dias; com `loja`, a conversa termina na banca (`World::comprar`). Não conta como morador na população.
- **Monumentos**: construções com `monumento` e `requer` (marco). A inscrição (`World::inscricao`) é montada com o estado real da campanha.
- **Save v5**: arquivo novo `chronicles.json`. Saves v1 a v4 são migrados, e as Crônicas são reconstruídas a partir do que já tinha acontecido (páginas sem data). Testado numa cópia do save real (v3).
- **Testes**: 92 (13 novos da fase 9) e o roteiro com as etapas 20 a 23 (Crônicas, leitura, mercador e carta, memorial).

## Fase 10 — estações e acontecimentos (versão 1.2)

- **Estações** (`Game/Kingdom/Estacoes.h`, `Data/World/estacoes.json`): o relógio passou a ter estações de 7 dias, para o ano inteiro caber numa partida. Cada estação traz um `ModificadorDia` (hortas, consumo, ouro das construções, felicidade) que `calcularResumo`/`simularDia` aplicam, mais força das criaturas e o intervalo do mercador.
- **Acontecimentos raros**: `World::eventoDoDia` sorteia pelo número do dia (hash), com requisitos de marco e era; por isso nada disso vai para o save. A Lua de Sangue tinge `luzAmbiente`; aurora, estrelas cadentes e fogos são desenhados na cena (os fogos e as estrelas numa camada de partículas própria, por cima da escuridão da noite). A estrela caída é um `Saqueavel` com `Meteorito`, recriada ao carregar enquanto não for recolhida (o marco `meteorito:<dia>` guarda isso).
- **Festival**: dia fixo no calendário; os moradores trocam a rotina por um lugar em volta da fogueira (`itemAtual = -2`), aldeões passeiam na praça, os `Enfeite` com `festival` só aparecem na hora da festa. Falas com condição `festival` e `evento`.
- **Balanceamento**: a simulação mostrou que um bônus de viajantes na primavera virava bola de neve (Renascido no dia 9 em vez de 18); ficou só +1 de ânimo, e o ritmo das eras voltou ao da fase 8.
- **Testes**: 103 (10 novos da fase 10) e o roteiro com as etapas 24 a 29 (festival de dia e de noite, as três noites raras, a estrela recolhida e o inverno).

## Fase 11 — regiões vivas (versão 1.3)

- **Regiões vivas** (`Game/World/Regioes.h`): regras puras e testáveis (`registrarAbate`, `noiteDaRegiao`, `fracaoCriaturas`, `producaoRegiao`) separadas do `World`. Só regiões com `vida` no JSON têm `EstadoRegiao` (corrupção, segurança, prosperidade, última visita). A noite das regiões roda em `World::dormir` antes do renascimento das criaturas, e só para regiões já visitadas. A produção vai para o relatório com a chave `recurso|região`.
- **Transformações**: lista no JSON da região. Vale a primeira cujo marco existe (`purificou:floresta`, `escolha:golem_cinzas:purificar`...). Ela troca nome, penumbra, hostilidade, era visual (`eraNoTile` fora do vilarejo), criaturas afastadas, piso de corrupção, efeito na felicidade (`World::modificadorReino`), chance da Lua de Sangue, rendimento dos veios, produção e enfeites próprios (componente `EnfeiteRegiao`, recriado em `atualizarEspeciais`). Como tudo deriva dos marcos, nada disso precisa de campo novo no save além do estado numérico.
- **Interações novas**: `TipoInteracao::Usar` (componente `Interativo`: o altar e o entulho) e `TipoInteracao::Decidir` (chefe rendido). Saídas podem exigir um marco (`requer`).
- **Chefe com escolha**: `InimigoDef::escolha` aponta para `Data/Lore/escolhas.json`. Com 30% da vida o chefe fica `rendido` (não anda, não fere, não morre) e a cena abre o painel de decisão. Esc adia; E perto dele reabre. `World::escolher` aplica recompensa, item, corrupção da região, os marcos `chefe_<tipo>` (não volta) e `escolha:<id>:<opção>`, e anuncia a transformação.
- **Save v6**: arquivo novo `regions.json`. Saves v1 a v5 são migrados: as regiões começam como no mapa e visitadas no dia carregado (sem abandono acumulado). Testado numa cópia do save real (v3).
- **Testes**: 119 (16 novos da fase 11) e o roteiro com as etapas 30 a 35 (painel R, o rito no Coração, a passagem, as minas, o veio, o golem que se rende, a escolha e as Minas Arcanas).

## Mouse (versão 1.3, depois da fase 11)

- **Engine**: `ke::Input` lê o mouse e converte a posição da janela para a tela interna (640×360), descontando as faixas pretas do letterbox e a densidade de pixels (`definirTela`, chamado pelo `Application` a cada quadro). Cliques e roda valem um passo de lógica, como as teclas. Com a janela de depuração (F1) sob o cursor, o jogo ignora o clique.
- **Jogo**: `Game/UI/Cliques.h` guarda as áreas clicáveis que cada painel registra ao ser desenhado; o passo seguinte consulta (`listaComMouse`: passar seleciona, clicar confirma; `cliqueFecha`: clicar fora ou no rodapé fecha). Assim o mouse usa exatamente o lugar onde a opção apareceu. O clique esquerdo fora dos menus ataca, virando o herói para o cursor.

## Fase 12 — Conselho Real e decisões (versão 1.4)

- **Conselho** (`Game/Kingdom/Conselho.h`): cargos, leis e casos vêm de `Data/Kingdom/conselho.json`. Regras puras (`fatorLealdade`, `efeitoNoCargo`, `lealdadeInicial`) separadas do `World`. O efeito de cada cargo entra em `World::modificadorReino`, junto com as leis; o `ModificadorDia` ganhou impostos, segurança, ouro por dia e madeira/pedra por dia, que `calcularResumo` aplica. O ouro da noite nunca deixa o cofre negativo.
- **Casos**: `World::noiteDoConselho` (em `dormir`) forma o conselho, mexe na lealdade conforme a felicidade e, a cada `dias_entre_decisoes`, traz um caso possível (requisitos de população, era, imposto, marco e lei; casos decididos só voltam se `repetir`). `World::decidir` aplica recursos (gasta até o que houver), felicidade, população, amizade, corrupção de região, lei, imposto e a lealdade de quem apoiou ou não aquela saída, e grava o marco `decisao:<caso>:<saída>`.
- **Save v7**: arquivo novo `council.json` (cargos, lealdade, leis, caso pendente, dia do último caso). Saves v1 a v6 começam sem conselho; ele se forma na manhã seguinte se o reino já tiver as obras.
- **Testes**: 131 (11 novos da fase 12 e 1 do mouse) e o roteiro com as etapas 36 a 38 (mouse na pausa e no ataque, nomear pelo G, o caso que abre depois do relatório e vira lei).

## Fase 13 — Defesa do reino (versão 1.5)

- **Regras** (`Game/Kingdom/Defesa.h`): `forcaHorda`, `composicaoHorda` (rodízio da criatura mais cara para a mais barata enquanto a força der) e `resolverSemHeroi` são puras e testadas sem mundo.
- **Muralha Leste**: uma construção fixa do mapa (`muralha`, 1×7 tiles) que o `World` trata à parte: não conta como obra nem como desenvolvimento no nível 1 (ruínas); de pé, `obstaculos()` tem só os trechos de muro, e o vão do portão (`caixaPortao`) é barreira apenas para a horda. O `WorldRenderer` desenha um trecho por fileira e o portão (inteiro ou quebrado).
- **Batalha**: começa em `World::passo` na hora marcada (`iniciarBatalha`). As criaturas da horda (`Inimigo::horda`) têm o seu próprio comportamento em `atualizarInimigos`: alvo vivo mais perto (herói ou `Soldado`), senão o portão ou a praça; só esbarram na muralha e no portão. Torres e soldados ficam em `atualizarDefesa`/`atualizarSoldados`. A derrota de uma criatura passou para `derrotarInimigo`, usado pela espada, pelas flechas e pelas lanças. O fim (`terminarBatalha`) dá o ouro, grava a página e marca a próxima horda; `resolverHordaSemHeroi` cobre o herdeiro longe, dormindo ou saindo no meio.
- **Save v8**: arquivo novo `defense.json` (dia da horda, número, próxima, vida do portão, notas para a manhã). A batalha em si não se salva: carregada depois das 20:00 do dia marcado, ela recomeça do portão.
- **Testes**: 143 (12 novos da fase 13) e o roteiro com as etapas 39 a 41 (reconstruir a paliçada pelo painel, o aviso dos batedores e a batalha com torre, soldados e o herdeiro).

## Fase 14 — Guilda, expedições e relíquias (versão 1.6)

- **Regras** (`Game/Kingdom/Guilda.h`): `poderAventureiro`, `desfechoPorMargem`, `chanceEmPalavras` e `sorteExpedicao` são puras. A sorte vem de um hash do destino e do dia da partida: o mesmo plano dá o mesmo resultado, e nada de sorteio vai para o save.
- **Guilda**: uma construção comum com `requer: estabilidade` (marco dado de manhã quando o reino está em paz). O nível dela diz quantos aventureiros recruta (`efeito aventureiros`) e quantas relíquias expõe (`efeito reliquias`).
- **Expedição**: `World::partir` valida e cobra a comida; `voltaDaExpedicao` (em `manhaDaGuilda`, chamada por `dormir`) calcula margem, ganhos, relíquia, feridos, sumiço e nível, e escreve no relatório da manhã.
- **Relíquias**: `efeitosReliquias()` soma as expostas (e as maldições não purificadas) e entra em `modificadorReino` (felicidade, ouro, segurança, comida), em `danoArma`/`defesaArmadura` e na chance da Lua de Sangue. Equipar automaticamente compara só o equipamento, sem o bônus das relíquias.
- **Painel da guilda** (abas Expedição, Aventureiros e Relíquias), com teclado e mouse; o menu de obras encolhe a linha quando há muitas obras.
- **Save v9**: arquivo novo `guild.json` (aventureiros, expedição em curso, relíquias). Saves v1 a v8 começam sem guilda.
- **Testes**: 155 (12 novos da fase 14) e o roteiro com as etapas 42 a 45 (construir e abrir a guilda, montar o grupo, a volta da expedição e a maldição revelada).

## Fase 15 — Vida cotidiana (versão 1.7)

- **Dados** (`Game/Kingdom/Cotidiano.h`, `Data/World/vida.json`): culturas, peixes, animais, receitas, presentes e o nome de cada item novo (o `nomeRecurso` procura neles). `sortearPeixe` e `textoPrato` são puras.
- **Horta**: `World::criarCanteiros` cria o solo (`Canteiro`, `Interativo "canteiro"`) e a planta (`PlantaCanteiro`) de cada canteiro; o estado fica em `World::canteiros` (cultura, dias crescidos, dia da rega, maduro). O crescimento acontece em `noiteDoCotidiano`, chamada por `dormir` com a estação de ontem. O `WorldRenderer` desenha a terra como chão (antes de tudo) e a planta na ordem de profundidade.
- **Pesca**: estado `pesca_` no `World` (não vai para o save). Durante a pesca o E e o clique só puxam a linha; o toque que encerra a pesca não fica guardado para relançar.
- **Animais**: `sincronizarAnimais` cria e remove as entidades `Animal` conforme os galinheiros/currais prontos (chave `<obra>#n`, onde fica o carinho do dia); `atualizarAnimais` faz o passeio em volta do terreiro.
- **Cozinha e pratos**: `cozinhar`, `comerPrato` (vida, felicidade e os bônus `buffDano`/`buffDefesa`, que somam em `danoArma`/`defesaArmadura` e zeram ao dormir). O painel do fogão é do `Hud` (botão "comer" no id 500).
- **Presentes**: `presenteParaDar` escolhe o favorito do morador (ou um prato) e `Conversa::presente` mostra a dica "F dar ..." no diálogo (clicável, id 600).
- **Save v10**: arquivo novo `life.json` (canteiros, bônus dos pratos, presente e carinho do dia). Saves v1 a v9 começam com a horta vazia e ganham 4 sementes de nabo.
- **Testes**: 171 (16 novos da fase 15) e o roteiro com as etapas 46 a 50 (horta, pesca com o clique, fogão e banquete, animais e carinho, presente ao Bram).

## Fase 16 — Castelo e brasão (versão 1.8)

- **Castelo**: a decoração `castelo_ruina` virou a construção fixa `castelo` (10×5 tiles, no mesmo lugar). Como a muralha, o nível 1 não conta como obra, nem no desenvolvimento, nem nos títulos. Construções fixas agora limpam o que estiver embaixo ao carregar o mapa.
- **Requisito por nível**: `NivelConstrucao::requer` (marco) e `requerTexto`. `World::requisitoProximoNivel` bloqueia o "Reconstruir" sem cobrar nada; o painel da construção mostra o motivo em vermelho.
- **Alas e brasão** (`Game/Kingdom/Castelo.h`): `DadosCastelo` (alas, peças, tinturas, lemas, brasão inicial), `textoBrasao` (a descrição heráldica), `girarPeca` (o editor) e `validar` (peças que sumiram dos dados voltam à inicial). No `World`: `alas`, `brasao`, `erguerAla`, `efeitosAlas` (entram em `modificadorReino`, em `soldadosDosPostos` e em `margemSemSorte`) e `definirBrasao`.
- **Desenho** (`Game/UI/Brasao.h`): o escudo e o pano são máscaras brancas por forma e divisão (`escudo_<forma>_<divisão>_a/_b`, `pano_<divisão>_a/_b`) tingidas com as cores do brasão; a borda e o símbolo (com contorno) vão por cima. O `WorldRenderer` desenha os estandartes da praça (enfeites `estandarte_*`) e as bandeiras do castelo como peças compostas na ordem de profundidade (`Desenhavel::desenho`). O castelo não passa de 112 px de altura porque fica no alto do mapa.
- **Save v11**: arquivo novo `castle.json` (alas, brasão, se o herdeiro já criou o dele). O nível do castelo vai no `kingdom.json` como o das outras construções. Saves v1 a v10 começam com o castelo em ruínas, sem alas e com o brasão dos reis antigos.
- **Testes**: 181 (10 novos da fase 16) e o roteiro com as etapas 51 a 53 (reconstruir pelo painel, alas pelo teclado e pelo clique, o editor do brasão com teclado e mouse, estandartes e bandeiras).

## Fase 17 — Fundações (versão 1.9)

- **World por assunto**: `Game/World/World.cpp` (núcleo: mapa, passo, interação, eras, a noite) e `World<Domínio>.cpp` para combate, moradores, save, crônicas, regiões, conselho, defesa, guilda, cotidiano e castelo. O que eles dividem (`LADO`, `baseTile`) está em `WorldInterno.h`. A lógica não mudou: só mudou de arquivo.
- **Save**: `SaveSystem::salvar` grava em `<slot>.novo` e troca com `<slot>` por renomeação (o anterior vira `<slot>.bak`); `carregar` termina uma troca interrompida. Os módulos opcionais e a versão em que apareceram estão numa tabela (`MODULOS`). Save v12: `semente` no `metadata.json` (`World::semear`; jogo novo sorteia, o roteiro e os testes seguem com 2026). A cena grava em segundo plano (`std::async`), uma gravação por vez, e espera a última ao fechar.
- **Saves-modelo**: `Tests/fixtures/saves/vNN/Save_001` (v1, v2, v3, v5 a v11, saves reais dos backups) e o teste `SavesModelo.TodasAsVersoesAbremEGravamNaAtual`.
- **Cena**: `enum class Painel` e `pilha_` em `WorldScene`; `abrir`/`fechar` (fechar limpa o conteúdo: id da construção, fragmento, banca, escolha, conversa). O relógio para com a pilha não vazia; o alvo some com a pilha não vazia ou durante a colocação de obra.
- **Mensagens**: os avisos vão para o canto de baixo à esquerda e para `Hud::historico()` (60 mais recentes, com estação, dia e hora); `N` abre o painel.
- **Regras nos dados**: `BuildingDef` ganhou `funcao`, `ruinas`, `contaComoObra`, `janelas`, `acoes` e `fumaca`; `World::funcaoDe`, `porFuncao`, `efeitoMaximo` e `efeitoTotal` respondem pelos dados (nível da forja = `nivel_forja`, dano das torres = soma de `dano_torre`...). `Kingdom::defsEmRuinas` vem dos dados. Um teste garante que nenhum `defId == "..."` volte ao código do jogo.
- **Colisão**: `World::obstaculos()` devolve um cache refeito quando a assinatura muda (colisores, região, obras e níveis) ou quando algo marca `sujarObstaculos()` (coleta, renascimento, noite, carregamento); `calcularObstaculos()` é o cálculo completo que os testes comparam.
- **Balanceamento**: `Tools/Balanceamento/JogadorAutomatico` (usado pela ferramenta e pelo teste `Balanceamento.RitmoMedioDentroDasFaixas`) e `alarmes()` com as faixas. Regras novas na `Economia`: capacidade do celeiro (`ModificadorDia::diasCeleiro`, 12 dias, +8 com o Celeiro Real) e 40% do excesso estragando por noite (`comida|estragou` no relatório); felicidade das obras com teto (`FELICIDADE_DAS_OBRAS`); aglomeração acima de 40 moradores.
- **Testes**: 191 (10 novos da fase 17) e o roteiro com a etapa 54 (mensagens e pilha de painéis).

## Fase 18 — Povo com nome e clima (versão 1.10)

- **Regras puras**: `Game/Kingdom/Povo.h` (`gerarHabitante`, `efeitosDoPovo`, `misturar` para sorteios fixos) e `Game/Kingdom/Clima.h` (`tempoDoDia`: o tempo vem do dia e da semente da partida, então não vai para o save).
- **No mundo** (`Game/World/WorldPovo.cpp`): `World::povo` (as pessoas), `ajustarPovo` (depois da economia da noite: chegadas em famílias de 2 a 4, desejos já cumpridos sorteados de novo; partidas escolhidas por quem mais esperou), `manhaDoPovo` (depois da horta crescer: a chuva rega, os desejos são conferidos, quem desiste vai embora), `desejoCumprido`, `tempoAqui` (céu limpo nas regiões com `"subterranea": true`). O `modificadorReino` soma o que o povo rende e o tempo do dia; `ModificadorDia::comidaDia` é a comida do povo.
- **Aldeões**: cada um na rua é uma pessoa (`Aldeao::habitante`, no máximo 16 andando); `TipoInteracao::Habitante` e o evento `AbrirFicha`. Painéis `Ficha` e `Povo` na pilha da cena.
- **Clima na tela**: chuva e neve desenhadas na tela por fórmula (sem estado), a tinta da cena multiplicada pela luz do tempo, a névoa do renderer somada à neblina, e raios na tempestade.
- **Eras**: `Kingdom::eraConquistada` (dos marcos `era:N`) impede a era de voltar atrás; o Reino Renascido pede 26 de desenvolvimento.
- **Save v13**: arquivo novo `people.json` (pessoas e o próximo id). Saves v1 a v12 ganham o povo ao carregar, do tamanho da população, pela sorte do save.
- **Testes**: 204 (13 novos da fase 18) e o roteiro com a etapa 55 (chuva com 46 pessoas a 120 quadros por segundo, ficha, lista do povo e neve).

## Fase 19 — Reputação, facções e intriga (versão 1.11)

- **Regras puras**: `Game/Kingdom/Faccoes.h` (`Reputacao` com três eixos, `FaccaoDef`, `TratadoDef` e `EfeitosTratado`, `ReacaoMarco`; `DadosFaccoes::reacaoA` soma todas as reações que casam com um marco, aceitando `*` no fim; `textoTratado`). Os dados vêm de `Data/Kingdom/faccoes.json`.
- **No mundo** (`Game/World/WorldFaccoes.cpp`): `relacoes`, `tratados`, `reputacao`, `desvios` e `suspeito` no `World`. `marcar()` chama `reagirAoMarco` depois de inserir um marco novo: é o único gancho dos acontecimentos, então nenhum sistema antigo precisou saber das facções. `noiteDasFaccoes` roda depois da noite do conselho (fama pelo imposto, deriva de 2% para o que cada povo gosta, emissários, sabotagem do Culto hostil, tratados que rendem ou caem, choque dos rivais) e termina em `noiteDaIntriga`.
- **Efeitos dos tratados** entram onde o jogo já somava bônus: `modificadorReino` (pedra, ouro, segurança, felicidade), soldados do portão e força da horda (`WorldDefesa`), força das expedições (`WorldGuilda`) e dano da arma (`WorldCombate`). O cristal por dia vai direto ao inventário na noite.
- **Missões de facção**: `MissaoDef::faccao` e `relacao` (sem `npc`); `aceitarMissaoFaccao` inicia pelo painel; `atualizarMissoes` sobe a relação ao cumprir.
- **Caso manual**: `DecisaoDef::manual` (o sorteio dos casos nunca escolhe; só a intriga abre `desvio`); `decidir` chama `resolverDesvio`.
- **Save v14**: arquivo novo `factions.json` (relações, tratados, reputação, desvios, suspeito). Saves v1 a v13 começam nas relações iniciais e refazem relações e fama pelos marcos que já têm (os tratados não, porque não existiam).
- **Interface**: `Painel::Vizinhos` na pilha da cena (tecla T); `Hud::acoesVizinhos` dá a ordem das linhas (tratados e missões de cada povo), a mesma do teclado e dos ids de clique.
- **Testes**: 213 (9 novos da fase 19) e o roteiro com a etapa 56 (T abre os vizinhos, E assina a Rota da Pedra, o clique aceita a missão da Ordem).

## Fase 20 — Mercado vivo, Livro do Reino e acessibilidade (versão 1.12)

- **Regras puras**: `Game/Kingdom/Mercado.h` (`BemDef`, `EncomendaDef`, `DadosMercado`, `EstadoMercado`; `precoUnitario`, `sorteDoDia`, `precoCompra`/`precoVenda` arredondando sempre a favor do mercado, `negociar` e `noiteDoMercado`). `lerMercado` recusa dados em que o impacto de um lote vença a taxa (o que daria lucro comprando e vendendo).
- **No mundo** (`Game/World/WorldMercado.cpp`): `World::mercado`, `bensNoMercado`, `podeNegociar` (o motivo vira a dica do painel), `comprarNoMercado`/`venderNoMercado`, `encomendar`, `tendencia`. `marcar()` chama `reagirMercado` junto de `reagirAoMarco`; `manhaDoMercado` (depois da virada do dia) recupera a pressão, grava o preço da manhã e entrega as encomendas na visita do Corvin. O mercado antigo (`precoLote`/`comprarLote`) saiu; a construção agora tem a ação `mercado`.
- **Alas**: `AlaDef::acao` (`AcaoAla` em `Castelo.h`), `World::podeAgirAla`/`agirAla`/`manhaDasAlas`; `guardaTreinada` entra em `soldadosDosPostos` e zera depois da horda.
- **Áudio**: `ke::Ambiente` e `Sintetizador::definirAmbiente`; os sons de fundo são vozes de efeito agendadas por lugar (o volume dos efeitos manda neles). `sonsDeAmbiente()` conta o que tocou, para os testes.
- **Entrada**: camada de ações no `ke::Input` (`mapear`, `pressionouAcao`, `segurandoAcao`, `ultimaPressionada`). `Game/UI/Teclas.h` lista as ações e as teclas padrão; `Contexto::aplicarOpcoes` aplica o `config.json`. A cena pergunta pela ação (as setas, Espaço e as teclas dentro dos painéis continuam fixas). A tecla do som (no `Application`) também segue a ação.
- **Livro do Reino**: `ABAS_LIVRO` na cena liga cada aba a um `Painel` e a uma ação; `abrirAba`, `abaAberta` e `entradaLivro` (Tab e o clique na faixa). O HUD desenha a faixa (`abasLivro`, ids 7100+) e o botão (id 7000).
- **Interface**: `Hud::areaDica`/`desenharDica` (a dica vai por cima de tudo), `definirEscalaTexto` (texto grande nas conversas, avisos e dicas), `marcaSimNao` (✓/✗ desenhados com pixels). `Opcoes` ganhou `teclas`, `textoGrande` e `velocidadeTexto`.
- **Save v15**: arquivo novo `market.json` (pressão, histórico, negócios do dia, encomendas, totais). As ações das alas foram para o `castle.json` (campos novos com padrão). Saves v1 a v14 começam com o mercado calmo.
- **Testes**: 224 (11 novos: 9 do mercado e das alas e 2 da engine, ambiência e teclas) e o roteiro com a etapa 57 (mercado pela tecla, compra e venda, abas pelo clique e pelo Tab, o botão do Livro, texto grande com dica e a troca de tecla nos Controles).

## Fase 21 — Linhagem (versão 1.13)

- **Regras puras**: `Game/Kingdom/Linhagem.h` (`TracoLinhagem`, `EfeitosTracos`, `ConjugeDef`, `DadosLinhagem`; `Membro` e `Linhagem` são o que vai para o save; `tracosDaMaioridade` sorteia pela semente da partida e pelo id do filho, sempre igual).
- **No mundo** (`Game/World/WorldLinhagem.cpp`): `World::linhagem`, `herdeiro()`, `efeitosHerdeiro()`, `podeCasar`/`casar`, `definirTutor`, `podeSuceder`/`passarCoroa`, `noiteDaLinhagem` (depois da virada do dia: nascimento, maioridade, velhice e a morte natural). O "herdeiro" deixou de ser fixo: o jogador é sempre o membro `linhagem.atual`, e `aplicarTracos` acerta a vida máxima. Os efeitos dos traços entram onde o jogo já somava bônus: `danoArma`, `modificadorReino` (felicidade e consumo), `margemSemSorte` (expedições), `taxaMercado` (nunca abaixo de 5%, para o mercado continuar sem lucro infinito) e a chance de descobrir um desvio.
- **Salto**: a sucessão soma 15 a `linhagem.anosSaltados` (o relógio do jogo não muda, para nada que depende do dia quebrar); `anoDoReino()` mostra o ano da história.
- **Crônicas**: `EntradaCronica::reinado`; o painel abre um cabeçalho a cada reinado depois da primeira sucessão.
- **Interface**: `Painel::Familia` (tecla Y e aba "Família" do Livro), `Hud::painelFamilia`/`acoesFamilia` (linhas `c:`, `t:`, `s:`). Passar a coroa pede o segundo E. Com um painel aberto, as dicas do HUD ficam por baixo dele.
- **Save v16**: arquivo novo `family.json`; páginas das Crônicas ganham `reinado`. Saves v1 a v15 viram a primeira geração (o fundador, Aren, Valente).
- **Testes**: 231 (7 novos: dados, herança, casamento, filhos e tutor, sucessão sem perder o reino, morte natural e save v16) e o roteiro com a etapa 58 (casar pelo painel, nascimento, maioridade com o tutor, sucessão com confirmação e as Crônicas por reinado).

## Desvios (e por quê)

| Documento | Feito | Motivo |
|---|---|---|
| vcpkg | CMake FetchContent com versões fixas | o vcpkg não está instalado; o FetchContent é o mesmo esquema que já funciona no Roguelike e não exige instalar nada |
| Box2D | não incluído | o documento diz "quando necessário"; a colisão AABB × tiles basta para o 0.1 |
| GoogleTest | GoogleTest v1.17.0 | igual |
| Movimento em 4 direções | sprites em 4 direções, mas com movimento diagonal | igual ao jogo de referência; a direção do sprite segue o eixo dominante. Fácil de travar em 4 se preferir |
| Era muda tiles, prédios, NPCs... | tiles, prédios, objetos e enfeites mudam por era (fase 7); os moradores ainda não | as versões por era vêm da recoloração da arte; os moradores usam sprites procedurais |
| Assets/ com sprites prontos | arte do NidraIA para terreno, vegetação e construções (`Assets/Arte`); herói, moradores e criaturas ainda procedurais | qualquer sprite pode ser trocado pondo um PNG do mesmo tamanho em `Assets/Arte`, sem mexer no código |

## Versões das bibliotecas

SDL 3.2.30 · glm 1.0.1 · EnTT 3.15.0 · nlohmann/json 3.12.0 · Dear ImGui 1.92.6 · GoogleTest 1.17.0 · stb (commit 4e9b368) · glad (OpenGL 3.3 core, no repositório)
