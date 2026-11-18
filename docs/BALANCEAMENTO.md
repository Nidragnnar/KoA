# Balanceamento (simulação)

Gerado por `ferramentas\balanceamento.ps1`: um jogador automático (`Tools\Balanceamento\JogadorAutomatico`)
joga 120 dias com a lógica real do jogo. Toda manhã ele coleta N árvores, rochas ou baús (o mais perto do que falta),
cuida da horta (e compra sementes do Corvin), nomeia o conselho e segue a maioria nos casos, repara o portão,
compra pedra no mercado com o ouro que sobra, constrói e melhora por prioridade (castelo incluído), ergue alas,
manda expedições com dois aventureiros, usa imposto baixo quando o povo está abaixo de 50 de felicidade, e dorme.
As hordas são resolvidas pelas defesas, sem o herdeiro.

**Fase 17 (regras novas):** o celeiro guarda 12 dias de consumo (+8 com o Celeiro Real) e 40% do que passar disso
estraga por noite; a felicidade que as obras dão se aproxima de um teto de 35 (cada obra a mais rende menos); acima
de 40 moradores, cada um tira 0,5 de felicidade (aglomeração).

**Faixas (viram alarme):** nenhum recurso negativo; do dia 10 em diante, entre 1 e 20 dias de comida guardados;
felicidade a partir do dia 20 acima de 40; no ritmo de 25 coletas, Reino Renascido entre os dias 10 e 22.
O ouro ainda acumula no fim do jogo (sem destino): fica para o mercado vivo e a Guarda Real.

**Fase 18:** o povo com nome rende pelas profissões (madeira, pedra, comida, ouro, segurança) e cada desejo realizado
alegra um pouco o reino; o tempo do dia (chuva rega a horta, neve deixa todos mais lentos, neblina reforça a horda)
vale na simulação. O limiar do Reino Renascido subiu de 22 para 26 de desenvolvimento (a era já alcançada não volta
atrás). O jogador automático constrói hortas também quando a reserva fica abaixo de 2 dias.

**Suposições (estimativas, não medidas de gente jogando):** cada coleta leva uns 10 s de jogo
(andar + golpes), então 12/25/40 coletas por dia ≈ 2/4/7 minutos de coleta num dia de 14 minutos.
O jogador automático não luta: a derrota do Cavaleiro Caído é marcada num dia fixo.
As estações (7 dias cada) valem na simulação. O jogador automático constrói muitas hortas, então o
inverno quase não o afeta; quem tem poucas hortas sente a comida cair (ex.: 1 horta e 15 moradores = -13 por dia).

### Coletas por dia: 12 | Chefe derrotado no dia: 30 | Dias simulados: 120

- O Despertar da Coroa: dia 6
- Reino Renascido: dia 16
- Reino da Alta Fantasia: dia 30
- Desejos realizados: 37 (dia 60: 36); desistiram de esperar: 13
- Alarmes: nenhum

