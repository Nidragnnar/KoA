# Balanceamento (simulação)

Gerado por `ferramentas\balanceamento.ps1`: um jogador automático joga com a lógica real do jogo
(`Tools\Balanceamento`). Toda manhã ele coleta N árvores, rochas ou baús (o mais perto do que falta),
compra pedra no mercado com o ouro que sobra, constrói e melhora por prioridade, usa imposto baixo
quando o povo está abaixo de 50 de felicidade, e dorme.

**Suposições (estimativas, não medidas de gente jogando):** cada coleta leva uns 10 s de jogo
(andar + golpes), então 12/25/40 coletas por dia ≈ 2/4/7 minutos de coleta num dia de 14 minutos.
O jogador automático não luta: a derrota do Cavaleiro Caído é marcada num dia fixo.

### Coletas por dia: 12 | Chefe derrotado no dia: 30 | Dias simulados: 60

- O Despertar da Coroa: dia 5
- Reino Renascido: dia 13
- Reino da Alta Fantasia: dia 30

| Dia | Era | Desenv. | Povo | Feliz. | Ouro | Madeira | Pedra | Comida | Obras |
|---|---|---|---|---|---|---|---|---|---|
| 1 | 0 | 2 | 8/14 | 42 | 11 | 1 | 8 | 17 | 2 |
| 2 | 0 | 4 | 8/15 | 45 | 14 | 5 | 6 | 19 | 4 |
| 3 | 0 | 5 | 8/15 | 50 | 11 | 4 | 4 | 21 | 5 |
| 5 | 1 | 10 | 10/15 | 54 | 28 | 0 | 15 | 44 | 10 |
| 10 | 1 | 16 | 15/23 | 58 | 121 | 9 | 14 | 143 | 16 |
| 15 | 2 | 25 | 20/35 | 59 | 114 | 9 | 14 | 322 | 25 |
| 20 | 2 | 29 | 25/39 | 66 | 124 | 33 | 8 | 632 | 29 |
| 25 | 2 | 39 | 30/39 | 65 | 114 | 23 | 20 | 741 | 39 |
| 30 | 3 | 44 | 36/39 | 84 | 144 | 9 | 6 | 947 | 44 |
| 35 | 3 | 48 | 46/47 | 85 | 134 | 40 | 17 | 1347 | 48 |
| 40 | 3 | 59 | 56/91 | 86 | 164 | 26 | 3 | 1757 | 59 |
| 45 | 3 | 64 | 66/95 | 96 | 173 | 15 | 1 | 2261 | 64 |
| 50 | 3 | 71 | 76/96 | 100 | 288 | 86 | 208 | 2691 | 71 |
| 55 | 3 | 94 | 86/96 | 100 | 233 | 14 | 342 | 2728 | 94 |
| 60 | 3 | 104 | 96/96 | 100 | 373 | 21 | 514 | 3434 | 104 |

### Coletas por dia: 25 | Chefe derrotado no dia: 25 | Dias simulados: 60

- O Despertar da Coroa: dia 5
- Reino Renascido: dia 18
- Reino da Alta Fantasia: dia 25

| Dia | Era | Desenv. | Povo | Feliz. | Ouro | Madeira | Pedra | Comida | Obras |
|---|---|---|---|---|---|---|---|---|---|
| 1 | 0 | 2 | 8/14 | 42 | 11 | 13 | 10 | 17 | 2 |
| 2 | 0 | 5 | 8/15 | 45 | 9 | 27 | 22 | 24 | 5 |
| 3 | 0 | 6 | 8/15 | 49 | 10 | 54 | 20 | 31 | 6 |
| 5 | 1 | 8 | 10/15 | 54 | 23 | 89 | 2 | 44 | 8 |
| 10 | 1 | 16 | 15/19 | 60 | 40 | 75 | 17 | 94 | 16 |
| 15 | 1 | 19 | 20/23 | 64 | 138 | 178 | 25 | 200 | 19 |
| 20 | 2 | 30 | 26/59 | 77 | 161 | 145 | 8 | 360 | 30 |
| 25 | 3 | 56 | 35/71 | 74 | 81 | 19 | 21 | 426 | 56 |
| 30 | 3 | 62 | 42/75 | 90 | 158 | 33 | 6 | 695 | 62 |
| 35 | 3 | 68 | 52/95 | 92 | 118 | 35 | 4 | 1209 | 68 |
| 40 | 3 | 75 | 62/95 | 98 | 148 | 43 | 12 | 1844 | 75 |
| 45 | 3 | 80 | 72/96 | 100 | 185 | 29 | 21 | 2720 | 80 |
| 50 | 3 | 87 | 82/96 | 100 | 270 | 296 | 227 | 3504 | 87 |
| 55 | 3 | 99 | 92/96 | 100 | 280 | 401 | 347 | 3559 | 99 |
| 60 | 3 | 118 | 102/168 | 100 | 220 | 405 | 301 | 4188 | 118 |

