# Kingdom of Ashes 2.0 — identidade e plano

O jogador não está só construindo uma cidade: está **restaurando uma civilização apagada da história**.
A 1.0 provou a base (reino, vida, RPG, eras). A 2.0 dá ao jogo a sua identidade própria.

## Identidade (decidida em 15/11/2026)

| Pergunta | Escolha | O que significa na prática |
|---|---|---|
| Liberdade | **Híbrido** | Um fio principal, o mistério da Queda das Cinzas, revelado aos poucos conforme o reino cresce. A ordem é livre: construir, explorar e conversar avançam a história, cada um a seu modo. |
| Combate | **Ação + defesa integrada** | O herdeiro luta em tempo real, como hoje. Ataques ao reino chegam avisados com dias de antecedência e se resolvem com muralhas, torres, soldados automáticos e o herdeiro no portão. Nada de comandar tropas. |
| Administração | **Equilibrada** | Conselho, política e regiões com estado próprio, mas sempre com poucos números legíveis (no máximo 5 por painel) e decisões com consequência visível no relatório da manhã. Sem planilhas e sem gerenciar funcionário por funcionário. |

### Princípios que valem para toda fase

1. **O começo é sombrio.** Cinzas, ruína e pouca gente; a esperança é conquistada.
2. **O mundo lembra.** Toda conquista vira um marco salvo e aparece em algum lugar: numa fala, nas Crônicas, num monumento ou numa mudança no mapa.
3. **Nem tudo é guerra.** A vida cotidiana (conversar, cultivar, festejar) pesa tanto quanto lutar.
4. **Escolhas sem resposta certa.** Decisões favorecem uns e prejudicam outros.
5. **Cada fase é jogável e testada** antes da próxima: testes automáticos, roteiro com capturas e pacote.

## O que a 1.0 já tem e cada ideia aproveita

| Ideia | Base existente |
|---|---|
| Memória do reino, Crônicas, títulos | marcos permanentes no save (`quests.json`), lembranças dos moradores, falas por era e por nível da casa |
| Chefes que mudam o mundo | o Cavaleiro Caído libera a Alta Fantasia (`magiaLiberta`) |
| Regiões que evoluem | `cura_com_era` da floresta; regiões com objetos salvos |
| Eras do reino | 4 eras em onda, música e enfeites por era |
| Construções com função | 8 obras com produção, felicidade e segurança |
| Estações | o relógio já conta estações |
| Conselho e decisões | moradores com profissão e amizade; impostos no painel K |

## Plano em fases

### Fase 9 — Crônicas e memória do reino (feita, versão 1.1)
- **Crônicas do Reino** (tecla C): cada acontecimento vira um marco salvo; os que têm página (`Data/Lore/cronicas.json`) entram no livro com dia e era. Saves antigos ganham as páginas do que já tinha acontecido, sem data.
- **Títulos do herdeiro** (`Data/Lore/titulos.json`): Herdeiro Exilado → Senhor das Cinzas → O Restaurador → Protetor dos Povos → Libertador da Magia → Guardião da Memória → Rei dos Sete Estandartes. Aparecem no HUD, no painel K, num anúncio e nas falas (`{titulo}`).
- **O mundo reage:** falas com condição `marco`, `sem_marco` e `pistas_min`; a estrada limpa traz **Corvin, o mercador viajante** (a cada 3 dias, com banca na praça).
- **Monumentos:** Memorial das Cinzas (depois da casa), Jardim dos Heróis (era 1) e Estátua do Herdeiro (depois do Cavaleiro). A inscrição de cada um mostra fatos reais da campanha.
- **Começo do mistério:** 6 fragmentos da Queda (`Data/Lore/pistas.json`): no castelo, no cemitério, no poço (era 1), na floresta, na cripta (depois do Cavaleiro) e uma carta vendida pelo mercador. Juntos, revelam o primeiro ato: o rei Aldric abriu a Torre do Selo, e a Chama consumiu o reino.

