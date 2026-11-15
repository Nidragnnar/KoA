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

### Fase 11 — Regiões vivas
- Cada região com **corrupção, segurança e prosperidade**.
- Purificação: Floresta Amaldiçoada → Floresta Encantada.
- Região nova: Minas Abandonadas → Minas Arcanas (cristais).
- Região abandonada volta a ser tomada por criaturas.
- Chefes com escolha: derrotar, purificar ou fazer um pacto.

### Fase 12 — Conselho real e decisões
- Conselho formado pelos moradores (ex.: Oswin como Marechal, Bram como Mestre das Obras), começando com 3 ou 4 cargos.
- Eventos políticos com 2 ou 3 escolhas (nobre pede menos impostos, vila pede proteção...).
- Conselheiros com opinião e lealdade: podem discordar e, mais tarde, esconder coisas.

### Fase 13 — Defesa do reino
- Aviso de horda com dias para se preparar.
- Muralhas, portões, torres e soldados que patrulham.
- Batalha no portão com o herdeiro lutando; reparos depois.

### Fase 14 — Guilda, expedições e relíquias
- Guilda que surge com a estabilidade; expedições ao longo dos dias, com resultado conforme a preparação.
- Aventureiros podem voltar feridos, trazer artefatos ou desaparecer.
- Relíquias da antiga civilização, algumas amaldiçoadas.

### Fase 15 — Vida cotidiana
- Cultivo próprio, pesca, cozinha e animais.

### Depois da 2.0 (para decidir mais tarde)
- Linhagem: casamento, herdeiros, envelhecimento e sucessão.
- Facções e diplomacia (anões, elfos, tribos do norte, Ordem Arcana, Culto das Cinzas).
- Castelo personalizável, brasão e cores da linhagem.
- Quinta era (Coroa Eterna) como estado pós-história.
- Moradores que ganham fama e títulos próprios.

## Notas técnicas

- Cada fase que mudar o que é salvo sobe a versão do save (5, 6...), sempre com migração dos saves antigos e teste com cópia de save real.
- Conteúdo (crônicas, títulos, eventos, cargos, estações) em JSON dentro de `Data/`, como missões e diálogos hoje.
- O roteiro ponta a ponta ganha etapas a cada fase; a arte nova vem do NidraIA em sessão separada.
