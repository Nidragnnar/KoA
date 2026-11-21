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

### Fase 18 — Povo com nome e clima (feita, versão 1.10)
- **Povo com nome** (`Data/NPCs/povo.json`): cada habitante além dos moradores do conselho é uma pessoa: nome, família (quem chega junto vem em famílias de 2 a 4), profissão (10, algumas só com a obra pronta: guarda com posto, ferreiro com forja...), traço (6: rende mais ou menos e aguenta mais ou menos esperar) e um desejo (10: casa de pedra, taverna, capela, dormir sem medo, despensa cheia, povo feliz, a horta do herdeiro, o castelo, o brasão, o mercado). Ninguém chega desejando o que o reino já tem.
- As profissões rendem por noite (madeira, pedra, comida, ouro, segurança, alegria). Desejos realizados viram páginas nas Crônicas e alegram o reino (+0,2 cada, até +5). Com o povo abaixo de 50 de felicidade, quem esperou mais que a paciência vai embora (uma pessoa por noite). Fome e falta de casa também levam gente, e o relatório diz quem foi e por quê.
- **E perto de um aldeão** abre a ficha (retrato, família, desde quando mora, o desejo e há quantos dias espera). **P** abre a lista do povo, com mouse.
- **Clima** (`Data/World/clima.json`): o tempo de cada dia é sorteado pela estação e pela sorte da partida, e aparece no relatório e no painel da data. Chuva e tempestade regam a horta; tempestade tira 2 de felicidade e traz raios; neve deixa o herói e as criaturas 20% mais lentos; neblina deixa o mundo cinza e a horda 20% mais forte. Nada disso chega à cripta e às minas. Na Alta Fantasia a chuva é de luz e a neve brilha.
- **Balanceamento**: o limiar do Reino Renascido subiu de 22 para 26 (mais obras desde a fase 9 aceleravam as eras), e a era já alcançada não volta atrás. Na simulação de 120 dias, os 4 ritmos ficam sem alarmes; de 31 a 36 desejos realizados até o dia 60.
- Fica para depois: moradores do povo no conselho e na guilda, casamentos entre famílias (vai junto da linhagem), falas próprias por profissão.

### Fase 19 — Reputação, facções e intriga (feita, versão 1.11)
- **Três vizinhos** (`Data/Kingdom/faccoes.json`): os Anões de Pedra-Funda (minas; gostam de justiça), a Ordem Arcana (floresta; gostam de devoção e justiça) e o Culto das Cinzas (cripta; gostam de mão dura e de magia proibida). Relação de -100 a 100 em cinco faixas (hostis, desconfiados, neutros, amistosos, aliados).
- **Reagem ao que já acontece**: 34 reações ligadas a marcos que o jogo já tinha (o pacto, a purificação ou a destruição do Golem; a floresta purificada; relíquias purificadas ou malditas; hordas vencidas ou com saque; cada saída dos casos do conselho; presentes e banquetes; alas do castelo). Saves antigos refazem as relações pelos marcos que já têm.
- **Tratados** (dois por povo): pedem relação mínima e um custo; enquanto valem, rendem todo dia (pedra, cristal, ouro, segurança, soldados no portão, força nas expedições, dano, hordas mais fracas) e seguram a relação. Se a relação despencar, o povo rompe. Assinar com a Ordem tira 20 do Culto e vice-versa. Os tratados do Culto pagam bem e custam felicidade.
- **Missões das facções** (duas por povo), aceitas no painel T; cumpridas, sobem a relação.
- **Fama do herdeiro** em três eixos: justiça, generosidade e devoção. As decisões e o imposto empurram a fama; passar de 30 marca Justo, Tirano, Generoso, Avarento, Devoto ou Arcano. A fama abre três títulos (o Justo, o Temido, o Bem-Amado), falas novas de seis moradores e, devagar, puxa a relação de cada povo para o que ele gosta.
- **Intriga no conselho**: um conselheiro com lealdade baixa desvia ouro toda noite sem aparecer no relatório. A chance de descobrir é de 20% por noite (+25 com um Marechal leal, +25 com a Biblioteca no castelo); descoberto, abre o caso `Ouro sumindo do cofre`: tirar do cargo e cobrar metade, perdoar em segredo ou fingir que não viu.
- **Painel T** com teclado e mouse: as três barras da fama, cada povo com a relação, os tratados à esquerda e as missões à direita, e o detalhe da linha escolhida (descrição, efeitos, custo e o que falta).
- **Balanceamento**: o jogador automático assina os tratados dos anões e da Ordem quando sobra ouro. Em 120 dias: anões perto de +30, Ordem +16, Culto -13, fama de justiça +23 (abaixo do limite de 30: a fama é para quem governa de propósito), nenhum desvio (o bot não maltrata o conselho). Sem alarmes nos 4 ritmos.
- Fica para depois: elfos e tribos do norte, guerra entre facções no mapa, emissários andando pelo vilarejo, conselheiros vindos do povo.

