# Balanceamento (simulaÃ§Ã£o)

Gerado por `ferramentas\balanceamento.ps1`: um jogador automÃ¡tico (`Tools\Balanceamento\JogadorAutomatico`)
joga 120 dias com a lÃ³gica real do jogo. Toda manhÃ£ ele coleta N Ã¡rvores, rochas ou baÃºs (o mais perto do que falta),
cuida da horta (e compra sementes do Corvin), nomeia o conselho e segue a maioria nos casos, repara o portÃ£o,
compra pedra no mercado com o ouro que sobra, constrÃ³i e melhora por prioridade (castelo incluÃ­do), ergue alas,
manda expediÃ§Ãµes com dois aventureiros, usa imposto baixo quando o povo estÃ¡ abaixo de 50 de felicidade, e dorme.
As hordas sÃ£o resolvidas pelas defesas, sem o herdeiro.

**Fase 17 (regras novas):** o celeiro guarda 12 dias de consumo (+8 com o Celeiro Real) e 40% do que passar disso
estraga por noite; a felicidade que as obras dÃ£o se aproxima de um teto de 35 (cada obra a mais rende menos); acima
de 40 moradores, cada um tira 0,5 de felicidade (aglomeraÃ§Ã£o).

**Faixas (viram alarme):** nenhum recurso negativo; do dia 10 em diante, entre 1 e 20 dias de comida guardados;
felicidade a partir do dia 20 acima de 40; no ritmo de 25 coletas, Reino Renascido entre os dias 10 e 22.
O ouro ainda acumula no fim do jogo (sem destino): fica para o mercado vivo e a Guarda Real.

**Fase 18:** o povo com nome rende pelas profissÃµes (madeira, pedra, comida, ouro, seguranÃ§a) e cada desejo realizado
alegra um pouco o reino; o tempo do dia (chuva rega a horta, neve deixa todos mais lentos, neblina reforÃ§a a horda)
vale na simulaÃ§Ã£o. O limiar do Reino Renascido subiu de 22 para 26 de desenvolvimento (a era jÃ¡ alcanÃ§ada nÃ£o volta
atrÃ¡s). O jogador automÃ¡tico constrÃ³i hortas tambÃ©m quando a reserva fica abaixo de 2 dias.

**SuposiÃ§Ãµes (estimativas, nÃ£o medidas de gente jogando):** cada coleta leva uns 10 s de jogo
(andar + golpes), entÃ£o 12/25/40 coletas por dia â‰ˆ 2/4/7 minutos de coleta num dia de 14 minutos.
O jogador automÃ¡tico nÃ£o luta: a derrota do Cavaleiro CaÃ­do Ã© marcada num dia fixo.
As estaÃ§Ãµes (7 dias cada) valem na simulaÃ§Ã£o. O jogador automÃ¡tico constrÃ³i muitas hortas, entÃ£o o
inverno quase nÃ£o o afeta; quem tem poucas hortas sente a comida cair (ex.: 1 horta e 15 moradores = -13 por dia).

### Coletas por dia: 12 | Chefe derrotado no dia: 30 | Dias simulados: 120

- O Despertar da Coroa: dia 6
- Reino Renascido: dia 16
- Reino da Alta Fantasia: dia 30
- Desejos realizados: 30 (dia 60: 30); desistiram de esperar: 14
- Vizinhos no fim: anões +25, Ordem +14, Culto -10; tratados em vigor: 1; fama: justiça +23, generosidade +11, devoção +10
- Intriga: desvios descobertos 0; ouro ainda sumido 0
- Mercado: 3810 de comida vendida; 1965 de ouro em vendas e 19136 em compras; comida a 0.70 a unidade no dia 60 e 0.64 no fim
- Alarmes: nenhum

