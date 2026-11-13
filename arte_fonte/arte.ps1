# Gera a arte final no NidraIA (http://localhost:3333) e converte para Assets/Arte/<nome>.png
# Uso: .\arte_fonte\arte.ps1 [-Nomes casa_n1,terreno_0] [-SoConverter] [-Refazer]
#   -Nomes        só estas entradas do prompts.json (padrão: todas)
#   -SoConverter  não chama o NidraIA; reconverte o que já está em arte_fonte\brutas
#   -Refazer      gera de novo mesmo que a imagem bruta já exista
# Depois rode .\ferramentas\compilar.ps1 debug: o AssetGen põe a arte importada no atlas.
param([string[]]$Nomes, [switch]$SoConverter, [switch]$Refazer, [string]$Servidor = "http://localhost:3333")
$ErrorActionPreference = "Stop"
$raiz = Split-Path $PSScriptRoot
$fonte = $PSScriptRoot
Add-Type -AssemblyName System.Drawing
if (-not ("Arte" -as [type])) { Add-Type -Path "$fonte\Arte.cs" -ReferencedAssemblies System.Drawing }

$cfg = Get-Content "$fonte\prompts.json" -Raw -Encoding UTF8 | ConvertFrom-Json
$pastaProc = "$fonte\procedurais"; $pastaBases = "$fonte\bases"; $pastaBrutas = "$fonte\brutas"; $pastaArte = "$raiz\Assets\Arte"
New-Item -ItemType Directory -Force $pastaBases, $pastaBrutas, $pastaArte | Out-Null

# Sprites procedurais atuais (referência de tamanho, silhueta e brilho)
$assetGen = @("$raiz\build\Tools\Debug\AssetGen.exe", "$raiz\build\Tools\Release\AssetGen.exe") | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $assetGen) { throw "Compile antes: .\ferramentas\compilar.ps1 debug" }
& $assetGen "$env:TEMP\koa_atlas_procedural" --procedurais $pastaProc | Out-Null

$fundo = [Convert]::ToInt32($cfg.fundo.TrimStart('#'), 16)
function Valor($s, $campo) { if ($null -ne $s.$campo) { $s.$campo } else { $cfg.padroes.$campo } }

function Gerar($s, $prompt, $base) {
    $corpo = @{
        prompt = $prompt; negativo = $(if ($s.negativo) { $s.negativo } else { $cfg.negativo }); estilo = $cfg.estilo; traduzir = $false
        largura = (Valor $s "largura"); altura = (Valor $s "altura"); seed = $s.seed; quantidade = 1
    }
    if ($base) {
        $corpo.imagemBase = "data:image/png;base64," + [Convert]::ToBase64String([IO.File]::ReadAllBytes($base))
        $corpo.forca = (Valor $s "forca")
    }
    $json = [Text.Encoding]::UTF8.GetBytes(($corpo | ConvertTo-Json -Compress))
    $t = Invoke-RestMethod "$Servidor/api/gerar" -Method Post -Body $json -ContentType "application/json; charset=utf-8"
    $inicio = Get-Date
    do {
        Start-Sleep -Milliseconds 800
        $r = Invoke-RestMethod "$Servidor/api/trabalhos/$($t.id)"
    } while ($r.status -notin "completed", "failed", "cancelled")
    if ($r.status -ne "completed") { throw "$($s.nome): geração falhou: $($r.erro)" }
    $img = $r.imagens[0]
    Invoke-WebRequest "$Servidor/galeria/$($img.arquivo)" -OutFile "$pastaBrutas\$($s.nome).png" -UseBasicParsing
    # Prompt e configurações ao lado da bruta; tira da galeria do NidraIA para não poluir
    $img | ConvertTo-Json -Depth 5 | Set-Content "$pastaBrutas\$($s.nome).json" -Encoding UTF8
    Invoke-RestMethod "$Servidor/api/galeria/$($img.id)" -Method Delete | Out-Null
    "{0}: gerado em {1:N0} s" -f $s.nome, ((Get-Date) - $inicio).TotalSeconds
}

if (-not $SoConverter) {
    $estado = Invoke-RestMethod "$Servidor/api/estado"
    if ($estado.modelo -ne $cfg.modelo) {
        Invoke-RestMethod "$Servidor/api/modelo" -Method Post -Body (@{ id = $cfg.modelo } | ConvertTo-Json) -ContentType "application/json" | Out-Null
        do { Start-Sleep 2; $estado = Invoke-RestMethod "$Servidor/api/estado" } while ($estado.estado -eq "carregando")
    }
}

