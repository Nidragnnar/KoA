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
- Desejos realizados: 53 (dia 60: 26); desistiram de esperar: 16
- Vizinhos no fim: anões +94, Ordem +43, Culto -32; tratados em vigor: 2; fama: justiça +76, generosidade +100, devoção +10
- Intriga: desvios descobertos 0; ouro ainda sumido 0
- Mundo vivo: 3 de 3 aldeias de pé no fim; 50 ajudas dadas; no máximo 2 notícias num relatório
- Guarda Real: 1 esquadra(s) no fim; 188 de ouro de soldo em 120 dias
- Mercado: 3790 de comida vendida; 1848 de ouro em vendas e 15313 em compras; comida a 0.51 a unidade no dia 60 e 0.47 no fim
- Alarmes: nenhum

| Dia | Era | Desenv. | Povo | Feliz. | Ouro | Madeira | Pedra | Comida (dias) | Estragou | Obras | Castelo/alas | Exped. | Hordas/saques |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | 0 | 2 | 8/14 | 42 | 11 | 1 | 8 | 17 (2.1) | 0 | 2 | 1/0 | 0 | 0/0 |
| 2 | 0 | 4 | 8/15 | 45 | 14 | 5 | 6 | 19 (2.4) | 0 | 4 | 1/0 | 0 | 0/0 |
| 3 | 0 | 5 | 8/15 | 49 | 11 | 4 | 4 | 21 (2.6) | 0 | 5 | 1/0 | 0 | 0/0 |
| 5 | 0 | 7 | 10/15 | 53 | 43 | 14 | 12 | 26 (2.6) | 0 | 7 | 1/0 | 0 | 0/0 |
| 10 | 1 | 18 | 18/27 | 56 | 105 | 7 | 17 | 91 (5.1) | 0 | 18 | 1/0 | 0 | 0/0 |
| 20 | 2 | 32 | 25/43 | 58 | 139 | 5 | 16 | 265 (10.6) | 0 | 32 | 1/0 | 0 | 1/5 |
| 30 | 3 | 47 | 33/47 | 67 | 146 | 14 | 17 | 321 (9.7) | 0 | 47 | 1/0 | 0 | 2/12 |
| 40 | 3 | 58 | 43/59 | 63 | 171 | 34 | 18 | 561 (13.0) | 43 | 58 | 1/0 | 0 | 2/12 |
| 50 | 3 | 65 | 53/60 | 71 | 265 | 102 | 37 | 664 (12.5) | 32 | 65 | 1/0 | 2 | 3/16 |
| 60 | 3 | 94 | 63/108 | 66 | 206 | 92 | 220 | 786 (12.5) | 34 | 94 | 1/0 | 7 | 4/16 |
| 70 | 3 | 104 | 73/132 | 56 | 302 | 429 | 509 | 1024 (14.0) | 113 | 104 | 1/0 | 12 | 4/16 |
| 80 | 3 | 105 | 83/132 | 52 | 350 | 794 | 901 | 967 (11.7) | 0 | 105 | 2/1 | 17 | 5/16 |
| 90 | 3 | 105 | 89/132 | 51 | 334 | 1313 | 1534 | 1139 (12.8) | 63 | 105 | 2/1 | 22 | 6/16 |
| 100 | 3 | 105 | 91/132 | 52 | 253 | 1843 | 2121 | 1310 (14.4) | 161 | 105 | 2/1 | 27 | 7/17 |
| 110 | 3 | 107 | 93/132 | 51 | 255 | 2314 | 2549 | 993 (10.7) | 0 | 107 | 2/1 | 32 | 7/17 |
| 120 | 3 | 109 | 95/132 | 49 | 325 | 2777 | 2995 | 1249 (13.1) | 73 | 109 | 2/1 | 37 | 8/18 |

### Coletas por dia: 25 | Chefe derrotado no dia: 25 | Dias simulados: 120