### Fase 10 — Estações e acontecimentos (feita, versão 1.2)
- **Estações de 7 dias** (o ano tem 28), definidas em `Data/World/estacoes.json`: primavera com ânimo (+1 de felicidade), verão com comércio (+25% de ouro das construções e o mercador a cada 2 dias), outono com colheita (hortas +50%), inverno com escassez (hortas a 40%, -3 de felicidade, criaturas +30%). O painel K mostra o efeito da estação; o relatório da manhã avisa quando o inverno está chegando.
- **Acontecimentos raros** (no máximo um por noite, sorteado de forma fixa pelo dia): Lua de Sangue (noite vermelha, criaturas mais fortes com o dobro de ouro), Aurora Mágica (céu dançando, +6 de felicidade) e Chuva de Meteoros (uma estrela cai perto do vilarejo e vale 40 de ouro). O HUD avisa de manhã ("Esta noite: ...").
- **Festival da Colheita** (último dia do outono, 17:00 às 23:00): o povo larga a rotina e vai para a praça; conversar vale amizade em dobro; no dia seguinte, +10 de felicidade. A festa cresce com a era: lanternas e fogueira (cinzas), barracas (Despertar), bandeirolas e banquete (Renascido), fogos (Alta Fantasia). Cada festival ganha uma página nas Crônicas com o tom da era.
- Primeiro verão, outono e inverno, cada acontecimento raro e cada ano novo também entram nas Crônicas.

### Fase 11 — Regiões vivas (feita, versão 1.3)
- **Corrupção, segurança e prosperidade** na floresta e nas minas (`vida` em `Data/World/<região>.json`; a cripta é calabouço e fica de fora). Cada criatura vencida tira 1,5 de corrupção e põe 6 de segurança. À noite a segurança cai, a corrupção sobe 2 (5 se a região está há 3 dias sem visita) e a prosperidade persegue a terra livre e rende recursos no relatório da manhã. Quanto mais corrupção, mais criaturas renascem (um terço com a terra limpa). Painel **R** (Terras do Reino): três barras e o que a região rende por noite.
- **Purificação:** o Coração da Floresta (altar nas ruínas da mata) aceita o rito com a corrupção abaixo de 25. A floresta vira a **Floresta Encantada**: clara, em paz, com a cara da Alta Fantasia, sem sombras nem esqueletos, e rende madeira e frutos.
- **Região nova:** as **Minas Abandonadas**, ao sul da floresta, atrás de uma passagem desabada (escorada com 15 de madeira). Têm veios de **cristal** (recurso novo, usado na Lâmina e na Couraça de Cristal da forja nível 3), aranhas e esqueletos.
- **Abandono:** terra sem visita piora a cada noite. Acima de 85 é **tomada** (criaturas a mais pelas bordas, aviso no relatório e página nas Crônicas). A Floresta Encantada abandonada perde a cor e as sombras voltam, e basta voltar e limpar.
- **Chefe com escolha:** o **Golem de Cinzas** se rende com 30% da vida (`Data/Lore/escolhas.json`). *Despedaçar* rende ouro e pedra e reabre as Minas de Valdren. *Purificar* só fica disponível para quem fez o rito da floresta: o golem vira o Guardião de Cristal e as minas viram as **Minas Arcanas**, ricas em cristal. *O pacto* dá a Lâmina da Chama e minas que rendem cristal e ouro, mas o povo perde 4 de felicidade, a Lua de Sangue vem com o dobro de chance e a corrupção das minas nunca baixa de 50.
- Missões novas: O Coração da Floresta (Maelis), As Minas de Valdren (Bram) e O Gigante das Minas (Oswin). Falas que reagem à purificação, às minas, ao cristal, à terra tomada e ao pacto.
- Fica para depois: o Cavaleiro Caído continua só com "derrotar", porque a história da Alta Fantasia depende dele.