foreach ($s in $cfg.sprites) {
    if ($Nomes -and $s.nome -notin $Nomes) { continue }
    if ($s.desativado) { "$($s.nome): desativado ($($s.desativado))"; continue }
    $bruta = "$pastaBrutas\$($s.nome).png"
    $contorno = if ((Valor $s "contorno")) { [Convert]::ToInt32((Valor $s "contorno").TrimStart('#'), 16) } else { 0 }
    # "comum" pode ser o texto ou o nome de um texto do topo do prompts.json (ex.: "comumVistas")
    $comum = if ($s.comum -and $cfg.($s.comum)) { $cfg.($s.comum) } elseif ($s.comum) { $s.comum } else { @{ base = $cfg.comum; textura = $cfg.comumTextura; prancha = $cfg.comumPrancha }[$s.modo] }
    $prompt = "$($s.prompt), $comum"
    # Era 1 (começo do jogo): clima sombrio; níveis da casa de eras seguintes usam "era": ""
    if ((Valor $s "era") -eq "sombria") { $prompt += ", $($cfg.eraSombria)" }

    if ($s.modo -eq "textura") {
        $alvos = 0..3 | ForEach-Object { "$($s.nome)_$_" }
        if (-not $SoConverter -and ($Refazer -or -not (Test-Path $bruta))) { Gerar $s $prompt $null }
        [Arte]::ConverterTiles($bruta, ($alvos | ForEach-Object { "$pastaProc\$_.png" }), ($alvos | ForEach-Object { "$pastaArte\$_.png" }),
            $s.reducao, (Valor $s "cores"), (Valor $s "saturacao"), (Valor $s "ajusteBrilho"), [bool]$s.agua)
        continue
    }
    $alvos = if ($s.alvos) { @($s.alvos) } else { @($s.nome) }
    $procs = [string[]]($alvos | ForEach-Object { "$pastaProc\$_.png" })
    $base = "$pastaBases\$($s.nome).png"
    if ($s.modo -eq "prancha") { $pos = [Arte]::MontarPrancha($procs, $(if ($s.colunas) { $s.colunas } else { 6 }), (Valor $s "largura"), (Valor $s "altura"), $fundo, $base) }
    else { $pos = [Arte]::MontarBase($procs[0], (Valor $s "largura"), (Valor $s "altura"), $fundo, $base) }
    if (-not $SoConverter -and ($Refazer -or -not (Test-Path $bruta))) { Gerar $s $prompt $base }
    [Arte]::ConverterObjetos($bruta, $procs, [string[]]($alvos | ForEach-Object { "$pastaArte\$_.png" }), $pos, $fundo,
        (Valor $s "mascara"), (Valor $s "cores"), (Valor $s "saturacao"), (Valor $s "ajusteBrilho"), $contorno, [bool]$s.caixa, [bool]$s.moda)
    # Pele lisa (cor dominante da arte) e, depois, olhos, joias etc.: cores do procedural que voltam por cima
    if ($s.uniformizar) {
        $cores = [int[]]($s.uniformizar | ForEach-Object { [Convert]::ToInt32($_.TrimStart('#'), 16) })
        foreach ($a in $alvos) { [Arte]::UniformizarCores("$pastaProc\$a.png", "$pastaArte\$a.png", $cores) }
    }
    if ($s.manter) {
        $cores = [int[]]($s.manter | ForEach-Object { [Convert]::ToInt32($_.TrimStart('#'), 16) })
        foreach ($a in $alvos) { [Arte]::ManterCores("$pastaProc\$a.png", "$pastaArte\$a.png", $cores) }
    }
    # Quadros de animação: cada alvo "<prefixo>_0" gera os irmãos "<prefixo>_*" a partir da arte dele
    if ($s.derivar) {
        foreach ($a in $alvos) {
            $prefixo = $a -replace '0$', ''
            $irmaos = @(Get-ChildItem $pastaProc -Filter "$prefixo*.png" | ForEach-Object { $_.BaseName } | Where-Object { $_ -ne $a })
            if ($irmaos.Count -eq 0) { continue }
            [Arte]::DerivarQuadros("$pastaArte\$a.png", "$pastaProc\$a.png", [string[]]($irmaos | ForEach-Object { "$pastaProc\$_.png" }),
                [string[]]($irmaos | ForEach-Object { "$pastaArte\$_.png" }), $(if ($s.alcance) { $s.alcance } else { 3 }))
            "  $a -> $($irmaos -join ', ')"
        }
    }
    "$($s.nome): convertido"
}