- O Despertar da Coroa: dia 5
- Reino Renascido: dia 11
- Reino da Alta Fantasia: dia 25
- Desejos realizados: 34 (dia 60: 34); desistiram de esperar: 16
- Vizinhos no fim: anões +94, Ordem +43, Culto -32; tratados em vigor: 2; fama: justiça +75, generosidade +100, devoção +10
- Intriga: desvios descobertos 0; ouro ainda sumido 0
- Mundo vivo: 3 de 3 aldeias de pé no fim; 49 ajudas dadas; no máximo 2 notícias num relatório
- Guarda Real: 1 esquadra(s) no fim; 284 de ouro de soldo em 120 dias
- Mercado: 4090 de comida vendida; 1970 de ouro em vendas e 16595 em compras; comida a 0.51 a unidade no dia 60 e 0.47 no fim
- Alarmes: nenhum

| Dia | Era | Desenv. | Povo | Feliz. | Ouro | Madeira | Pedra | Comida (dias) | Estragou | Obras | Castelo/alas | Exped. | Hordas/saques |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | 0 | 2 | 8/14 | 42 | 11 | 13 | 10 | 17 (2.1) | 0 | 2 | 1/0 | 0 | 0/0 |
| 2 | 0 | 5 | 8/15 | 45 | 9 | 27 | 22 | 24 (3.0) | 0 | 5 | 1/0 | 0 | 0/0 |
| 3 | 0 | 6 | 8/15 | 49 | 11 | 49 | 21 | 32 (4.0) | 0 | 6 | 1/0 | 0 | 0/0 |
| 5 | 1 | 8 | 10/15 | 52 | 27 | 86 | 5 | 47 (4.7) | 0 | 8 | 1/0 | 0 | 0/0 |
| 10 | 1 | 25 | 18/35 | 53 | 110 | 2 | 27 | 179 (9.9) | 0 | 25 | 1/0 | 0 | 0/0 |
| 20 | 2 | 47 | 27/63 | 65 | 135 | 51 | 11 | 363 (13.4) | 39 | 47 | 1/0 | 4 | 1/5 |
| 30 | 3 | 60 | 37/83 | 71 | 110 | 197 | 38 | 364 (9.8) | 0 | 60 | 1/0 | 9 | 2/12 |
| 40 | 3 | 70 | 49/84 | 71 | 262 | 179 | 85 | 601 (12.3) | 22 | 70 | 1/0 | 14 | 2/12 |
| 50 | 3 | 78 | 59/84 | 72 | 259 | 415 | 266 | 695 (11.8) | 7 | 78 | 2/1 | 19 | 3/12 |
| 60 | 3 | 90 | 69/84 | 64 | 327 | 1035 | 672 | 864 (12.5) | 40 | 90 | 2/1 | 24 | 4/12 |
| 70 | 3 | 90 | 79/84 | 54 | 353 | 1886 | 1325 | 1058 (13.4) | 90 | 90 | 2/1 | 29 | 4/12 |
| 80 | 3 | 108 | 87/156 | 49 | 333 | 2436 | 1706 | 1007 (11.6) | 0 | 108 | 2/1 | 34 | 5/12 |
| 90 | 3 | 108 | 91/156 | 52 | 359 | 3269 | 2346 | 1136 (12.5) | 45 | 108 | 2/1 | 39 | 6/12 |
| 100 | 3 | 108 | 93/156 | 52 | 260 | 4129 | 2968 | 1304 (14.0) | 141 | 108 | 2/1 | 44 | 7/13 |
| 110 | 3 | 127 | 93/156 | 50 | 196 | 4656 | 3316 | 1078 (11.6) | 0 | 127 | 2/1 | 49 | 7/13 |
| 120 | 3 | 129 | 97/156 | 51 | 157 | 5439 | 3806 | 1371 (14.1) | 154 | 129 | 2/1 | 54 | 8/14 |

### Coletas por dia: 40 | Chefe derrotado no dia: 15 | Dias simulados: 120

