# Arte final pelo NidraIA

A arte do jogo sai do NidraIA (gerador de imagens local, `Z:\NidraIA`) e vai para `Assets/Arte/<nome>.png`.
O `AssetGen` usa esse PNG no lugar do sprite procedural quando ele existe com o tamanho exato do sprite;
o que não tem arte importada continua procedural.

## Como refazer

1. Abra o `NidraIA.exe` (porta 3333). O script troca para o FLUX.2 klein se precisar.
2. `.\arte_fonte\arte.ps1` gera o que falta e converte tudo.
   - `-Nomes casa_n2,poco -Refazer` gera de novo só esses
   - `-SoConverter` só reconverte as imagens já geradas (sem o NidraIA)
3. `.\ferramentas\compilar.ps1 debug` monta o atlas com a arte nova.

## Arquivos

| Arquivo | O que é |
| --- | --- |
| `prompts.json` | prompts, sementes e ajustes de cada sprite (versionado: dá para repetir) |
| `arte.ps1` | chama a API do NidraIA e o conversor |
| `Arte.cs` | conversor: recorte, fundo magenta transparente, redução por área, paleta limitada, contorno |
| `brutas/` | imagens como saíram do NidraIA, com o `.json` de cada uma (não vai para o Git) |
| `bases/`, `procedurais/` | imagens de trabalho (não vão para o Git) |

## Modos

- `base`: o sprite procedural ampliado vai como imagem de partida (força 0,62 a 0,9), e o resultado é
  recortado na mesma posição. Assim a construção mantém tamanho e ancoragem (base embaixo, no centro).
  Com `"caixa": true` o objeto desenhado é recortado pela própria caixa e encaixado no sprite (objetos pequenos).
- `textura`: gera uma textura, reduz 16×, recorta 4 variantes e funde as bordas para emendar sem costura.
  A água usa a mesma variante deslocada em cada quadro, então a animação também emenda.
- `prancha`: vários quadros numa imagem só (`colunas` define a grade). Com muitos quadros pequenos cada um vira
  outro personagem (herói e aldeões em 12–18 quadros; desativados, veja o `prompts.json`). Com 3 quadros grandes
  (`"comum": "comumVistas"`: frente, costas e lado do mesmo personagem) o NidraIA mantém o personagem.

## Personagens e criaturas

O NidraIA desenha só o quadro parado de cada direção (`heroi_baixo_0`, `heroi_cima_0`, `heroi_lado_0`) numa prancha
de 3 vistas; os outros quadros saem por código:

- `"derivar": true`: cada alvo `<prefixo>_0` gera os irmãos `<prefixo>_*` (andar, golpe, espada; quadro 1 das criaturas).
  O `DerivarQuadros` (em `Arte.cs`) acha, para cada pixel do quadro procedural, de onde ele veio no quadro 0 procedural
  (corpo que sobe, perna e braço que mudam) e copia a arte do NidraIA dessa posição. O que não existe no quadro 0
  (machado, espada) fica com as cores do procedural. Assim o personagem é o mesmo em todos os quadros.
- `"mascara": "procedural"`: a silhueta é a do procedural (tamanho, pés embaixo no centro, contorno iguais).
- `"moda": true`: na redução cada pixel pega a cor que domina o bloco, sem misturar cabelo, faixa e pele.
- `"uniformizar"`: cores do procedural (a pele) que viram uma cor lisa da arte: em 16×24 os olhos grandes da IA
  viravam uma faixa escura. `"manter"`: cores do procedural que voltam por cima (olhos, joia do diadema).
- Mudou a animação no `Sprites.cpp`? Rode `.\arte_fonte\arte.ps1 -SoConverter` para derivar os quadros de novo.

## O que veio do NidraIA

- Fase 1–4: casas (n1–n4), castelo, árvores, rochas, baús, túmulos, poço e terreno.
- Fase 5: moradia, taverna, mercado, capela, forja, posto, torre e campo (n1–n3), canteiros `obra_*` e `barraco`.
- Personagens: herói (andar, golpe e espada), os 5 moradores, os 4 aldeões, sombra, lobo, cavaleiro caído e dragão.
  Personagens sem a era sombria (`"era": ""`): os verdes de musgo pintavam pele e roupa de verde.
  Gere as pranchas de 3 vistas em 1536×1024: em 1024×768 sai bem mais rápido (30 s contra 6 min), mas com manchas.
- 1.16 e 1.17: as 6 criaturas novas (arqueiro espectral, matilha, conjurador de ossos, ossada, cuspidora e enxame) e os
  chefes dos atos (Sacerdotisa das Cinzas e Varek), cada uma com o seu procedural em `criaturasDosAtos` (antes eram
  sprites de outras criaturas tingidos). Com força 0,6 o NidraIA mudava a pose (o lobo de costas, a ossada maior que a
  silhueta) e a máscara procedural cortava a arte no lugar errado: com 0,42 a 0,45 a arte segue a base.
  Varek ficou com a versão de 0,6 (escolhida no teste).
- Ficam procedurais: esqueleto (o vão entre os ossos sai preto e vira um vulto), fogueira (a prancha de 4 quadros
  saiu sem chama), ícones, tocha, cerca, arbusto, toco, luz e sombra.