| Dia | Era | Desenv. | Povo | Feliz. | Ouro | Madeira | Pedra | Comida (dias) | Estragou | Obras | Castelo/alas | Exped. | Hordas/saques |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | 0 | 2 | 8/14 | 42 | 11 | 1 | 8 | 17 (2.1) | 0 | 2 | 1/0 | 0 | 0/0 |
| 2 | 0 | 4 | 8/15 | 45 | 14 | 5 | 6 | 19 (2.4) | 0 | 4 | 1/0 | 0 | 0/0 |
| 3 | 0 | 5 | 8/15 | 49 | 11 | 4 | 4 | 21 (2.6) | 0 | 5 | 1/0 | 0 | 0/0 |
| 5 | 0 | 7 | 10/15 | 53 | 30 | 14 | 12 | 26 (2.6) | 0 | 7 | 1/0 | 0 | 0/0 |
| 10 | 1 | 17 | 17/19 | 57 | 107 | 1 | 11 | 99 (5.8) | 0 | 17 | 1/0 | 0 | 0/0 |
| 20 | 2 | 33 | 27/43 | 63 | 130 | 12 | 13 | 353 (13.1) | 31 | 33 | 1/0 | 0 | 1/5 |
| 30 | 3 | 56 | 37/67 | 69 | 235 | 64 | 34 | 449 (12.1) | 16 | 56 | 1/0 | 5 | 2/11 |
| 40 | 3 | 72 | 47/83 | 67 | 178 | 93 | 40 | 713 (15.2) | 113 | 72 | 1/0 | 10 | 2/11 |
| 50 | 3 | 80 | 57/84 | 75 | 282 | 270 | 186 | 815 (14.3) | 101 | 80 | 1/0 | 15 | 3/13 |
| 60 | 3 | 85 | 67/84 | 67 | 342 | 552 | 400 | 889 (13.3) | 71 | 85 | 2/1 | 20 | 4/13 |
| 70 | 3 | 88 | 77/86 | 62 | 1318 | 940 | 828 | 1039 (13.5) | 91 | 88 | 2/1 | 25 | 4/13 |
| 80 | 3 | 106 | 87/110 | 57 | 2825 | 1240 | 1488 | 1052 (12.1) | 20 | 106 | 2/1 | 30 | 5/13 |
| 90 | 3 | 124 | 95/158 | 54 | 4116 | 1386 | 1807 | 1333 (14.0) | 143 | 124 | 2/1 | 35 | 6/13 |
| 100 | 3 | 124 | 97/158 | 53 | 5682 | 1906 | 2475 | 1503 (15.5) | 240 | 124 | 2/1 | 40 | 7/15 |
| 110 | 3 | 124 | 99/158 | 51 | 7282 | 2443 | 3165 | 1204 (12.2) | 24 | 124 | 2/1 | 45 | 7/15 |
| 120 | 3 | 124 | 101/158 | 51 | 8865 | 2930 | 3816 | 1421 (14.1) | 153 | 124 | 2/1 | 50 | 8/18 |

### Coletas por dia: 25 | Chefe derrotado no dia: 25 | Dias simulados: 120

- O Despertar da Coroa: dia 5
- Reino Renascido: dia 11
- Reino da Alta Fantasia: dia 25
- Desejos realizados: 32 (dia 60: 31); desistiram de esperar: 15
- Alarmes: nenhum

| Dia | Era | Desenv. | Povo | Feliz. | Ouro | Madeira | Pedra | Comida (dias) | Estragou | Obras | Castelo/alas | Exped. | Hordas/saques |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | 0 | 2 | 8/14 | 42 | 11 | 13 | 10 | 17 (2.1) | 0 | 2 | 1/0 | 0 | 0/0 |
| 2 | 0 | 5 | 8/15 | 45 | 9 | 27 | 22 | 24 (3.0) | 0 | 5 | 1/0 | 0 | 0/0 |
| 3 | 0 | 6 | 8/15 | 49 | 11 | 49 | 21 | 32 (4.0) | 0 | 6 | 1/0 | 0 | 0/0 |
| 5 | 1 | 8 | 10/15 | 52 | 27 | 86 | 5 | 47 (4.7) | 0 | 8 | 1/0 | 0 | 0/0 |
| 10 | 1 | 25 | 17/35 | 56 | 107 | 2 | 27 | 202 (11.9) | 9 | 25 | 1/0 | 0 | 0/0 |
| 20 | 2 | 47 | 27/63 | 67 | 124 | 82 | 18 | 418 (15.5) | 76 | 47 | 1/0 | 4 | 1/5 |
| 30 | 3 | 60 | 37/83 | 73 | 140 | 216 | 32 | 461 (12.5) | 24 | 60 | 1/0 | 9 | 2/12 |
| 40 | 3 | 70 | 51/84 | 72 | 141 | 280 | 96 | 688 (13.5) | 65 | 70 | 1/0 | 14 | 2/12 |
| 50 | 3 | 80 | 61/84 | 74 | 346 | 464 | 307 | 797 (13.1) | 58 | 80 | 2/1 | 19 | 3/12 |
| 60 | 3 | 97 | 71/86 | 66 | 367 | 887 | 443 | 987 (13.9) | 104 | 97 | 2/1 | 24 | 4/12 |
| 70 | 3 | 97 | 81/86 | 60 | 1947 | 1684 | 1156 | 1149 (14.2) | 132 | 97 | 2/1 | 29 | 4/12 |
| 80 | 3 | 115 | 91/158 | 56 | 3349 | 2222 | 1607 | 1111 (12.2) | 26 | 115 | 2/1 | 34 | 5/12 |
| 90 | 3 | 133 | 97/158 | 51 | 4781 | 2742 | 2171 | 1408 (14.5) | 177 | 133 | 2/1 | 39 | 6/12 |
| 100 | 3 | 133 | 99/158 | 51 | 6316 | 3614 | 2871 | 1600 (16.2) | 289 | 133 | 2/1 | 44 | 7/14 |
| 110 | 3 | 133 | 99/158 | 50 | 7910 | 4462 | 3551 | 1229 (12.4) | 42 | 133 | 2/1 | 49 | 7/14 |
| 120 | 3 | 133 | 101/158 | 51 | 9516 | 5295 | 4263 | 1483 (14.7) | 194 | 133 | 2/1 | 54 | 8/17 |

