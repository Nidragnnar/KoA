# Roda a simulação de balanceamento em vários ritmos de jogo e grava docs\BALANCEAMENTO.md
Set-Location (Split-Path $PSScriptRoot)
$env:PATH = "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;" + $env:PATH
cmake --build --preset release --target Balanceamento *> build\balanceamento.log
if ($LASTEXITCODE -ne 0) { "COMPILAÇÃO FALHOU (veja build\balanceamento.log)"; exit 1 }
$md = "docs\BALANCEAMENTO.md"
@"
# Balanceamento (simulação)

Gerado por ``ferramentas\balanceamento.ps1``: um jogador automático joga com a lógica real do jogo
(``Tools\Balanceamento``). Toda manhã ele coleta N árvores, rochas ou baús (o mais perto do que falta),
compra pedra no mercado com o ouro que sobra, constrói e melhora por prioridade, usa imposto baixo
quando o povo está abaixo de 50 de felicidade, e dorme.

**Suposições (estimativas, não medidas de gente jogando):** cada coleta leva uns 10 s de jogo
(andar + golpes), então 12/25/40 coletas por dia ≈ 2/4/7 minutos de coleta num dia de 14 minutos.
O jogador automático não luta: a derrota do Cavaleiro Caído é marcada num dia fixo.
As estações (7 dias cada) valem na simulação. O jogador automático constrói muitas hortas, então o
inverno quase não o afeta; quem tem poucas hortas sente a comida cair (ex.: 1 horta e 15 moradores = -13 por dia).
"@ | Set-Content $md -Encoding UTF8
foreach ($cenario in @(@(12, 30), @(25, 25), @(40, 15), @(25, 0))) {
    & build\Tools\Release\Balanceamento.exe Data 60 $cenario[0] $cenario[1] $md | Select-Object -First 6
}
"gravado em $md"
