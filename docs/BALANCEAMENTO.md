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
- Reino da Alta Fantasia: dia 36

| Dia | Era | Desenv. | Povo | Feliz. | Ouro | Madeira | Pedra | Comida | Obras |
|---|---|---|---|---|---|---|---|---|---|
| 1 | 0 | 2 | 8/14 | 42 | 11 | 1 | 8 | 17 | 2 |
| 2 | 0 | 4 | 8/15 | 44 | 14 | 5 | 6 | 19 | 4 |
| 3 | 0 | 5 | 8/15 | 49 | 11 | 4 | 4 | 21 | 5 |
| 5 | 1 | 10 | 10/15 | 53 | 28 | 0 | 15 | 44 | 10 |
| 10 | 1 | 16 | 15/23 | 58 | 116 | 9 | 14 | 134 | 16 |
| 15 | 2 | 24 | 20/31 | 58 | 136 | 11 | 16 | 271 | 24 |
| 20 | 2 | 28 | 25/39 | 61 | 161 | 31 | 16 | 425 | 28 |
| 25 | 2 | 32 | 30/39 | 68 | 151 | 27 | 22 | 630 | 32 |
| 30 | 2 | 36 | 35/47 | 72 | 121 | 58 | 11 | 830 | 36 |
| 35 | 2 | 39 | 44/47 | 79 | 200 | 48 | 31 | 999 | 39 |
| 40 | 3 | 51 | 54/91 | 87 | 145 | 1 | 79 | 1119 | 51 |
| 45 | 3 | 56 | 64/108 | 95 | 220 | 60 | 68 | 1189 | 56 |
| 50 | 3 | 63 | 74/108 | 99 | 210 | 186 | 242 | 1239 | 63 |
| 55 | 3 | 75 | 84/108 | 100 | 275 | 120 | 276 | 1518 | 75 |
| 60 | 3 | 78 | 94/108 | 100 | 210 | 177 | 318 | 1798 | 78 |

### Coletas por dia: 25 | Chefe derrotado no dia: 25 | Dias simulados: 60

- O Despertar da Coroa: dia 5
- Reino Renascido: dia 18
- Reino da Alta Fantasia: dia 31

| Dia | Era | Desenv. | Povo | Feliz. | Ouro | Madeira | Pedra | Comida | Obras |
|---|---|---|---|---|---|---|---|---|---|
| 1 | 0 | 2 | 8/14 | 42 | 11 | 13 | 10 | 17 | 2 |
| 2 | 0 | 5 | 8/15 | 44 | 9 | 27 | 22 | 24 | 5 |
| 3 | 0 | 6 | 8/15 | 48 | 10 | 54 | 20 | 31 | 6 |
| 5 | 1 | 8 | 10/15 | 53 | 23 | 89 | 2 | 44 | 8 |
| 10 | 1 | 16 | 15/19 | 60 | 34 | 75 | 17 | 87 | 16 |
| 15 | 1 | 19 | 20/23 | 63 | 114 | 178 | 15 | 154 | 19 |
| 20 | 2 | 31 | 25/67 | 67 | 129 | 155 | 3 | 224 | 31 |
| 25 | 2 | 35 | 30/83 | 71 | 114 | 231 | 25 | 269 | 35 |
| 30 | 2 | 39 | 35/95 | 74 | 116 | 298 | 25 | 289 | 39 |
| 35 | 3 | 56 | 43/95 | 81 | 92 | 203 | 1 | 543 | 56 |
| 40 | 3 | 60 | 53/95 | 91 | 149 | 247 | 14 | 844 | 60 |
| 45 | 3 | 62 | 63/96 | 99 | 239 | 323 | 149 | 1099 | 62 |
| 50 | 3 | 63 | 73/96 | 100 | 290 | 576 | 339 | 1304 | 63 |
| 55 | 3 | 65 | 83/96 | 100 | 260 | 861 | 479 | 1459 | 65 |
| 60 | 3 | 67 | 93/96 | 100 | 312 | 1115 | 607 | 1564 | 67 |

### Coletas por dia: 40 | Chefe derrotado no dia: 15 | Dias simulados: 60