### Fase 20 — Mercado vivo, Livro do Reino e acessibilidade (feita, versão 1.12)
- **Mercado vivo** (`Data/Kingdom/mercado.json`): o preço de cada bem é base × estação × (1 + pressão) × (1 + sorte do dia). Comprar sobe a pressão, vender desce, e a cada noite ela volta 25% para o normal. Comida: 0,8 no outono, 1,4 no inverno (vale ter guardado). A taxa (20%, 15%, 10% pelo nível do mercado; 25% no Corvin) e o limite de lotes por dia (6, 9, 12) impedem lucro sem fim: um teste de propriedade faz 300 ciclos aleatórios de compra e venda que voltam ao mesmo estoque e nenhum termina com mais ouro. Acontecimentos mexem nos preços (saque na horda, festival, minas abertas, tratados dos anões e da Ordem, Lua de Sangue, meteoros).
- O mercado compra colheita, peixe, ovos, leite e pratos (o que antes só servia para comer ou presentear). O **Corvin** também compra (pagando menos) e aceita **encomendas** pagas na hora, que chegam numa visita seguinte (cristal, peixe dourado, sementes, pedra lavrada, tábuas).
- Painel do mercado (V, ou "Negociar no mercado" na construção): uma linha por bem, com botões de comprar e vender, ✓/✗, o que o herdeiro tem e a tendência dos últimos 14 dias; embaixo, por que o preço está assim.
- **Livro do Reino** (Tab ou o botão "Livro" no canto): os painéis de informação viram abas numa faixa embaixo (Reino, Mercado, Terras, Conselho, Povo, Vizinhos, Diário, Crônicas, Avisos). Clique troca de aba; Tab passa para a próxima; as teclas antigas continuam. Os painéis de cada construção (forja, guilda, fogão, alas, brasão) seguem abrindo na construção.
- **Acessibilidade**: dicas ao passar o mouse nos recursos, no povo e na data (o que entra por dia, o preço no mercado); texto grande (conversas, avisos e dicas com letra em dobro); velocidade do texto (lenta, normal, rápida, instantânea); teclas trocáveis em Opções → Controles (guardadas no `config.json`); ✓ ou ✗ na prévia de obra e nos botões, além da cor.
- **Ações das alas** (`castelo.json`, com recarga em dias): Salão de Banquetes dá uma festa (+6 de felicidade), Sala do Tesouro empresta 60 ao povo (voltam 78 em 6 dias, ou só 60 com o povo triste), Quartel treina a guarda (+2 soldados na próxima horda), Biblioteca decifra fragmentos (diz onde está o próximo fragmento da Queda). Os vizinhos e a fama reagem a cada uso.
- **Sons de fundo por região**: grilos à noite e pássaros nas eras felizes no vilarejo, corvos e vento na floresta amaldiçoada, pássaros e brilhos na encantada, gotas com eco na cripta, gotas e estrondos nas minas.
- **Balanceamento**: o jogador automático vende a comida que passa de 8 dias e compra pedra no mercado. Em 120 dias: perto de 4.000 de comida vendida (cerca de 2.000 de ouro); a comida ainda vale 0,64 de ouro a unidade no fim. Sem alarmes nos 4 ritmos.
- Fica para depois: a escala de texto dentro dos painéis grandes (hoje só conversas, avisos e dicas), controle (gamepad) sobre a camada de ações, e mercadores de outras facções.