| Dia | Era | Desenv. | Povo | Feliz. | Ouro | Madeira | Pedra | Comida (dias) | Estragou | Obras | Castelo/alas | Exped. | Hordas/saques |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | 0 | 2 | 8/14 | 42 | 11 | 1 | 8 | 17 (2.1) | 0 | 2 | 1/0 | 0 | 0/0 |
| 2 | 0 | 4 | 8/15 | 45 | 14 | 5 | 6 | 19 (2.4) | 0 | 4 | 1/0 | 0 | 0/0 |
| 3 | 0 | 5 | 8/15 | 49 | 11 | 4 | 4 | 21 (2.6) | 0 | 5 | 1/0 | 0 | 0/0 |
| 5 | 0 | 7 | 10/15 | 53 | 30 | 14 | 12 | 26 (2.6) | 0 | 7 | 1/0 | 0 | 0/0 |
| 10 | 1 | 17 | 17/19 | 57 | 104 | 1 | 11 | 99 (5.8) | 0 | 17 | 1/0 | 0 | 0/0 |
| 20 | 2 | 34 | 27/51 | 61 | 151 | 11 | 34 | 261 (9.7) | 0 | 34 | 1/0 | 2 | 1/5 |
| 30 | 3 | 57 | 37/55 | 70 | 139 | 77 | 14 | 380 (10.3) | 0 | 57 | 1/0 | 7 | 2/12 |
| 40 | 3 | 71 | 47/71 | 71 | 177 | 91 | 13 | 655 (13.9) | 74 | 71 | 1/0 | 12 | 2/12 |
| 50 | 3 | 78 | 58/72 | 75 | 175 | 193 | 55 | 752 (13.0) | 59 | 78 | 2/0 | 17 | 3/13 |
| 60 | 3 | 78 | 68/72 | 61 | 288 | 671 | 634 | 762 (11.2) | 0 | 78 | 2/1 | 22 | 4/13 |
| 70 | 3 | 96 | 78/144 | 55 | 355 | 916 | 1001 | 988 (12.7) | 48 | 96 | 2/1 | 27 | 4/13 |
| 80 | 3 | 112 | 88/144 | 51 | 278 | 1199 | 1480 | 987 (11.2) | 0 | 112 | 2/1 | 32 | 5/15 |
| 90 | 3 | 117 | 92/144 | 50 | 353 | 1528 | 1918 | 1238 (13.5) | 88 | 117 | 2/1 | 37 | 6/17 |
| 100 | 3 | 120 | 96/146 | 51 | 263 | 1907 | 2320 | 1423 (14.8) | 195 | 120 | 2/1 | 42 | 7/19 |
| 110 | 3 | 121 | 98/146 | 51 | 361 | 2386 | 2856 | 1109 (11.3) | 0 | 121 | 2/1 | 47 | 7/19 |
| 120 | 3 | 121 | 100/146 | 52 | 575 | 2901 | 3544 | 1345 (13.4) | 111 | 121 | 2/1 | 52 | 8/23 |

### Coletas por dia: 25 | Chefe derrotado no dia: 25 | Dias simulados: 120

- O Despertar da Coroa: dia 5
- Reino Renascido: dia 11
- Reino da Alta Fantasia: dia 25
- Desejos realizados: 32 (dia 60: 32); desistiram de esperar: 15
- Vizinhos no fim: anões +27, Ordem +15, Culto -10; tratados em vigor: 1; fama: justiça +23, generosidade +12, devoção +10
- Intriga: desvios descobertos 0; ouro ainda sumido 0
- Mercado: 4170 de comida vendida; 2118 de ouro em vendas e 19766 em compras; comida a 0.70 a unidade no dia 60 e 0.64 no fim
- Alarmes: nenhum

| Dia | Era | Desenv. | Povo | Feliz. | Ouro | Madeira | Pedra | Comida (dias) | Estragou | Obras | Castelo/alas | Exped. | Hordas/saques |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | 0 | 2 | 8/14 | 42 | 11 | 13 | 10 | 17 (2.1) | 0 | 2 | 1/0 | 0 | 0/0 |
| 2 | 0 | 5 | 8/15 | 45 | 9 | 27 | 22 | 24 (3.0) | 0 | 5 | 1/0 | 0 | 0/0 |
| 3 | 0 | 6 | 8/15 | 49 | 11 | 49 | 21 | 32 (4.0) | 0 | 6 | 1/0 | 0 | 0/0 |
| 5 | 1 | 8 | 10/15 | 52 | 27 | 86 | 5 | 47 (4.7) | 0 | 8 | 1/0 | 0 | 0/0 |
| 10 | 1 | 25 | 17/35 | 56 | 106 | 2 | 37 | 171 (10.1) | 0 | 25 | 1/0 | 0 | 0/0 |
| 20 | 2 | 49 | 27/63 | 68 | 104 | 67 | 32 | 373 (13.8) | 46 | 49 | 1/0 | 4 | 1/5 |
| 30 | 3 | 61 | 37/83 | 76 | 163 | 184 | 22 | 378 (10.2) | 0 | 61 | 1/0 | 9 | 2/12 |
| 40 | 3 | 70 | 51/84 | 72 | 261 | 228 | 135 | 631 (12.4) | 26 | 70 | 1/0 | 14 | 2/12 |
| 50 | 3 | 78 | 61/84 | 73 | 281 | 620 | 421 | 740 (12.1) | 20 | 78 | 2/1 | 19 | 3/12 |
| 60 | 3 | 90 | 71/84 | 60 | 361 | 1204 | 870 | 895 (12.6) | 43 | 90 | 2/1 | 24 | 4/12 |
| 70 | 3 | 90 | 81/84 | 54 | 375 | 2082 | 1564 | 1091 (13.5) | 94 | 90 | 2/1 | 29 | 4/12 |
| 80 | 3 | 108 | 91/156 | 50 | 446 | 2640 | 1979 | 1042 (11.5) | 0 | 108 | 2/1 | 34 | 5/14 |
| 90 | 3 | 128 | 95/156 | 50 | 426 | 3092 | 2291 | 1318 (13.9) | 133 | 128 | 2/1 | 39 | 6/18 |
| 100 | 3 | 132 | 97/158 | 52 | 218 | 3749 | 2617 | 1522 (15.7) | 253 | 132 | 2/1 | 44 | 7/20 |
| 110 | 3 | 133 | 99/158 | 51 | 424 | 4589 | 3193 | 1175 (11.9) | 5 | 133 | 2/1 | 49 | 7/20 |
| 120 | 3 | 133 | 101/158 | 51 | 590 | 5394 | 3870 | 1426 (14.1) | 156 | 133 | 2/1 | 54 | 8/23 |