- O Despertar da Coroa: dia 5
- Reino Renascido: dia 10
- Reino da Alta Fantasia: dia 22

| Dia | Era | Desenv. | Povo | Feliz. | Ouro | Madeira | Pedra | Comida | Obras |
|---|---|---|---|---|---|---|---|---|---|
| 1 | 0 | 2 | 8/14 | 42 | 11 | 13 | 10 | 17 | 2 |
| 2 | 0 | 4 | 8/15 | 44 | 12 | 59 | 36 | 19 | 4 |
| 3 | 0 | 5 | 8/15 | 49 | 11 | 58 | 30 | 21 | 5 |
| 5 | 1 | 10 | 10/15 | 53 | 23 | 95 | 22 | 44 | 10 |
| 10 | 2 | 23 | 15/39 | 58 | 73 | 51 | 49 | 155 | 23 |
| 15 | 2 | 33 | 20/67 | 62 | 123 | 82 | 18 | 310 | 33 |
| 20 | 2 | 37 | 25/71 | 66 | 123 | 145 | 8 | 480 | 37 |
| 25 | 3 | 42 | 30/79 | 74 | 73 | 175 | 0 | 697 | 42 |
| 30 | 3 | 46 | 40/95 | 78 | 133 | 235 | 14 | 887 | 46 |
| 35 | 3 | 50 | 50/95 | 83 | 141 | 307 | 13 | 1027 | 50 |
| 40 | 3 | 53 | 60/96 | 93 | 221 | 365 | 68 | 1117 | 53 |
| 45 | 3 | 54 | 70/96 | 96 | 151 | 653 | 316 | 1157 | 54 |
| 50 | 3 | 70 | 80/96 | 99 | 176 | 787 | 448 | 1345 | 70 |
| 55 | 3 | 74 | 90/96 | 100 | 192 | 1065 | 538 | 1645 | 74 |
| 60 | 3 | 87 | 100/144 | 100 | 437 | 1205 | 698 | 1895 | 87 |

### Coletas por dia: 25 | Chefe derrotado no dia: nunca | Dias simulados: 60

- O Despertar da Coroa: dia 5
- Reino Renascido: dia 18
- Reino da Alta Fantasia: não alcançado

| Dia | Era | Desenv. | Povo | Feliz. | Ouro | Madeira | Pedra | Comida | Obras |
|---|---|---|---|---|---|---|---|---|---|
| 1 | 0 | 2 | 8/14 | 42 | 11 | 13 | 10 | 17 | 2 |
| 2 | 0 | 5 | 8/15 | 44 | 9 | 27 | 22 | 24 | 5 |
| 3 | 0 | 6 | 8/15 | 48 | 10 | 54 | 20 | 31 | 6 |
| 5 | 1 | 8 | 10/15 | 53 | 23 | 89 | 2 | 44 | 8 |
| 10 | 1 | 16 | 15/19 | 60 | 34 | 75 | 17 | 87 | 16 |
| 15 | 1 | 19 | 20/23 | 63 | 114 | 178 | 15 | 154 | 19 |
| 20 | 2 | 31 | 25/67 | 67 | 129 | 155 | 3 | 224 | 31 |
| 25 | 2 | 35 | 30/83 | 71 | 114 | 231 | 25 | 269 | 35 |
| 30 | 2 | 39 | 35/95 | 74 | 116 | 298 | 25 | 289 | 39 |
| 35 | 2 | 56 | 43/95 | 81 | 92 | 203 | 1 | 543 | 56 |
| 40 | 2 | 60 | 53/95 | 91 | 149 | 247 | 14 | 844 | 60 |
| 45 | 2 | 62 | 63/96 | 99 | 239 | 323 | 149 | 1099 | 62 |
| 50 | 2 | 63 | 73/96 | 100 | 290 | 576 | 339 | 1304 | 63 |
| 55 | 2 | 65 | 83/96 | 100 | 260 | 861 | 479 | 1459 | 65 |
| 60 | 2 | 67 | 93/96 | 100 | 312 | 1115 | 607 | 1564 | 67 |
