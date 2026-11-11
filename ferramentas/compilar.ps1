# Compila, regenera a arte e roda os testes; uso: .\ferramentas\compilar.ps1 [debug|release]
param([string]$Config = "debug")
$env:PATH = "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;" + $env:PATH
Set-Location (Split-Path $PSScriptRoot)
if (-not (Test-Path build\CMakeCache.txt)) { cmake --preset vs *> build_config.log; if ($LASTEXITCODE -ne 0) { "CONFIGURAÇÃO FALHOU (veja build_config.log)"; exit 1 } }
cmake --build --preset $Config *> build\compilar.log
$codigo = $LASTEXITCODE
# Só avisos e erros do nosso código (fora de _deps)
Select-String -Path build\compilar.log -Pattern "warning|error" |
    Where-Object { $_.Line -notmatch "_deps" } |
    Select-Object -First 40 | ForEach-Object { $_.Line }
if ($codigo -ne 0) { "COMPILAÇÃO FALHOU (código $codigo)"; exit $codigo }
"compilação ok"
$pasta = (Get-Culture).TextInfo.ToTitleCase($Config)
& "build\Tools\$pasta\AssetGen.exe" Assets\Sprites
ctest --preset $Config *> build\testes.log
$codigo = $LASTEXITCODE
Get-Content build\testes.log | Select-String -Pattern "tests passed|Failed|FAILED|\*\*\*" | Select-Object -First 40 | ForEach-Object { $_.Line }
exit $codigo