### Coletas por dia: 40 | Chefe derrotado no dia: 15 | Dias simulados: 120

- O Despertar da Coroa: dia 6
- Reino Renascido: dia 10
- Reino da Alta Fantasia: dia 17
- Desejos realizados: 46 (dia 60: 34); desistiram de esperar: 15
- Vizinhos no fim: anões +32, Ordem +16, Culto -13; tratados em vigor: 1; fama: justiça +23, generosidade +12, devoção +10
- Intriga: desvios descobertos 0; ouro ainda sumido 0
- Mercado: 4000 de comida vendida; 2025 de ouro em vendas e 19861 em compras; comida a 0.70 a unidade no dia 60 e 0.64 no fim
- Alarmes: nenhum

| Dia | Era | Desenv. | Povo | Feliz. | Ouro | Madeira | Pedra | Comida (dias) | Estragou | Obras | Castelo/alas | Exped. | Hordas/saques |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | 0 | 2 | 8/14 | 42 | 11 | 13 | 10 | 17 (2.1) | 0 | 2 | 1/0 | 0 | 0/0 |
| 2 | 0 | 4 | 8/15 | 45 | 12 | 59 | 36 | 19 (2.4) | 0 | 4 | 1/0 | 0 | 0/0 |
| 3 | 0 | 5 | 8/15 | 49 | 11 | 55 | 30 | 21 (2.6) | 0 | 5 | 1/0 | 0 | 0/0 |
| 5 | 0 | 7 | 10/15 | 52 | 25 | 106 | 19 | 26 (2.6) | 0 | 7 | 1/0 | 0 | 0/0 |
| 10 | 2 | 27 | 17/39 | 59 | 51 | 5 | 2 | 145 (8.5) | 0 | 27 | 1/0 | 0 | 0/0 |
| 20 | 3 | 46 | 27/67 | 68 | 208 | 113 | 50 | 341 (12.6) | 24 | 46 | 1/0 | 3 | 1/5 |
| 30 | 3 | 60 | 37/95 | 73 | 203 | 255 | 50 | 339 (9.2) | 0 | 60 | 1/0 | 8 | 2/12 |
| 40 | 3 | 70 | 51/96 | 72 | 161 | 295 | 126 | 589 (11.5) | 0 | 70 | 1/0 | 13 | 2/12 |
| 50 | 3 | 78 | 61/96 | 73 | 327 | 691 | 400 | 721 (11.8) | 6 | 78 | 2/1 | 18 | 3/12 |
| 60 | 3 | 91 | 71/96 | 60 | 367 | 1477 | 862 | 873 (12.3) | 28 | 91 | 2/1 | 23 | 4/12 |
| 70 | 3 | 91 | 81/96 | 54 | 428 | 2616 | 1584 | 1070 (13.2) | 79 | 91 | 2/1 | 28 | 4/12 |
| 80 | 3 | 108 | 91/96 | 50 | 359 | 3361 | 1980 | 1065 (11.7) | 0 | 108 | 2/1 | 33 | 5/12 |
| 90 | 3 | 114 | 95/98 | 52 | 443 | 4169 | 2381 | 1307 (13.8) | 126 | 114 | 2/1 | 38 | 6/12 |
| 100 | 3 | 133 | 97/170 | 52 | 198 | 4853 | 2713 | 1496 (15.4) | 235 | 133 | 2/1 | 43 | 7/14 |
| 110 | 3 | 133 | 99/170 | 51 | 382 | 5854 | 3433 | 1162 (11.7) | 0 | 133 | 2/1 | 48 | 7/14 |
| 120 | 3 | 133 | 101/170 | 51 | 539 | 6718 | 4120 | 1405 (13.9) | 143 | 133 | 2/1 | 53 | 8/17 |

