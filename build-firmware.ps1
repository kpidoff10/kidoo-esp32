# build-firmware.ps1
# Demande le modèle et la version, met à jour default_config.h (FIRMWARE_VERSION),
# build, découpe en parts (2 Mo max) et crée un zip dans firmware\<model>\firmware_<version>.zip
#
# Usage: .\build-firmware.ps1
#        .\build-firmware.ps1 -Model dream -Version 1.0.2
#        .\build-firmware.ps1 dream 1.0.2

param(
    [Parameter(Position = 0, Mandatory = $false)]
    [string]$Model = "",
    [Parameter(Position = 1, Mandatory = $false)]
    [string]$Version = ""
)

# Liste des modèles : répertoires src/models/* sauf common (aligné avec models.yaml)
$ValidModels = (Get-ChildItem -Path (Join-Path $PSScriptRoot "src\models") -Directory -ErrorAction SilentlyContinue | Where-Object { $_.Name -ne "common" } | Select-Object -ExpandProperty Name)
if (-not $ValidModels) { $ValidModels = @("dream", "gotchi") }

$PART_MAX_SIZE_BYTES = 2 * 1024 * 1024   # 2 Mo par part

# Détecter PlatformIO CLI
$pioPath = "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe"
if (-not (Test-Path $pioPath)) {
    $pioCmd = Get-Command pio -ErrorAction SilentlyContinue
    if ($pioCmd) {
        $pioPath = $pioCmd.Source
    } else {
        Write-Error "PlatformIO introuvable. Installez l'extension PlatformIO IDE ou ajoutez pio au PATH."
        exit 1
    }
}

# Demander le modèle si non fourni
if ([string]::IsNullOrWhiteSpace($Model)) {
    $modelList = $ValidModels -join " / "
    do {
        $Model = Read-Host "Modele ($modelList)"
        $Model = $Model.Trim().ToLower()
    } while ($Model -notin $ValidModels)
} else {
    $Model = $Model.Trim().ToLower()
    if ($Model -notin $ValidModels) {
        Write-Error "Modele invalide: $Model. Valides: $($ValidModels -join ', ')"
        exit 1
    }
}

# Chemin du fichier de config par modèle
$ConfigPath = Join-Path $PSScriptRoot "src\models\$Model\config\default_config.h"

# Demander la version si non fournie
if ([string]::IsNullOrWhiteSpace($Version)) {
    $current = "1.0.0"
    if (Test-Path $ConfigPath) {
        $line = Get-Content $ConfigPath | Where-Object { $_ -match 'FIRMWARE_VERSION\s+"([^"]+)"' }
        if ($line) { $current = $matches[1] }
    }
    $Version = Read-Host "Version du firmware (ex: 1.0.2) [actuelle: $current]"
    if ([string]::IsNullOrWhiteSpace($Version)) { $Version = $current }
}

# Valider le format (chiffres et points, optionnel -suffix)
if ($Version -notmatch '^[\d.]+(-[a-zA-Z0-9.]+)?$') {
    Write-Error "Version invalide. Utilise par ex: 1.0.0 ou 1.0.1-beta"
    exit 1
}

# Mettre à jour default_config.h du modèle
if (-not (Test-Path $ConfigPath)) {
    Write-Error "Fichier non trouvé: $ConfigPath"
    exit 1
}

$content = Get-Content $ConfigPath -Raw
$newContent = $content -replace '(#define\s+FIRMWARE_VERSION\s+")[^"]*(")', ('${1}' + $Version + '${2}')
if ($content -eq $newContent) {
    Write-Host "FIRMWARE_VERSION inchangé ou motif non trouvé."
} else {
    Set-Content -Path $ConfigPath -Value $newContent -NoNewline
    Write-Host "FIRMWARE_VERSION mis à jour: $Version (modèle: $Model)"
}

$versionSafe = $Version -replace '[^a-zA-Z0-9._-]', '_'
$buildDir = Join-Path $PSScriptRoot ".pio\build\$Model"
$partsDir = Join-Path $buildDir "parts"
$zipPath = Join-Path $buildDir "firmware_$Model.zip"

Push-Location $PSScriptRoot
try {
    Write-Host ""
    Write-Host "Build en cours (modele: $Model, version: $Version)..." -ForegroundColor Cyan
    & $pioPath run -e $Model
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Build échoué."
        exit $LASTEXITCODE
    }

    $binPath = Join-Path $buildDir "firmware.bin"
    if (-not (Test-Path $binPath)) {
        Write-Error "Fichier non généré: $binPath"
        exit 1
    }

    $info = Get-Item $binPath
    $totalSize = $info.Length
    Write-Host "Firmware généré: $binPath"
    Write-Host "Taille: $([math]::Round($totalSize / 1KB, 2)) Ko"

    # Découper en parts (max 2 Mo chacune)
    $bytes = [System.IO.File]::ReadAllBytes($binPath)
    $partCount = [math]::Ceiling($totalSize / $PART_MAX_SIZE_BYTES)
    if (Test-Path $partsDir) { Remove-Item $partsDir -Recurse -Force }
    New-Item -ItemType Directory -Path $partsDir -Force | Out-Null

    for ($i = 0; $i -lt $partCount; $i++) {
        $start = $i * $PART_MAX_SIZE_BYTES
        $length = [math]::Min($PART_MAX_SIZE_BYTES, $totalSize - $start)
        $partBytes = $bytes[$start..($start + $length - 1)]
        $partPath = Join-Path $partsDir "part$i.bin"
        [System.IO.File]::WriteAllBytes($partPath, $partBytes)
        Write-Host "  Part $i : part$i.bin ($([math]::Round($length / 1KB, 2)) Ko)"
    }

    # Créer le .zip (contenu: part0.bin, part1.bin, ... à la racine)
    if (Test-Path $zipPath) { Remove-Item $zipPath -Force }
    $partFiles = Get-ChildItem -Path (Join-Path $partsDir "*.bin") | Sort-Object Name
    Compress-Archive -Path $partFiles.FullName -DestinationPath $zipPath -CompressionLevel Optimal

    # Déplacer le zip dans firmware\<model>\
    $modelFirmwareDir = Join-Path (Join-Path $PSScriptRoot "firmware") $Model
    if (-not (Test-Path $modelFirmwareDir)) { New-Item -ItemType Directory -Path $modelFirmwareDir -Force | Out-Null }
    $destZip = Join-Path $modelFirmwareDir "firmware_$versionSafe.zip"
    Move-Item -Path $zipPath -Destination $destZip -Force

    $zipInfo = Get-Item $destZip
    Write-Host ""
    Write-Host "Zip OTA créé et déplacé : $destZip" -ForegroundColor Green
    Write-Host "  Modele: $Model  |  Version: $Version"
    Write-Host "  Parts: $partCount  (max $([math]::Round($PART_MAX_SIZE_BYTES / 1MB, 2)) Mo par part)"
    Write-Host "  Taille zip: $([math]::Round($zipInfo.Length / 1KB, 2)) Ko"
    Write-Host "  Upload ce .zip dans l'admin (Firmware) avec la version $Version."
} finally {
    Pop-Location
}
