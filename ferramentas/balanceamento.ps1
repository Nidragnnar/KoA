# Roda a simulação de balanceamento em vários ritmos de jogo e grava docs\BALANCEAMENTO.md
Set-Location (Split-Path $PSScriptRoot)
$env:PATH = "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;" + $env:PATH
cmake --build --preset release --target Balanceamento *> build\balanceamento.log
if ($LASTEXITCODE -ne 0) { "COMPILAÇÃO FALHOU (veja build\balanceamento.log)"; exit 1 }
$md = "docs\BALANCEAMENTO.md"
@"
# Balanceamento (simulação)

Gerado por ``ferramentas\balanceamento.ps1``: um jogador automático (``Tools\Balanceamento\JogadorAutomatico``)
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
"@ | Set-Content $md -Encoding UTF8
foreach ($cenario in @(@(12, 30), @(25, 25), @(40, 15), @(25, 0))) {
    & build\Tools\Release\Balanceamento.exe Data 120 $cenario[0] $cenario[1] $md | Select-Object -First 12
}
"gravado em $md"
