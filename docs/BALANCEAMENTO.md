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
- Reino Renascido: dia 15
- Reino da Alta Fantasia: dia 30
- Desejos realizados: 38 (dia 60: 31); desistiram de esperar: 14
- Vizinhos no fim: anões +72, Ordem +36, Culto -22; tratados em vigor: 2; fama: justiça +42, generosidade +47, devoção +10
- Intriga: desvios descobertos 0; ouro ainda sumido 0
- Mundo vivo: 3 de 3 aldeias de pé no fim; 17 ajudas dadas; no máximo 2 notícias num relatório
- Mercado: 3770 de comida vendida; 1861 de ouro em vendas e 19417 em compras; comida a 0.51 a unidade no dia 60 e 0.47 no fim
- Alarmes: nenhum

| Dia | Era | Desenv. | Povo | Feliz. | Ouro | Madeira | Pedra | Comida (dias) | Estragou | Obras | Castelo/alas | Exped. | Hordas/saques |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | 0 | 2 | 8/14 | 42 | 11 | 1 | 8 | 17 (2.1) | 0 | 2 | 1/0 | 0 | 0/0 |
| 2 | 0 | 4 | 8/15 | 45 | 14 | 5 | 6 | 19 (2.4) | 0 | 4 | 1/0 | 0 | 0/0 |
| 3 | 0 | 5 | 8/15 | 49 | 11 | 4 | 4 | 21 (2.6) | 0 | 5 | 1/0 | 0 | 0/0 |
| 5 | 0 | 7 | 10/15 | 53 | 43 | 14 | 12 | 26 (2.6) | 0 | 7 | 1/0 | 0 | 0/0 |
| 10 | 1 | 18 | 18/27 | 56 | 105 | 7 | 17 | 91 (5.1) | 0 | 18 | 1/0 | 0 | 0/0 |
| 20 | 2 | 32 | 25/43 | 59 | 169 | 19 | 44 | 261 (10.4) | 0 | 32 | 1/0 | 1 | 1/5 |
| 30 | 3 | 51 | 35/47 | 69 | 198 | 99 | 27 | 320 (9.1) | 0 | 51 | 1/0 | 6 | 2/11 |
| 40 | 3 | 65 | 45/60 | 72 | 193 | 80 | 43 | 581 (12.9) | 40 | 65 | 1/0 | 11 | 2/11 |
| 50 | 3 | 69 | 55/60 | 74 | 230 | 358 | 158 | 689 (12.5) | 34 | 69 | 2/1 | 16 | 3/12 |
| 60 | 3 | 98 | 65/108 | 66 | 288 | 380 | 483 | 829 (12.8) | 47 | 98 | 2/1 | 21 | 4/12 |
| 70 | 3 | 106 | 75/132 | 55 | 419 | 777 | 825 | 1049 (14.0) | 113 | 106 | 2/1 | 26 | 4/12 |
| 80 | 3 | 109 | 85/132 | 56 | 450 | 1172 | 1278 | 1001 (11.8) | 2 | 109 | 2/1 | 31 | 5/12 |
| 90 | 3 | 112 | 95/134 | 53 | 611 | 1559 | 1715 | 1207 (12.7) | 59 | 112 | 2/1 | 36 | 6/12 |
| 100 | 3 | 112 | 97/134 | 51 | 827 | 2086 | 2427 | 1387 (14.3) | 163 | 112 | 2/1 | 41 | 7/12 |
| 110 | 3 | 130 | 97/134 | 52 | 651 | 2335 | 2997 | 1143 (11.8) | 0 | 130 | 2/1 | 46 | 7/12 |
| 120 | 3 | 130 | 99/134 | 51 | 784 | 2822 | 3678 | 1425 (14.4) | 172 | 130 | 2/1 | 51 | 8/14 |

### Coletas por dia: 25 | Chefe derrotado no dia: 25 | Dias simulados: 120

- O Despertar da Coroa: dia 5
- Reino Renascido: dia 11
- Reino da Alta Fantasia: dia 25
- Desejos realizados: 53 (dia 60: 33); desistiram de esperar: 15
- Vizinhos no fim: anões +72, Ordem +36, Culto -22; tratados em vigor: 2; fama: justiça +42, generosidade +45, devoção +10
- Intriga: desvios descobertos 0; ouro ainda sumido 0
- Mundo vivo: 3 de 3 aldeias de pé no fim; 16 ajudas dadas; no máximo 2 notícias num relatório
- Mercado: 4130 de comida vendida; 1976 de ouro em vendas e 20236 em compras; comida a 0.51 a unidade no dia 60 e 0.47 no fim
- Alarmes: nenhum