### Coletas por dia: 40 | Chefe derrotado no dia: 15 | Dias simulados: 60

- O Despertar da Coroa: dia 5
- Reino Renascido: dia 10
- Reino da Alta Fantasia: dia 22

| Dia | Era | Desenv. | Povo | Feliz. | Ouro | Madeira | Pedra | Comida | Obras |
|---|---|---|---|---|---|---|---|---|---|
| 1 | 0 | 2 | 8/14 | 42 | 11 | 13 | 10 | 17 | 2 |
| 2 | 0 | 4 | 8/15 | 45 | 12 | 59 | 36 | 19 | 4 |
| 3 | 0 | 5 | 8/15 | 50 | 11 | 58 | 30 | 21 | 5 |
| 5 | 1 | 10 | 10/15 | 54 | 23 | 95 | 22 | 44 | 10 |
| 10 | 2 | 23 | 15/39 | 59 | 79 | 90 | 47 | 166 | 23 |
| 15 | 2 | 34 | 20/67 | 64 | 107 | 12 | 4 | 375 | 34 |
| 20 | 2 | 38 | 25/71 | 72 | 117 | 99 | 2 | 703 | 38 |
| 25 | 3 | 52 | 31/71 | 72 | 163 | 94 | 12 | 848 | 52 |
| 30 | 3 | 57 | 38/87 | 88 | 145 | 129 | 12 | 1076 | 57 |
| 35 | 3 | 62 | 48/95 | 91 | 185 | 186 | 14 | 1514 | 62 |
| 40 | 3 | 67 | 58/95 | 93 | 171 | 234 | 27 | 2044 | 67 |
| 45 | 3 | 71 | 68/96 | 99 | 249 | 239 | 82 | 2698 | 71 |
| 50 | 3 | 79 | 78/96 | 100 | 233 | 375 | 238 | 3272 | 79 |
| 55 | 3 | 101 | 88/96 | 100 | 198 | 545 | 362 | 3329 | 101 |
| 60 | 3 | 115 | 98/120 | 100 | 298 | 711 | 440 | 4103 | 115 |

### Coletas por dia: 25 | Chefe derrotado no dia: nunca | Dias simulados: 60

- O Despertar da Coroa: dia 5
- Reino Renascido: dia 18
- Reino da Alta Fantasia: não alcançado

| Dia | Era | Desenv. | Povo | Feliz. | Ouro | Madeira | Pedra | Comida | Obras |
|---|---|---|---|---|---|---|---|---|---|
| 1 | 0 | 2 | 8/14 | 42 | 11 | 13 | 10 | 17 | 2 |
| 2 | 0 | 5 | 8/15 | 45 | 9 | 27 | 22 | 24 | 5 |
| 3 | 0 | 6 | 8/15 | 49 | 10 | 54 | 20 | 31 | 6 |
| 5 | 1 | 8 | 10/15 | 54 | 23 | 89 | 2 | 44 | 8 |
| 10 | 1 | 16 | 15/19 | 60 | 40 | 75 | 17 | 94 | 16 |
| 15 | 1 | 19 | 20/23 | 64 | 138 | 178 | 25 | 200 | 19 |
| 20 | 2 | 30 | 26/59 | 77 | 161 | 145 | 8 | 360 | 30 |
| 25 | 2 | 56 | 35/71 | 74 | 81 | 19 | 21 | 426 | 56 |
| 30 | 2 | 62 | 42/75 | 90 | 158 | 33 | 6 | 695 | 62 |
| 35 | 2 | 68 | 52/95 | 92 | 118 | 35 | 4 | 1209 | 68 |
| 40 | 2 | 75 | 62/95 | 98 | 148 | 43 | 12 | 1844 | 75 |
| 45 | 2 | 80 | 72/96 | 100 | 185 | 29 | 21 | 2720 | 80 |
| 50 | 2 | 87 | 82/96 | 100 | 270 | 296 | 227 | 3504 | 87 |
| 55 | 2 | 99 | 92/96 | 100 | 280 | 401 | 347 | 3559 | 99 |
| 60 | 2 | 118 | 102/168 | 100 | 220 | 405 | 301 | 4188 | 118 |