### Coletas por dia: 40 | Chefe derrotado no dia: 15 | Dias simulados: 120

- O Despertar da Coroa: dia 6
- Reino Renascido: dia 10
- Reino da Alta Fantasia: dia 17
- Desejos realizados: 35 (dia 60: 34); desistiram de esperar: 16
- Alarmes: nenhum

| Dia | Era | Desenv. | Povo | Feliz. | Ouro | Madeira | Pedra | Comida (dias) | Estragou | Obras | Castelo/alas | Exped. | Hordas/saques |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | 0 | 2 | 8/14 | 42 | 11 | 13 | 10 | 17 (2.1) | 0 | 2 | 1/0 | 0 | 0/0 |
| 2 | 0 | 4 | 8/15 | 45 | 12 | 59 | 36 | 19 (2.4) | 0 | 4 | 1/0 | 0 | 0/0 |
| 3 | 0 | 5 | 8/15 | 49 | 11 | 55 | 30 | 21 (2.6) | 0 | 5 | 1/0 | 0 | 0/0 |
| 5 | 0 | 7 | 10/15 | 52 | 25 | 106 | 19 | 26 (2.6) | 0 | 7 | 1/0 | 0 | 0/0 |
| 10 | 2 | 27 | 17/39 | 59 | 51 | 5 | 2 | 145 (8.5) | 0 | 27 | 1/0 | 0 | 0/0 |
| 20 | 3 | 46 | 27/67 | 68 | 154 | 29 | 40 | 397 (14.7) | 62 | 46 | 1/0 | 3 | 1/5 |
| 30 | 3 | 60 | 37/95 | 73 | 140 | 183 | 18 | 435 (11.8) | 8 | 60 | 1/0 | 8 | 2/12 |
| 40 | 3 | 70 | 51/96 | 72 | 154 | 196 | 96 | 669 (13.1) | 52 | 70 | 1/0 | 13 | 2/12 |
| 50 | 3 | 80 | 61/96 | 74 | 360 | 497 | 302 | 781 (12.8) | 46 | 80 | 2/1 | 18 | 3/12 |
| 60 | 3 | 97 | 71/98 | 66 | 316 | 1116 | 520 | 971 (13.7) | 94 | 97 | 2/1 | 23 | 4/12 |
| 70 | 3 | 97 | 81/98 | 60 | 1871 | 2027 | 1238 | 1130 (14.0) | 119 | 97 | 2/1 | 28 | 4/12 |
| 80 | 3 | 115 | 91/98 | 55 | 3319 | 2801 | 1834 | 1132 (12.4) | 40 | 115 | 2/1 | 33 | 5/12 |
| 90 | 3 | 115 | 97/98 | 50 | 5103 | 3818 | 2545 | 1390 (14.3) | 164 | 115 | 2/1 | 38 | 6/12 |
| 100 | 3 | 133 | 97/170 | 49 | 6314 | 4520 | 3019 | 1570 (16.2) | 268 | 133 | 2/1 | 43 | 7/14 |
| 110 | 3 | 133 | 97/170 | 50 | 7931 | 5446 | 3735 | 1222 (12.6) | 37 | 133 | 2/1 | 48 | 7/14 |
| 120 | 3 | 133 | 99/170 | 52 | 9475 | 6400 | 4452 | 1446 (14.6) | 186 | 133 | 2/1 | 53 | 8/17 |

