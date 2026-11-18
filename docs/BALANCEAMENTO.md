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

**Suposições (estimativas, não medidas de gente jogando):** cada coleta leva uns 10 s de jogo
(andar + golpes), então 12/25/40 coletas por dia ≈ 2/4/7 minutos de coleta num dia de 14 minutos.
O jogador automático não luta: a derrota do Cavaleiro Caído é marcada num dia fixo.
As estações (7 dias cada) valem na simulação. O jogador automático constrói muitas hortas, então o
inverno quase não o afeta; quem tem poucas hortas sente a comida cair (ex.: 1 horta e 15 moradores = -13 por dia).

### Coletas por dia: 12 | Chefe derrotado no dia: 30 | Dias simulados: 120

- O Despertar da Coroa: dia 6
- Reino Renascido: dia 14
- Reino da Alta Fantasia: dia 30
- Alarmes: nenhum

| Dia | Era | Desenv. | Povo | Feliz. | Ouro | Madeira | Pedra | Comida (dias) | Estragou | Obras | Castelo/alas | Exped. | Hordas/saques |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | 0 | 2 | 8/14 | 42 | 11 | 1 | 8 | 17 (2.1) | 0 | 2 | 1/0 | 0 | 0/0 |
| 2 | 0 | 4 | 8/15 | 45 | 14 | 5 | 6 | 19 (2.4) | 0 | 4 | 1/0 | 0 | 0/0 |
| 3 | 0 | 5 | 8/15 | 49 | 11 | 4 | 4 | 21 (2.6) | 0 | 5 | 1/0 | 0 | 0/0 |
| 5 | 0 | 7 | 10/15 | 53 | 30 | 14 | 12 | 26 (2.6) | 0 | 7 | 1/0 | 0 | 0/0 |
| 10 | 1 | 17 | 17/19 | 57 | 107 | 1 | 11 | 96 (5.6) | 0 | 17 | 1/0 | 0 | 0/0 |
| 20 | 2 | 34 | 27/51 | 60 | 142 | 17 | 43 | 346 (12.8) | 27 | 34 | 1/0 | 1 | 1/5 |
| 30 | 3 | 62 | 37/67 | 68 | 178 | 62 | 25 | 493 (13.3) | 46 | 62 | 1/0 | 6 | 2/8 |
| 40 | 3 | 78 | 47/83 | 64 | 132 | 62 | 37 | 755 (16.1) | 141 | 78 | 1/0 | 11 | 2/8 |
| 50 | 3 | 88 | 57/84 | 66 | 284 | 161 | 118 | 874 (15.3) | 140 | 88 | 1/0 | 16 | 3/12 |
| 60 | 3 | 93 | 67/84 | 61 | 384 | 391 | 300 | 939 (14.0) | 104 | 93 | 2/1 | 21 | 4/12 |
| 70 | 3 | 97 | 77/86 | 57 | 1182 | 731 | 610 | 1098 (14.3) | 130 | 97 | 2/1 | 26 | 4/12 |
| 80 | 3 | 115 | 87/110 | 52 | 2694 | 1010 | 1229 | 1081 (12.4) | 38 | 115 | 2/1 | 31 | 5/12 |
| 90 | 3 | 133 | 93/158 | 51 | 3528 | 1124 | 1508 | 1357 (14.6) | 175 | 133 | 2/1 | 36 | 6/12 |
| 100 | 3 | 133 | 96/158 | 47 | 4716 | 1604 | 2136 | 1561 (16.3) | 279 | 133 | 2/1 | 41 | 7/14 |
| 110 | 3 | 133 | 98/158 | 47 | 5876 | 2104 | 2784 | 1213 (12.4) | 30 | 133 | 2/1 | 46 | 7/14 |
| 120 | 3 | 133 | 99/158 | 48 | 6835 | 2548 | 3397 | 1457 (14.7) | 186 | 133 | 2/1 | 51 | 8/18 |

### Coletas por dia: 25 | Chefe derrotado no dia: 25 | Dias simulados: 120

- O Despertar da Coroa: dia 5
- Reino Renascido: dia 11
- Reino da Alta Fantasia: dia 25
- Alarmes: nenhum

