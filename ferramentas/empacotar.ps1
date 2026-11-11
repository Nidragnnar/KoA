# Monta o pacote para os amigos: pacote\KingdomOfAshes-v<versão>.zip
# Compila em Release, confere que não depende do Visual C++ Redistributable e testa o zip extraído.
# Uso: .\ferramentas\empacotar.ps1 0.1   (sem versão: pacote\KingdomOfAshes-teste.zip)
param([string]$Versao = "")
$ErrorActionPreference = "Stop"
if ($Versao -and $Versao -notmatch '^\d+(\.\d+)+$') { "versão inválida: $Versao (use números, ex.: 0.1)"; exit 1 }
Set-Location (Split-Path $PSScriptRoot)
$nome = "KingdomOfAshes"

& .\ferramentas\compilar.ps1 release | Out-Host
if ($LASTEXITCODE -ne 0) { "PACOTE NÃO GERADO: compilação ou testes falharam"; exit 1 }

$pasta = "pacote\$nome"
if (Test-Path pacote) { Remove-Item pacote -Recurse -Force }
New-Item -ItemType Directory -Force $pasta | Out-Null
Copy-Item build\Game\Release\KingdomOfAshes.exe, build\Game\Release\SDL3.dll $pasta
New-Item -ItemType Directory -Force "$pasta\Assets" | Out-Null
Copy-Item Assets\Sprites, Assets\UI, Assets\Shaders "$pasta\Assets" -Recurse
Copy-Item Data $pasta -Recurse
$guia = Get-Content docs\COMO_JOGAR.txt -Raw -Encoding UTF8
[IO.File]::WriteAllText("$PWD\$pasta\COMO_JOGAR.txt", $guia, (New-Object Text.UTF8Encoding $true))

# Sem runtime do Visual C++ (tudo com /MT)
$dumpbin = Get-ChildItem "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\*\bin\Hostx64\x64\dumpbin.exe" | Select-Object -First 1
$proibidas = @()
foreach ($arquivo in "$nome.exe", "SDL3.dll") {
    $deps = & $dumpbin.FullName /dependents "$pasta\$arquivo" | Where-Object { $_ -match "^\s+\S+\.dll$" } | ForEach-Object { $_.Trim() }
    "$arquivo -> $($deps -join ', ')"
    $proibidas += $deps | Where-Object { $_ -match "^(VCRUNTIME|MSVCP|api-ms-win-crt-)" }
}
if ($proibidas) { "PACOTE NÃO GERADO: depende do runtime do Visual C++: $($proibidas -join ', ')"; exit 1 }
"dependências ok"

$zip = if ($Versao) { "pacote\$nome-v$Versao.zip" } else { "pacote\$nome-teste.zip" }
Add-Type -AssemblyName System.IO.Compression, System.IO.Compression.FileSystem
$arquivoZip = [IO.Compression.ZipFile]::Open("$PWD\$zip", [IO.Compression.ZipArchiveMode]::Create)
try {
    $base = (Resolve-Path pacote).Path
    foreach ($f in Get-ChildItem $pasta -Recurse -File) {
        $entrada = $f.FullName.Substring($base.Length + 1).Replace("\", "/")
        [void][IO.Compression.ZipFileExtensions]::CreateEntryFromFile($arquivoZip, $f.FullName, $entrada, [IO.Compression.CompressionLevel]::Optimal)
    }
} finally { $arquivoZip.Dispose() }

# Teste de fumaça: o zip extraído em outra pasta joga o roteiro inteiro sozinho
$teste = Join-Path $env:TEMP "teste_pacote_$(Get-Random)"
Expand-Archive $zip $teste
$p = Start-Process (Join-Path $teste "$nome\$nome.exe") -ArgumentList "--roteiro", "`"$teste\roteiro`"", "--saves", "`"$teste\saves`"" -Wait -PassThru
$resultado = Get-Content "$teste\roteiro\relatorio.txt" -Encoding UTF8 -ErrorAction SilentlyContinue | Select-Object -Last 1
Remove-Item $teste -Recurse -Force
if ($p.ExitCode -ne 0) { "TESTE DE FUMAÇA FALHOU (saída $($p.ExitCode): $resultado)"; exit 1 }
"teste de fumaça ok ($resultado)"
"$zip  $([math]::Round((Get-Item $zip).Length / 1MB, 1)) MB"
