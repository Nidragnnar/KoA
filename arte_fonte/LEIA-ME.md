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
- `prancha`: vários quadros numa imagem só (tentado no herói; desativado, veja o `prompts.json`).

## O que veio do NidraIA

- Fase 1–4: casas (n1–n4), castelo, árvores, rochas, baús, túmulos, poço e terreno.
- Fase 5: moradia, taverna, mercado, capela, forja, posto, torre e campo (n1–n3), canteiros `obra_*` e `barraco`.
- Ficam procedurais: herói, NPCs e aldeões (nas pranchas os quadros saem diferentes entre si e os rostos somem),
  fogueira (a prancha de 4 quadros saiu sem chama), ícones, tocha, cerca, arbusto, toco, luz e sombra.