### Coletas por dia: 25 | Chefe derrotado no dia: nunca | Dias simulados: 120

- O Despertar da Coroa: dia 5
- Reino Renascido: dia 11
- Reino da Alta Fantasia: não alcançado
- Desejos realizados: 34 (dia 60: 33); desistiram de esperar: 16
- Alarmes: nenhum

| Dia | Era | Desenv. | Povo | Feliz. | Ouro | Madeira | Pedra | Comida (dias) | Estragou | Obras | Castelo/alas | Exped. | Hordas/saques |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | 0 | 2 | 8/14 | 42 | 11 | 13 | 10 | 17 (2.1) | 0 | 2 | 1/0 | 0 | 0/0 |
| 2 | 0 | 5 | 8/15 | 45 | 9 | 27 | 22 | 24 (3.0) | 0 | 5 | 1/0 | 0 | 0/0 |
| 3 | 0 | 6 | 8/15 | 49 | 11 | 49 | 21 | 32 (4.0) | 0 | 6 | 1/0 | 0 | 0/0 |
| 5 | 1 | 8 | 10/15 | 52 | 27 | 86 | 5 | 47 (4.7) | 0 | 8 | 1/0 | 0 | 0/0 |
| 10 | 1 | 25 | 17/35 | 56 | 107 | 2 | 27 | 202 (11.9) | 9 | 25 | 1/0 | 0 | 0/0 |
| 20 | 2 | 47 | 27/63 | 67 | 124 | 82 | 18 | 418 (15.5) | 76 | 47 | 1/0 | 4 | 1/5 |
| 30 | 2 | 60 | 37/83 | 72 | 140 | 176 | 25 | 461 (12.5) | 25 | 60 | 1/0 | 9 | 2/12 |
| 40 | 2 | 69 | 51/84 | 72 | 178 | 256 | 151 | 688 (13.5) | 64 | 69 | 1/0 | 14 | 2/12 |
| 50 | 2 | 79 | 61/84 | 73 | 292 | 784 | 478 | 797 (13.1) | 58 | 79 | 1/0 | 19 | 3/12 |
| 60 | 2 | 95 | 71/86 | 66 | 342 | 1124 | 592 | 987 (13.9) | 104 | 95 | 2/1 | 24 | 4/12 |
| 70 | 2 | 96 | 81/86 | 60 | 1747 | 1906 | 1239 | 1149 (14.2) | 132 | 96 | 2/1 | 29 | 4/12 |
| 80 | 2 | 114 | 91/158 | 54 | 3149 | 2444 | 1690 | 1111 (12.2) | 26 | 114 | 2/1 | 34 | 5/12 |
| 90 | 2 | 132 | 95/158 | 49 | 4559 | 2963 | 2253 | 1402 (14.8) | 172 | 132 | 2/1 | 39 | 6/12 |
| 100 | 2 | 132 | 97/158 | 49 | 6090 | 3835 | 2953 | 1593 (16.4) | 284 | 132 | 2/1 | 44 | 7/14 |
| 110 | 2 | 132 | 97/158 | 50 | 7650 | 4683 | 3633 | 1229 (12.7) | 42 | 132 | 2/1 | 49 | 7/14 |
| 120 | 2 | 132 | 99/158 | 52 | 9251 | 5516 | 4339 | 1464 (14.8) | 198 | 132 | 2/1 | 54 | 8/18 |