### Fase 12 — Conselho real e decisões (feita, versão 1.4)
- **Conselho Real** (tecla **G**, `Data/Kingdom/conselho.json`): forma-se quando o reino tem 4 obras de pé. São 4 cargos: Marechal (segurança), Mestre da Moeda (impostos), Intendente do Celeiro (comida) e Mestre das Obras (madeira e pedra por noite). Cada morador tem jeito maior ou menor para cada cargo (Oswin nasceu Marechal, Bram é das Obras, Tilda é do Celeiro, Edda é da Moeda). A/D escolhe e E nomeia. Quem perde o cargo fica magoado (-20 de lealdade).
- **Lealdade** (palavra e barra, sem número): começa pela amizade. Sobe ao conversar (+2 por dia), com o povo feliz e quando o herdeiro segue o conselho da pessoa (+8); cai quando ele decide contra (-6) e com o povo infeliz. Com 50 ou mais, o conselheiro rende tudo; entre 25 e 49, rende 60%; abaixo de 25 fica descontente, não rende nada e não opina. O relatório da manhã avisa.
- **Casos políticos** a cada 3 dias, sorteados entre os possíveis: lenhadores pedem proteção, refugiados no portão, nobre pede isenção, briga por terra, protesto contra o imposto alto (pode voltar), pregadores das cinzas e a exclusividade de Corvin. Cada um tem 3 saídas, com o que custa e o que rende escrito, e a fala de cada conselheiro embaixo da saída que ele apoia. O caso abre depois do relatório da manhã; Esc deixa para depois e ele espera no G.
- **Leis** que valem todo dia: patrulha da estrada, isenção dos Vael, pregadores tolerados e exclusividade de Corvin.
- Páginas nas Crônicas (o conselho, cada nomeação, cada decisão) e falas de quem assume o cargo natural.
- Também nesta versão: o mouse ataca (virando para o cursor) e escolhe nos menus.
- Fica para depois: conselheiro descontente que **esconde coisas** (desvia ouro, mente no relatório) e cargos novos com a era.

### Fase 13 — Defesa do reino (feita, versão 1.5)
- **Aviso de horda** (`Data/Kingdom/defesa.json`): a partir do dia 10, com 12 moradores, os batedores avisam no relatório da manhã que uma horda ataca o portão leste em 3 dias (4 com um Marechal que não esteja descontente), às 20:00. O HUD conta os dias. As próximas vêm 9 a 12 dias depois de cada batalha. A força cresce a cada horda e com a corrupção da floresta, e é maior em noite de Lua de Sangue.
- **Muralha Leste**: fixa na entrada da estrada, começa em ruínas e se reconstrói no painel (E perto do portão): Paliçada, Muralha de Pedra e Muralha Fortificada (portão com 150, 300 e 500 de vida). O muro segura todos; o portão deixa o herdeiro e o povo passarem, mas segura as criaturas até a vida dele acabar.
- **Torres** (agora até 3) atiram flechas nas criaturas ao alcance (6, 10 ou 16 de dano por nível). **Postos da guarda** mandam 2, 3 ou 4 soldados ao portão (mais 1 com o Marechal), que lutam com lança e podem cair feridos.
- **Batalha no portão**: as criaturas marcham pela estrada, golpeiam o portão e, se ele cair, vão à praça. Cada uma que chega leva comida, ouro e ânimo. O herdeiro luta junto; vencida a horda, o povo dá ouro por criatura. Se o herdeiro estiver longe, dormir ou sair no meio, as defesas lutam sozinhas (a conta compara portão, torres, soldados e segurança com a força da horda).
- **Reparos**: o portão danificado se repara no painel da muralha (pedra e madeira). Um Mestre das Obras contente conserta 30% do estrago por noite.
- Páginas nas Crônicas (cada horda, cada nível da muralha) e falas do Oswin.
- Fica para depois: hordas por outras entradas, muralha em volta do vilarejo inteiro e criaturas que miram construções.