- O Despertar da Coroa: dia 6
- Reino Renascido: dia 11
- Reino da Alta Fantasia: dia 19
- Desejos realizados: 44 (dia 60: 31); desistiram de esperar: 17
- Vizinhos no fim: anões +100, Ordem +45, Culto -35; tratados em vigor: 2; fama: justiça +76, generosidade +100, devoção +10
- Intriga: desvios descobertos 0; ouro ainda sumido 0
- Mundo vivo: 3 de 3 aldeias de pé no fim; 50 ajudas dadas; no máximo 2 notícias num relatório
- Guarda Real: 1 esquadra(s) no fim; 300 de ouro de soldo em 120 dias
- Mercado: 3900 de comida vendida; 1894 de ouro em vendas e 16857 em compras; comida a 0.51 a unidade no dia 60 e 0.47 no fim
- Alarmes: nenhum

| Dia | Era | Desenv. | Povo | Feliz. | Ouro | Madeira | Pedra | Comida (dias) | Estragou | Obras | Castelo/alas | Exped. | Hordas/saques |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | 0 | 2 | 8/14 | 42 | 11 | 13 | 10 | 17 (2.1) | 0 | 2 | 1/0 | 0 | 0/0 |
| 2 | 0 | 4 | 8/15 | 45 | 12 | 59 | 36 | 19 (2.4) | 0 | 4 | 1/0 | 0 | 0/0 |
| 3 | 0 | 5 | 8/15 | 49 | 11 | 55 | 30 | 21 (2.6) | 0 | 5 | 1/0 | 0 | 0/0 |
| 5 | 0 | 7 | 10/15 | 52 | 38 | 106 | 19 | 26 (2.6) | 0 | 7 | 1/0 | 0 | 0/0 |
| 10 | 1 | 24 | 18/27 | 56 | 107 | 10 | 16 | 144 (8.0) | 0 | 24 | 1/0 | 0 | 0/0 |
| 20 | 3 | 41 | 27/47 | 66 | 145 | 157 | 12 | 338 (12.5) | 23 | 41 | 1/0 | 4 | 1/3 |
| 30 | 3 | 53 | 37/59 | 71 | 148 | 309 | 48 | 324 (8.8) | 0 | 53 | 1/0 | 9 | 2/9 |
| 40 | 3 | 62 | 50/60 | 69 | 265 | 498 | 90 | 569 (11.4) | 0 | 62 | 1/0 | 14 | 2/9 |
| 50 | 3 | 69 | 60/60 | 71 | 219 | 729 | 423 | 697 (11.6) | 0 | 69 | 2/1 | 19 | 3/9 |
| 60 | 3 | 99 | 70/132 | 64 | 300 | 1135 | 546 | 847 (12.1) | 20 | 99 | 2/1 | 24 | 4/9 |
| 70 | 3 | 100 | 80/132 | 53 | 326 | 2124 | 1201 | 1048 (13.1) | 75 | 100 | 2/1 | 29 | 4/9 |
| 80 | 3 | 114 | 88/132 | 49 | 261 | 2985 | 1810 | 1040 (11.8) | 0 | 114 | 2/1 | 34 | 5/9 |
| 90 | 3 | 118 | 92/132 | 51 | 369 | 3880 | 2348 | 1265 (13.8) | 124 | 118 | 2/1 | 39 | 6/9 |
| 100 | 3 | 118 | 94/132 | 51 | 244 | 4865 | 2986 | 1455 (15.5) | 234 | 118 | 2/1 | 44 | 7/10 |
| 110 | 3 | 120 | 94/132 | 50 | 390 | 5826 | 3448 | 1095 (11.6) | 0 | 120 | 2/1 | 49 | 7/10 |
| 120 | 3 | 121 | 96/132 | 49 | 392 | 6660 | 4068 | 1359 (14.2) | 138 | 121 | 2/1 | 54 | 8/10 |

### Coletas por dia: 25 | Chefe derrotado no dia: nunca | Dias simulados: 120