| Dia | Era | Desenv. | Povo | Feliz. | Ouro | Madeira | Pedra | Comida (dias) | Estragou | Obras | Castelo/alas | Exped. | Hordas/saques |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | 0 | 2 | 8/14 | 42 | 11 | 13 | 10 | 17 (2.1) | 0 | 2 | 1/0 | 0 | 0/0 |
| 2 | 0 | 5 | 8/15 | 45 | 9 | 27 | 22 | 24 (3.0) | 0 | 5 | 1/0 | 0 | 0/0 |
| 3 | 0 | 6 | 8/15 | 49 | 11 | 49 | 21 | 32 (4.0) | 0 | 6 | 1/0 | 0 | 0/0 |
| 5 | 1 | 8 | 10/15 | 52 | 27 | 86 | 5 | 47 (4.7) | 0 | 8 | 1/0 | 0 | 0/0 |
| 10 | 1 | 25 | 18/35 | 53 | 110 | 2 | 27 | 179 (9.9) | 0 | 25 | 1/0 | 0 | 0/0 |
| 20 | 2 | 48 | 27/63 | 66 | 92 | 8 | 11 | 362 (13.4) | 39 | 48 | 1/0 | 4 | 1/5 |
| 30 | 3 | 61 | 37/83 | 71 | 106 | 172 | 16 | 362 (9.8) | 0 | 61 | 1/0 | 9 | 2/12 |
| 40 | 3 | 70 | 49/84 | 71 | 151 | 266 | 147 | 601 (12.3) | 23 | 70 | 1/0 | 14 | 2/12 |
| 50 | 3 | 78 | 59/84 | 72 | 266 | 610 | 422 | 716 (12.1) | 19 | 78 | 2/1 | 19 | 3/12 |
| 60 | 3 | 90 | 69/84 | 60 | 331 | 1278 | 818 | 872 (12.6) | 44 | 90 | 2/1 | 24 | 4/12 |
| 70 | 3 | 92 | 79/84 | 53 | 379 | 2106 | 1383 | 1066 (13.5) | 93 | 92 | 2/1 | 29 | 4/12 |
| 80 | 3 | 110 | 87/156 | 49 | 379 | 2644 | 1824 | 1017 (11.7) | 0 | 110 | 2/1 | 34 | 5/12 |
| 90 | 3 | 113 | 91/156 | 53 | 230 | 3383 | 2272 | 1153 (12.7) | 55 | 113 | 2/1 | 39 | 6/12 |
| 100 | 3 | 115 | 95/158 | 50 | 484 | 4160 | 2788 | 1343 (14.1) | 133 | 115 | 2/1 | 44 | 7/12 |
| 110 | 3 | 133 | 97/158 | 51 | 403 | 4708 | 3330 | 1114 (11.5) | 0 | 133 | 2/1 | 49 | 7/12 |
| 120 | 3 | 133 | 99/158 | 51 | 571 | 5475 | 4062 | 1400 (14.1) | 156 | 133 | 2/1 | 54 | 8/14 |

### Coletas por dia: 40 | Chefe derrotado no dia: 15 | Dias simulados: 120

- O Despertar da Coroa: dia 6
- Reino Renascido: dia 11
- Reino da Alta Fantasia: dia 18
- Desejos realizados: 40 (dia 60: 30); desistiram de esperar: 16
- Vizinhos no fim: anões +74, Ordem +36, Culto -22; tratados em vigor: 2; fama: justiça +43, generosidade +48, devoção +10
- Intriga: desvios descobertos 0; ouro ainda sumido 0
- Mundo vivo: 3 de 3 aldeias de pé no fim; 17 ajudas dadas; no máximo 2 notícias num relatório
- Mercado: 3930 de comida vendida; 1906 de ouro em vendas e 20393 em compras; comida a 0.51 a unidade no dia 60 e 0.47 no fim
- Alarmes: nenhum

| Dia | Era | Desenv. | Povo | Feliz. | Ouro | Madeira | Pedra | Comida (dias) | Estragou | Obras | Castelo/alas | Exped. | Hordas/saques |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | 0 | 2 | 8/14 | 42 | 11 | 13 | 10 | 17 (2.1) | 0 | 2 | 1/0 | 0 | 0/0 |
| 2 | 0 | 4 | 8/15 | 45 | 12 | 59 | 36 | 19 (2.4) | 0 | 4 | 1/0 | 0 | 0/0 |
| 3 | 0 | 5 | 8/15 | 49 | 11 | 55 | 30 | 21 (2.6) | 0 | 5 | 1/0 | 0 | 0/0 |
| 5 | 0 | 7 | 10/15 | 52 | 38 | 106 | 19 | 26 (2.6) | 0 | 7 | 1/0 | 0 | 0/0 |
| 10 | 1 | 24 | 18/27 | 56 | 107 | 10 | 16 | 144 (8.0) | 0 | 24 | 1/0 | 0 | 0/0 |
| 20 | 3 | 42 | 27/47 | 66 | 147 | 129 | 2 | 347 (12.9) | 28 | 42 | 1/0 | 4 | 1/3 |
| 30 | 3 | 54 | 37/59 | 76 | 152 | 266 | 43 | 332 (9.0) | 0 | 54 | 1/0 | 9 | 2/9 |
| 40 | 3 | 62 | 50/60 | 69 | 272 | 495 | 140 | 565 (11.3) | 0 | 62 | 1/0 | 14 | 2/9 |
| 50 | 3 | 69 | 60/60 | 71 | 269 | 930 | 543 | 707 (11.8) | 5 | 69 | 2/1 | 19 | 3/9 |
| 60 | 3 | 99 | 70/132 | 60 | 338 | 1405 | 732 | 859 (12.3) | 26 | 99 | 2/1 | 24 | 4/9 |
| 70 | 3 | 101 | 80/132 | 53 | 351 | 2421 | 1323 | 1055 (13.2) | 78 | 101 | 2/1 | 29 | 4/9 |
| 80 | 3 | 117 | 90/132 | 54 | 317 | 3300 | 1826 | 1051 (11.7) | 0 | 117 | 2/1 | 34 | 5/9 |
| 90 | 3 | 124 | 96/134 | 51 | 438 | 4188 | 2208 | 1307 (13.6) | 117 | 124 | 2/1 | 39 | 6/9 |
| 100 | 3 | 124 | 96/134 | 49 | 586 | 5183 | 2988 | 1501 (15.6) | 231 | 124 | 2/1 | 44 | 7/9 |
| 110 | 3 | 124 | 96/134 | 49 | 606 | 6192 | 3728 | 1143 (11.9) | 0 | 124 | 2/1 | 49 | 7/9 |
| 120 | 3 | 124 | 98/134 | 49 | 794 | 7186 | 4476 | 1385 (14.1) | 137 | 124 | 2/1 | 54 | 8/11 |

