# Kingdom of Ashes: The Last Crown

Simulação de vida + gestão de reino + RPG, em pixel art top-down. Você é o último herdeiro de uma
linhagem real e volta a um reino em ruínas para reconstruí-lo. O mundo começa **Dark Fantasy** e evolui,
era a era, até a **Alta Fantasia**.

Engine própria em **C++20 + SDL3 + OpenGL 3.3** (Kingdom Engine), com EnTT, Dear ImGui, nlohmann/json e GoogleTest.

## Versão 1.10

A 2.0 está em andamento (identidade e plano em `docs/VISAO_2.0.md`). A fase 9 trouxe as Crônicas do Reino
(tecla C), os títulos do herdeiro, o mercador viajante, três monumentos e o começo do mistério da Queda das Cinzas.
A fase 10 trouxe as estações que mudam a economia, as noites raras (Lua de Sangue, aurora, meteoros) e o
Festival da Colheita, que cresce com a era.
A fase 11 trouxe as regiões vivas (tecla R): a floresta e as minas têm corrupção, segurança e prosperidade.
A floresta pode ser purificada no Coração da Floresta e virar a Floresta Encantada. Ao sul dela ficam as minas,
com cristal e o Golem de Cinzas, um chefe que se rende e deixa o herdeiro escolher: despedaçar, purificar ou aceitar um pacto.
A fase 12 trouxe o Conselho Real (tecla G): quatro cargos ocupados pelos moradores, cada um com jeito e lealdade
próprios, e casos políticos a cada poucos dias. Cada conselheiro diz o que apoia, e algumas escolhas viram leis.
O jogo também passou a aceitar o mouse: clique para atacar e para escolher nos menus.
A fase 13 trouxe a defesa do reino: hordas avisadas com dias de antecedência atacam o portão leste às 20:00.
A Muralha Leste (reconstruída em 3 níveis) segura as criaturas, as torres atiram flechas, os postos da guarda mandam
soldados, e o herdeiro luta no portão. Sem ele, as defesas lutam sozinhas; o que passar saqueia a praça.
A fase 14 trouxe a Guilda dos Aventureiros (com o reino estável): seis aventureiros com nome partem em expedições de
alguns dias para as ruínas da antiga Valdren e voltam com recursos, feridos, desaparecidos ou com relíquias, que,
expostas na guilda, ajudam o reino; algumas são amaldiçoadas, e Maelis sabe purificá-las.
A fase 15 trouxe a vida cotidiana: a horta do herdeiro ao lado da casa (plantar, regar todo dia e colher; cada
semente tem suas estações), a pesca (E de frente para a água, espere a mordida e puxe), o fogão na fogueira da praça
(sete receitas que curam, dão força até dormir ou alegram o povo), galinheiro e curral com galinhas e vacas que dão
ovo e leite, e presentes: na conversa, F dá ao morador o prato ou ingrediente de que ele mais gosta.
A fase 16 trouxe o castelo dos reis e o brasão da linhagem: as ruínas no alto do vilarejo se reconstroem em quatro
níveis (Torre de Menagem, Salão Real e Castelo Real, os dois últimos esperando as eras), cada nível abre vaga para
uma ala (tesouro, quartel, jardim, celeiro, biblioteca ou salão de banquetes), e no painel do castelo se monta o
brasão: forma, divisão, cores, símbolo e lema. As cores aparecem nos estandartes da praça e nas bandeiras do castelo.
A fase 17 arrumou as fundações (depois da auditoria da versão 1.8): o save é gravado inteiro de uma vez e em segundo
plano, cada save guarda a própria sorte, um save real de cada versão antiga é testado, o código do mundo foi dividido
por assunto, os painéis viraram uma pilha só, os avisos foram para o canto (N relê as mensagens), as regras das obras
saíram do código para os dados, e o balanceamento foi refeito com um jogador automático que usa todos os sistemas:
o celeiro tem limite (a comida que sobra estraga), as obras alegram cada vez menos e o vilarejo apertado reclama.
A fase 18 deu nome ao povo e trouxe o clima: cada habitante tem família, profissão (que rende para o reino), um jeito
de ser e um desejo; desejos realizados viram páginas nas Crônicas, e quem cansa de esperar vai embora (e o relatório
diz quem e por quê). E perto de um aldeão abre a ficha dele, e P mostra o povo inteiro. Cada dia tem o seu tempo:
a chuva rega a horta, a neve deixa todo mundo mais lento, a neblina esconde a horda e a tempestade traz raios.

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
| clique esquerdo / J | atacar (o clique vira o herói para o cursor) |
| mouse | escolher e confirmar nos menus e painéis (clicar fora fecha) |
| H | comer (o prato que mais cura, ou 3 de comida) |
| F | na conversa, dar presente; no fogão, comer o prato escolhido |
| B / K / R / G / L / C | obras / reino / terras do reino / Conselho Real / diário / Crônicas |
| N | mensagens (os últimos avisos, com dia e hora) |
| P | o povo (todos os habitantes; clique para a ficha) |
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
