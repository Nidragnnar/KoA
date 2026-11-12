# Roda o teste ponta a ponta do jogo (joga sozinho, tira capturas e confere tudo)
# Uso: .\ferramentas\roteiro.ps1 [-Release]   → capturas\roteiro\*.png + relatorio.txt
param([switch]$Release)
Set-Location (Split-Path $PSScriptRoot)
# Áudio de mentira nos testes: todo o caminho do som roda, mas nada sai nos alto-falantes
$env:SDL_AUDIODRIVER = "dummy"
$config = if ($Release) { "Release" } else { "Debug" }
$pasta = "$PWD\capturas\roteiro"
if (Test-Path $pasta) { Remove-Item $pasta -Recurse -Force }
New-Item -ItemType Directory -Force $pasta | Out-Null
# Saves numa pasta própria: o roteiro nunca toca nos saves de verdade
$p = Start-Process "$PWD\build\Game\$config\KingdomOfAshes.exe" -ArgumentList "--roteiro", "`"$pasta`"", "--saves", "`"$pasta\saves`"" -Wait -PassThru
Add-Type -AssemblyName System.Drawing
foreach ($bmp in Get-ChildItem $pasta -Filter *.bmp) {
    $img = [Drawing.Image]::FromFile($bmp.FullName)
    $img.Save(($bmp.FullName -replace '\.bmp$', '.png'), [Drawing.Imaging.ImageFormat]::Png)
    $img.Dispose()
    Remove-Item $bmp.FullName
}
Get-Content "$pasta\relatorio.txt" -Encoding UTF8 -ErrorAction SilentlyContinue
"código de saída: $($p.ExitCode)"
exit $p.ExitCode