### Coletas por dia: 25 | Chefe derrotado no dia: nunca | Dias simulados: 120

- O Despertar da Coroa: dia 5
- Reino Renascido: dia 11
- Reino da Alta Fantasia: não alcançado
- Desejos realizados: 34 (dia 60: 34); desistiram de esperar: 15
- Vizinhos no fim: anões +26, Ordem +15, Culto -10; tratados em vigor: 1; fama: justiça +23, generosidade +12, devoção +10
- Intriga: desvios descobertos 0; ouro ainda sumido 0
- Mercado: 4170 de comida vendida; 2123 de ouro em vendas e 19943 em compras; comida a 0.70 a unidade no dia 60 e 0.64 no fim
- Alarmes: nenhum

| Dia | Era | Desenv. | Povo | Feliz. | Ouro | Madeira | Pedra | Comida (dias) | Estragou | Obras | Castelo/alas | Exped. | Hordas/saques |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | 0 | 2 | 8/14 | 42 | 11 | 13 | 10 | 17 (2.1) | 0 | 2 | 1/0 | 0 | 0/0 |
| 2 | 0 | 5 | 8/15 | 45 | 9 | 27 | 22 | 24 (3.0) | 0 | 5 | 1/0 | 0 | 0/0 |
| 3 | 0 | 6 | 8/15 | 49 | 11 | 49 | 21 | 32 (4.0) | 0 | 6 | 1/0 | 0 | 0/0 |
| 5 | 1 | 8 | 10/15 | 52 | 27 | 86 | 5 | 47 (4.7) | 0 | 8 | 1/0 | 0 | 0/0 |
| 10 | 1 | 25 | 17/35 | 56 | 106 | 2 | 37 | 171 (10.1) | 0 | 25 | 1/0 | 0 | 0/0 |
| 20 | 2 | 49 | 27/63 | 68 | 104 | 67 | 32 | 373 (13.8) | 46 | 49 | 1/0 | 4 | 1/5 |
| 30 | 2 | 61 | 37/83 | 73 | 153 | 164 | 20 | 378 (10.2) | 0 | 61 | 1/0 | 9 | 2/12 |
| 40 | 2 | 70 | 51/84 | 72 | 176 | 252 | 30 | 631 (12.4) | 26 | 70 | 1/0 | 14 | 2/12 |
| 50 | 2 | 77 | 61/84 | 73 | 320 | 671 | 430 | 740 (12.1) | 20 | 77 | 2/1 | 19 | 3/12 |
| 60 | 2 | 89 | 71/84 | 60 | 345 | 1294 | 887 | 893 (12.6) | 42 | 89 | 2/1 | 24 | 4/12 |
| 70 | 2 | 90 | 81/84 | 54 | 371 | 2122 | 1501 | 1091 (13.5) | 94 | 90 | 2/1 | 29 | 4/12 |
| 80 | 2 | 109 | 91/156 | 50 | 441 | 2680 | 1836 | 1042 (11.5) | 0 | 109 | 2/1 | 34 | 5/14 |
| 90 | 2 | 129 | 95/156 | 50 | 457 | 3122 | 2118 | 1319 (13.9) | 133 | 129 | 2/1 | 39 | 6/16 |
| 100 | 2 | 132 | 97/158 | 52 | 360 | 3809 | 2514 | 1522 (15.7) | 253 | 132 | 2/1 | 44 | 7/18 |
| 110 | 2 | 132 | 99/158 | 51 | 419 | 4676 | 3240 | 1175 (11.9) | 5 | 132 | 2/1 | 49 | 7/18 |
| 120 | 2 | 132 | 101/158 | 51 | 616 | 5481 | 3911 | 1425 (14.1) | 156 | 132 | 2/1 | 54 | 8/22 |