### Coletas por dia: 25 | Chefe derrotado no dia: nunca | Dias simulados: 120

- O Despertar da Coroa: dia 5
- Reino Renascido: dia 11
- Reino da Alta Fantasia: não alcançado
- Desejos realizados: 41 (dia 60: 33); desistiram de esperar: 17
- Vizinhos no fim: anões +72, Ordem +36, Culto -22; tratados em vigor: 2; fama: justiça +42, generosidade +46, devoção +10
- Intriga: desvios descobertos 0; ouro ainda sumido 0
- Mundo vivo: 3 de 3 aldeias de pé no fim; 16 ajudas dadas; no máximo 2 notícias num relatório
- Mercado: 4120 de comida vendida; 1973 de ouro em vendas e 20114 em compras; comida a 0.51 a unidade no dia 60 e 0.47 no fim
- Alarmes: nenhum

| Dia | Era | Desenv. | Povo | Feliz. | Ouro | Madeira | Pedra | Comida (dias) | Estragou | Obras | Castelo/alas | Exped. | Hordas/saques |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | 0 | 2 | 8/14 | 42 | 11 | 13 | 10 | 17 (2.1) | 0 | 2 | 1/0 | 0 | 0/0 |
| 2 | 0 | 5 | 8/15 | 45 | 9 | 27 | 22 | 24 (3.0) | 0 | 5 | 1/0 | 0 | 0/0 |
| 3 | 0 | 6 | 8/15 | 49 | 11 | 49 | 21 | 32 (4.0) | 0 | 6 | 1/0 | 0 | 0/0 |
| 5 | 1 | 8 | 10/15 | 52 | 27 | 86 | 5 | 47 (4.7) | 0 | 8 | 1/0 | 0 | 0/0 |
| 10 | 1 | 25 | 18/35 | 53 | 110 | 2 | 27 | 179 (9.9) | 0 | 25 | 1/0 | 0 | 0/0 |
| 20 | 2 | 48 | 27/63 | 66 | 92 | 8 | 11 | 362 (13.4) | 39 | 48 | 1/0 | 4 | 1/5 |
| 30 | 2 | 60 | 37/83 | 70 | 146 | 202 | 61 | 362 (9.8) | 0 | 60 | 1/0 | 9 | 2/12 |
| 40 | 2 | 70 | 50/84 | 70 | 145 | 287 | 69 | 611 (12.2) | 21 | 70 | 1/0 | 14 | 2/12 |
| 50 | 2 | 77 | 60/84 | 71 | 253 | 680 | 474 | 727 (12.1) | 18 | 77 | 2/1 | 19 | 3/12 |
| 60 | 2 | 89 | 70/84 | 58 | 349 | 1310 | 877 | 884 (12.6) | 43 | 89 | 2/1 | 24 | 4/12 |
| 70 | 2 | 92 | 80/84 | 55 | 289 | 2093 | 1376 | 1077 (13.5) | 92 | 92 | 2/1 | 29 | 4/12 |
| 80 | 2 | 111 | 88/156 | 49 | 301 | 2619 | 1719 | 1027 (11.7) | 0 | 111 | 2/1 | 34 | 5/12 |
| 90 | 2 | 130 | 92/156 | 49 | 417 | 3153 | 2150 | 1300 (14.1) | 128 | 130 | 2/1 | 39 | 6/12 |
| 100 | 2 | 132 | 94/158 | 49 | 515 | 3933 | 2658 | 1506 (16.0) | 250 | 132 | 2/1 | 44 | 7/12 |
| 110 | 2 | 132 | 94/158 | 50 | 478 | 4802 | 3364 | 1136 (12.1) | 3 | 132 | 2/1 | 49 | 7/12 |
| 120 | 2 | 132 | 96/158 | 52 | 627 | 5668 | 4096 | 1371 (14.3) | 160 | 132 | 2/1 | 54 | 8/14 |