- O Despertar da Coroa: dia 5
- Reino Renascido: dia 11
- Reino da Alta Fantasia: não alcançado
- Desejos realizados: 33 (dia 60: 33); desistiram de esperar: 17
- Vizinhos no fim: anões +94, Ordem +43, Culto -32; tratados em vigor: 2; fama: justiça +75, generosidade +100, devoção +10
- Intriga: desvios descobertos 0; ouro ainda sumido 0
- Mundo vivo: 3 de 3 aldeias de pé no fim; 49 ajudas dadas; no máximo 2 notícias num relatório
- Guarda Real: 1 esquadra(s) no fim; 300 de ouro de soldo em 120 dias
- Mercado: 4100 de comida vendida; 1975 de ouro em vendas e 16866 em compras; comida a 0.51 a unidade no dia 60 e 0.47 no fim
- Alarmes: nenhum

| Dia | Era | Desenv. | Povo | Feliz. | Ouro | Madeira | Pedra | Comida (dias) | Estragou | Obras | Castelo/alas | Exped. | Hordas/saques |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | 0 | 2 | 8/14 | 42 | 11 | 13 | 10 | 17 (2.1) | 0 | 2 | 1/0 | 0 | 0/0 |
| 2 | 0 | 5 | 8/15 | 45 | 9 | 27 | 22 | 24 (3.0) | 0 | 5 | 1/0 | 0 | 0/0 |
| 3 | 0 | 6 | 8/15 | 49 | 11 | 49 | 21 | 32 (4.0) | 0 | 6 | 1/0 | 0 | 0/0 |
| 5 | 1 | 8 | 10/15 | 52 | 27 | 86 | 5 | 47 (4.7) | 0 | 8 | 1/0 | 0 | 0/0 |
| 10 | 1 | 25 | 18/35 | 53 | 110 | 2 | 27 | 179 (9.9) | 0 | 25 | 1/0 | 0 | 0/0 |
| 20 | 2 | 47 | 27/63 | 65 | 135 | 51 | 11 | 363 (13.4) | 39 | 47 | 1/0 | 4 | 1/5 |
| 30 | 2 | 60 | 37/83 | 70 | 134 | 187 | 33 | 364 (9.8) | 0 | 60 | 1/0 | 9 | 2/12 |
| 40 | 2 | 70 | 49/84 | 71 | 173 | 254 | 20 | 601 (12.3) | 23 | 70 | 1/0 | 14 | 2/12 |
| 50 | 2 | 77 | 59/84 | 72 | 261 | 526 | 327 | 706 (12.0) | 15 | 77 | 2/1 | 19 | 3/12 |
| 60 | 2 | 89 | 69/84 | 64 | 326 | 1158 | 735 | 864 (12.5) | 40 | 89 | 2/1 | 24 | 4/12 |
| 70 | 2 | 89 | 79/84 | 53 | 352 | 2033 | 1388 | 1058 (13.4) | 90 | 89 | 2/1 | 29 | 4/12 |
| 80 | 2 | 107 | 87/156 | 49 | 332 | 2586 | 1771 | 1007 (11.6) | 0 | 107 | 2/1 | 34 | 5/12 |
| 90 | 2 | 107 | 91/156 | 51 | 358 | 3425 | 2407 | 1136 (12.5) | 45 | 107 | 2/1 | 39 | 6/12 |
| 100 | 2 | 107 | 91/156 | 49 | 354 | 4285 | 3009 | 1296 (14.2) | 136 | 107 | 2/1 | 44 | 7/13 |
| 110 | 2 | 125 | 93/156 | 50 | 215 | 4836 | 3449 | 1063 (11.4) | 0 | 125 | 2/1 | 49 | 7/13 |
| 120 | 2 | 126 | 95/156 | 52 | 142 | 5702 | 4018 | 1349 (14.2) | 156 | 126 | 2/1 | 54 | 8/16 |