| Dia | Era | Desenv. | Povo | Feliz. | Ouro | Madeira | Pedra | Comida (dias) | Estragou | Obras | Castelo/alas | Exped. | Hordas/saques |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | 0 | 2 | 8/14 | 42 | 11 | 13 | 10 | 17 (2.1) | 0 | 2 | 1/0 | 0 | 0/0 |
| 2 | 0 | 5 | 8/15 | 45 | 9 | 27 | 22 | 24 (3.0) | 0 | 5 | 1/0 | 0 | 0/0 |
| 3 | 0 | 6 | 8/15 | 49 | 11 | 49 | 21 | 32 (4.0) | 0 | 6 | 1/0 | 0 | 0/0 |
| 5 | 1 | 8 | 10/15 | 52 | 27 | 86 | 5 | 47 (4.7) | 0 | 8 | 1/0 | 0 | 0/0 |
| 10 | 1 | 21 | 17/39 | 58 | 80 | 18 | 3 | 17 (1.0) | 0 | 21 | 1/0 | 0 | 0/0 |
| 20 | 2 | 45 | 27/71 | 68 | 153 | 61 | 39 | 348 (12.9) | 29 | 45 | 1/0 | 5 | 1/5 |
| 30 | 3 | 68 | 37/95 | 72 | 141 | 215 | 20 | 492 (13.3) | 46 | 68 | 1/0 | 10 | 2/9 |
| 40 | 3 | 80 | 47/96 | 70 | 175 | 320 | 65 | 714 (15.2) | 114 | 80 | 1/0 | 15 | 2/9 |
| 50 | 3 | 85 | 57/96 | 71 | 307 | 579 | 215 | 808 (14.2) | 96 | 85 | 2/1 | 20 | 3/9 |
| 60 | 3 | 91 | 67/98 | 62 | 311 | 996 | 481 | 879 (13.1) | 64 | 91 | 2/1 | 25 | 4/9 |
| 70 | 3 | 91 | 77/98 | 57 | 1737 | 1796 | 1159 | 1029 (13.4) | 84 | 91 | 2/1 | 30 | 4/9 |
| 80 | 3 | 109 | 87/98 | 51 | 3254 | 2432 | 1818 | 1054 (12.1) | 20 | 109 | 2/1 | 35 | 5/9 |
| 90 | 3 | 127 | 93/98 | 51 | 4085 | 2828 | 2211 | 1416 (15.2) | 214 | 127 | 2/1 | 40 | 6/9 |
| 100 | 3 | 127 | 96/98 | 47 | 5268 | 3674 | 2853 | 1645 (17.1) | 334 | 127 | 2/1 | 45 | 7/11 |
| 110 | 3 | 145 | 98/170 | 47 | 6088 | 4228 | 3277 | 1241 (12.7) | 50 | 145 | 2/1 | 50 | 7/11 |
| 120 | 3 | 145 | 99/170 | 48 | 7047 | 4996 | 3910 | 1521 (15.4) | 228 | 145 | 2/1 | 55 | 8/15 |

### Coletas por dia: 40 | Chefe derrotado no dia: 15 | Dias simulados: 120

- O Despertar da Coroa: dia 6
- Reino Renascido: dia 8
- Reino da Alta Fantasia: dia 18
- Alarmes: nenhum

| Dia | Era | Desenv. | Povo | Feliz. | Ouro | Madeira | Pedra | Comida (dias) | Estragou | Obras | Castelo/alas | Exped. | Hordas/saques |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | 0 | 2 | 8/14 | 42 | 11 | 13 | 10 | 17 (2.1) | 0 | 2 | 1/0 | 0 | 0/0 |
| 2 | 0 | 4 | 8/15 | 45 | 12 | 59 | 36 | 19 (2.4) | 0 | 4 | 1/0 | 0 | 0/0 |
| 3 | 0 | 5 | 8/15 | 49 | 11 | 55 | 30 | 21 (2.6) | 0 | 5 | 1/0 | 0 | 0/0 |
| 5 | 0 | 7 | 10/15 | 52 | 25 | 106 | 19 | 26 (2.6) | 0 | 7 | 1/0 | 0 | 0/0 |
| 10 | 2 | 27 | 17/39 | 59 | 51 | 5 | 2 | 138 (8.1) | 0 | 27 | 1/0 | 0 | 0/0 |
| 20 | 3 | 46 | 27/67 | 68 | 154 | 40 | 36 | 393 (14.6) | 60 | 46 | 1/0 | 3 | 1/5 |
| 30 | 3 | 59 | 37/91 | 72 | 187 | 176 | 44 | 431 (11.6) | 4 | 59 | 1/0 | 8 | 2/9 |
| 40 | 3 | 69 | 47/96 | 69 | 148 | 197 | 115 | 623 (13.3) | 53 | 69 | 1/0 | 13 | 2/9 |
| 50 | 3 | 79 | 57/96 | 70 | 298 | 526 | 329 | 730 (12.8) | 45 | 79 | 2/1 | 18 | 3/9 |
| 60 | 3 | 110 | 67/96 | 60 | 296 | 998 | 661 | 1030 (15.4) | 165 | 110 | 2/1 | 23 | 4/9 |
| 70 | 3 | 115 | 77/98 | 57 | 863 | 1755 | 945 | 1211 (15.7) | 206 | 115 | 2/1 | 28 | 4/9 |
| 80 | 3 | 115 | 87/98 | 52 | 2495 | 2754 | 1662 | 1124 (12.9) | 68 | 115 | 2/1 | 33 | 5/9 |
| 90 | 3 | 115 | 93/98 | 51 | 3874 | 3606 | 2355 | 1337 (14.4) | 162 | 115 | 2/1 | 38 | 6/9 |
| 100 | 3 | 115 | 95/98 | 49 | 4917 | 4512 | 3025 | 1526 (16.1) | 263 | 115 | 2/1 | 43 | 7/11 |
| 110 | 3 | 133 | 98/170 | 47 | 5741 | 5153 | 3463 | 1201 (12.3) | 23 | 133 | 2/1 | 48 | 7/11 |
| 120 | 3 | 133 | 99/170 | 48 | 6700 | 5864 | 4096 | 1436 (14.5) | 172 | 133 | 2/1 | 53 | 8/15 |