### Fase 21 — Linhagem (feita, versão 1.13)
- **Tempo** (decisão do Nidragnnar): o dia a dia não muda; os filhos chegam à maioridade em 28 dias (um ano de jogo) e, na sucessão, a história salta 15 anos. O relógio não pula: o salto é de anos do reino (o painel mostra o "Ano N do reino").
- **Casamento** (tecla Y, `Data/Kingdom/linhagem.json`): com um morador de 7 corações (Bram, Edda, Maelis, Oswin ou Tilda, cada um com o seu traço) ou, por política, com alguém de uma facção aliada (Hilda dos anões, Arin da Ordem, Sera do Culto), que traz dote e irrita o rival. A festa custa 40 de comida e 60 de ouro e alegra o reino.
- **Filhos**: o primeiro nasce 7 dias depois do casamento, os outros a cada 14 (até 3). Na maioridade, cada um ganha dois traços: um herdado (do herdeiro ou do cônjuge) e um do **tutor**, o conselheiro escolhido no painel (Marechal ensina Valente, Mestre da Moeda Astuto, Intendente Frugal, Mestre das Obras Sábio). Tutor com lealdade baixa não ensina: vem um traço sorteado, que pode ser ruim (Frágil, Impetuoso).
- **Traços** que mudam o jogo: Valente (+25 de vida), Forte (+3 de dano), Carismático (+3 de felicidade), Astuto (-5% na taxa do mercado e mais chance de pegar desvio), Frugal (o povo come 10% menos), Sábio (+1 nas expedições), Devoto (fama de devoto por noite), Frágil e Impetuoso. O fundador, Aren, é Valente; saves antigos viram a primeira geração.
- **Sucessão**: com um filho adulto, a coroa passa (pede confirmação). O conselho reage (o tutor de quem sobe +10 de lealdade; pular o mais velho, -6), os vizinhos reagem aos traços de quem sobe, e o salto faz o povo esquecer 30% da fama e os vizinhos voltarem 30% ao que eram. O reino inteiro continua (obras, recursos, povo, tratados). Com 120 dias de reinado o herdeiro envelhece; com 168 morre, e a coroa passa sozinha para o filho adulto mais velho (sem filho adulto, ele segue no trono).
- **Crônicas por reinado** e um título novo (Herdeiro da Dinastia). O brasão passa de geração. Falas novas de Bram, Edda, Maelis e Oswin.
- Fica para depois: arte própria para crianças e para cada herdeiro (hoje o herói tem a mesma aparência), filhos andando pela vila, casamento dos filhos com gente das facções, regência quando ninguém pode herdar.

### Depois do plano da auditoria (para decidir mais tarde)
- Guarda Real (P3.4), Mundo vivo (P3.5), combate com escolhas (P2.5) em paralelo à arte, segundo e terceiro atos (P4.1), regiões novas (P4.2) e a Quinta Era como desfecho (P4.3).
### Depois da 2.0 (para decidir mais tarde)
- Linhagem: casamento, herdeiros, envelhecimento e sucessão.
- Mais facções e diplomacia (elfos, tribos do norte) e conflitos entre elas no mapa.
- Quinta era (Coroa Eterna) como estado pós-história.
- Moradores que ganham fama e títulos próprios.

## Notas técnicas

- Cada fase que mudar o que é salvo sobe a versão do save (5, 6...), sempre com migração dos saves antigos e teste com cópia de save real. A fase 11 levou o save para a v6 (`regions.json`) a 12 para a v7 (`council.json`) a 13 para a v8 (`defense.json`), a 14 para a v9 (`guild.json`), a 15 para a v10 (`life.json`), a 16 para a v11 (`castle.json`), a 17 para a v12 (a semente do sorteio no `metadata.json`) a 18 para a v13 (`people.json`) a 19 para a v14 (`factions.json`) a 20 para a v15 (`market.json`) e a 21 para a v16 (`family.json`).
- Conteúdo (crônicas, títulos, eventos, cargos, estações) em JSON dentro de `Data/`, como missões e diálogos hoje.
- O roteiro ponta a ponta ganha etapas a cada fase; a arte nova vem do NidraIA em sessão separada.