### Fase 14 — Guilda, expedições e relíquias (feita, versão 1.6)
- **Estabilidade**: com felicidade 55+, segurança para todos e 14 moradores, o relatório da manhã avisa que aventureiros procuram Valdren, e a **Guilda dos Aventureiros** passa a aparecer no B (`Data/Kingdom/guilda.json`). Três níveis: 3, 5 e 6 aventureiros; 1, 2 e 3 relíquias expostas.
- **Seis aventureiros com nome**, cada um com força, especialidade (combate, exploração, saber) e nível (sobe a cada expedição que dá certo).
- **Expedições** (E na guilda → "Abrir a guilda"): uma por vez, de 1 a 3 aventureiros, gastando 2 de comida por dia por pessoa. Cinco destinos: Ruínas do Vale, Biblioteca Afundada, Pântano das Cinzas, Fortaleza de Hadrian e a Torre do Selo (só com o mapa dela). O painel mostra a chance em palavras ("Boas chances", "Arriscado", "Muito arriscado", "Suicida"). O resultado sai no relatório da manhã da volta: triunfo (ganhos × 1,5 e nível), sucesso, a duras penas (metade, feridos) ou desastre (nada, alguém desaparece; às vezes ele volta dias depois).
- **Relíquias** (11): cada uma com um texto da antiga Valdren e um efeito quando exposta (felicidade, segurança, ouro, comida, dano, defesa). Três são amaldiçoadas: o custo só aparece quando expostas. Maelis purifica com o rito da floresta e 3 cristais.
- Páginas nas Crônicas (estabilidade, guilda, expedições, relíquias, maldições, desaparecidos). A da Torre do Selo deixa a próxima pergunta: a Chama não escapou, alguém a levou.
- Fica para depois: mais de uma expedição ao mesmo tempo, aventureiros que morrem, vender relíquias ao Corvin e o segundo ato do mistério.

### Fase 15 — Vida cotidiana (feita, versão 1.7)
- **Horta do herdeiro** (`Data/World/vida.json`): quatro canteiros fixos ao lado da casa. E num canteiro vazio planta a semente da estação (a de que há mais), E de novo rega (uma vez por dia), e de manhã cresce um dia o que foi regado ontem. Nabo (2 dias; primavera a outono), trigo (3; primavera e verão) e abóbora (4; verão e outono); fora da estação, o frio segura a planta, e no inverno nada se planta. O Corvin vende sementes; o jogo começa com 4 de nabo.
- **Pesca**: E de frente para a água lança a linha (a boia cai até 2 tiles adiante). A mordida vem em 1,5 a 4 segundos e dura 0,8: E ou clique pesca; cedo demais assusta, andar recolhe. Peixe comum, truta (floresta, primavera a outono) e o raro peixe dourado (página nas Crônicas).
- **Fogão** (E na fogueira da praça ou "Cozinhar" na casa): sete receitas. Sopa, peixe assado e pão curam; omelete dá defesa e o ensopado dá dano até dormir; o Banquete da Colheita é servido ao povo (+6 de felicidade). O H come o prato que mais cura antes da comida comum.
- **Animais**: galinheiro (3 e 5 galinhas) e curral (2 e 3 vacas). Cada bicho come 1 de comida à noite e dá ovo ou leite; com carinho no dia (E perto dele), dá dois.
- **Presentes**: na conversa, F dá o favorito do morador (+100 de amizade) ou qualquer prato (+30), uma vez por dia por morador.
- Missão nova da Tilda ("Mãos na Terra": a primeira colheita) e páginas nas Crônicas (primeira colheita, peixe dourado, galinheiro, curral, banquete, presentes).
- Fica para depois: regador e canteiros que se compram, vara de pesca melhor, receitas descobertas, vender pratos e o mercado comprando a colheita.

### Fase 16 — Castelo e brasão (feita, versão 1.8)
- **O castelo dos reis** deixou de ser enfeite: é uma construção fixa do mapa (como a casa e a muralha), em quatro níveis. Ruínas (não contam como obra), **Torre de Menagem** (a torre principal e o portão de pé, a primeira bandeira), **Salão Real** (muralhas inteiras, telhados, janelas acesas à noite; pede o Reino Renascido) e **Castelo Real** (pedra branca, cúpulas azuis e a torre do meio; pede a Alta Fantasia). Cada nível dá segurança, felicidade e, do Salão em diante, ouro.
- **Alas** (`Data/Kingdom/castelo.json`): cada nível a partir do 2 abre uma vaga (1, 2 e 3). Seis alas, cada uma erguida uma vez: Sala do Tesouro (+15% de impostos, +3 de ouro), Quartel Real (+8 de segurança e um soldado a mais no portão), Jardim Real (+5 de felicidade), Celeiro Real (+20% de comida), Biblioteca Real (+1 de força nas expedições) e Salão de Banquetes (mais famílias chegando).
- **Brasão da linhagem** (E no castelo → "Brasão da linhagem"): forma do escudo (4), divisão do campo (6: pleno, partido, cortado, esquartelado, banda, chefe), duas cores entre oito tinturas, símbolo (8: coroa, leão, dragão, chama, árvore, espada, torre, lua), cor do símbolo e lema (6). O painel descreve o brasão como os arautos ("Partido de azure e ouro, um dragão de prata") e mostra o pano.
- As cores e o símbolo aparecem nos **estandartes da praça**, nas **bandeiras do castelo** e no painel do reino (K). O primeiro brasão, cada nível do castelo e cada ala viram páginas nas Crônicas.
- Fica para depois: a capa do herdeiro nas cores da linhagem, o nome da casa escolhido pelo jogador, interiores do castelo e a sala do trono como lugar para receber os casos do conselho.