### Coletas por dia: 25 | Chefe derrotado no dia: nunca | Dias simulados: 120

- O Despertar da Coroa: dia 5
- Reino Renascido: dia 11
- Reino da Alta Fantasia: não alcançado
- Alarmes: nenhum

| Dia | Era | Desenv. | Povo | Feliz. | Ouro | Madeira | Pedra | Comida (dias) | Estragou | Obras | Castelo/alas | Exped. | Hordas/saques |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | 0 | 2 | 8/14 | 42 | 11 | 13 | 10 | 17 (2.1) | 0 | 2 | 1/0 | 0 | 0/0 |
| 2 | 0 | 5 | 8/15 | 45 | 9 | 27 | 22 | 24 (3.0) | 0 | 5 | 1/0 | 0 | 0/0 |
| 3 | 0 | 6 | 8/15 | 49 | 11 | 49 | 21 | 32 (4.0) | 0 | 6 | 1/0 | 0 | 0/0 |
| 5 | 1 | 8 | 10/15 | 52 | 27 | 86 | 5 | 47 (4.7) | 0 | 8 | 1/0 | 0 | 0/0 |
| 10 | 1 | 21 | 17/39 | 58 | 80 | 18 | 3 | 17 (1.0) | 0 | 21 | 1/0 | 0 | 0/0 |
| 20 | 2 | 45 | 27/71 | 68 | 153 | 61 | 39 | 348 (12.9) | 29 | 45 | 1/0 | 5 | 1/5 |
| 30 | 2 | 68 | 37/95 | 71 | 191 | 190 | 25 | 497 (13.4) | 48 | 68 | 1/0 | 10 | 2/10 |
| 40 | 2 | 79 | 47/96 | 69 | 177 | 320 | 110 | 714 (15.2) | 114 | 79 | 1/0 | 15 | 2/10 |
| 50 | 2 | 84 | 57/96 | 70 | 309 | 579 | 260 | 808 (14.2) | 96 | 84 | 2/1 | 20 | 3/10 |
| 60 | 2 | 90 | 67/98 | 62 | 313 | 1056 | 518 | 879 (13.1) | 64 | 90 | 2/1 | 25 | 4/10 |
| 70 | 2 | 90 | 77/98 | 57 | 1739 | 1829 | 1208 | 1029 (13.4) | 84 | 90 | 2/1 | 30 | 4/10 |
| 80 | 2 | 108 | 87/98 | 51 | 3256 | 2462 | 1867 | 1054 (12.1) | 20 | 108 | 2/1 | 35 | 5/10 |
| 90 | 2 | 126 | 92/98 | 51 | 3949 | 2858 | 2260 | 1406 (15.3) | 216 | 126 | 2/1 | 40 | 6/10 |
| 100 | 2 | 126 | 95/98 | 48 | 5113 | 3704 | 2902 | 1634 (17.2) | 336 | 126 | 2/1 | 45 | 7/12 |
| 110 | 2 | 144 | 97/170 | 48 | 5917 | 4258 | 3326 | 1232 (12.7) | 51 | 144 | 2/1 | 50 | 7/12 |
| 120 | 2 | 144 | 98/170 | 49 | 6903 | 5026 | 3959 | 1512 (15.4) | 230 | 144 | 2/1 | 55 | 8/16 |