### Fase 17 — Fundações (feita, versão 1.9)
Saída da auditoria da versão 1.8 (problemas E1 a E11). Nada novo para jogar; o jogo fica mais firme e mais barato de crescer.
- **Save confiável**: o save inteiro vai para uma pasta nova e troca de lugar de uma vez (o anterior vira o backup); uma queda no meio nunca mistura dois momentos. A gravação dos arquivos é feita em segundo plano (no quadro fica só a captura, 0,4 ms em média). Cada save guarda a própria semente do sorteio (save v12). Um save real de cada versão (v1, v2, v3 e v5 a v11, tirados dos backups) é aberto e regravado pelos testes.
- **Código do mundo por assunto**: o `World.cpp` foi dividido em 11 arquivos (núcleo, combate, moradores, save, crônicas, regiões, conselho, defesa, guilda, cotidiano e castelo), sem mudar a lógica.
- **Painéis em pilha**: uma pilha única de telas abertas decide quando o relógio para e quando o alvo some. Os avisos foram para o canto de baixo, e N mostra as últimas 60 mensagens com estação, dia e hora.
- **Regras nos dados**: cada construção diz a sua função, se começa em ruínas, se conta como obra, se tem janelas, as ações do painel e a chaminé; forja, mercado e guilda dizem o nível pelo efeito. Nenhuma comparação com o id de uma construção ficou no código.
- **Colisão em cache** (refeita só quando algo muda) e os animais não atravessam mais casas e cercas.
- **Balanceamento refeito** com um jogador automático que usa todos os sistemas por 120 dias, em 4 ritmos: o celeiro guarda 12 dias de comida (+8 com o Celeiro Real) e 40% do excesso estraga por noite; a felicidade das obras se aproxima de um teto de 35; acima de 40 moradores, cada um tira 0,5 de felicidade. No ritmo médio, o povo se estabiliza perto de 100 (antes passava de 230) e a comida fica entre 1 e 20 dias guardados.
- Fica para depois (de propósito): o ouro ainda acumula no fim do jogo (vai ter destino no mercado vivo e na Guarda Real); os sistemas ainda guardam o estado dentro do World (a divisão foi por arquivo).

### Próximas fases (plano da auditoria de 18/11/2026)
- **Fase 18 — Povo com nome e clima.** **Fase 19 — Reputação e as três primeiras facções.** **Fase 20 — Mercado vivo e Livro do Reino.** **Fase 21 — Linhagem.** Detalhes e critérios no relatório da auditoria.

### Depois da 2.0 (para decidir mais tarde)
- Linhagem: casamento, herdeiros, envelhecimento e sucessão.
- Facções e diplomacia (anões, elfos, tribos do norte, Ordem Arcana, Culto das Cinzas).
- Quinta era (Coroa Eterna) como estado pós-história.
- Moradores que ganham fama e títulos próprios.

## Notas técnicas

- Cada fase que mudar o que é salvo sobe a versão do save (5, 6...), sempre com migração dos saves antigos e teste com cópia de save real. A fase 11 levou o save para a v6 (`regions.json`) a 12 para a v7 (`council.json`) a 13 para a v8 (`defense.json`), a 14 para a v9 (`guild.json`), a 15 para a v10 (`life.json`), a 16 para a v11 (`castle.json`) e a 17 para a v12 (a semente do sorteio no `metadata.json`).
- Conteúdo (crônicas, títulos, eventos, cargos, estações) em JSON dentro de `Data/`, como missões e diálogos hoje.
- O roteiro ponta a ponta ganha etapas a cada fase; a arte nova vem do NidraIA em sessão separada.
