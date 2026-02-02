# build-firmware.ps1
# Demande la version, met à jour default_config.h (FIRMWARE_VERSION), build et copie dans firmware/firmware_X_Y_Z.bin
#
# Usage: .\build-firmware.ps1
#        .\build-firmware.ps1 -Version 1.0.2
#        .\build-firmware.ps1 -Version 1.0.2 -Env basic

param(
    [string]$Version = "",
    [ValidateSet("dream", "basic", "mini")]
    [string]$Env = "dream"
)

$ConfigPath = Join-Path $PSScriptRoot "src\models\common\config\default_config.h"
$BuildDir = Join-Path $PSScriptRoot ".pio\build\$Env"

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

# Mettre à jour default_config.h
if (-not (Test-Path $ConfigPath)) {
    Write-Error "Fichier non trouvé: $ConfigPath"
    exit 1
}

$content = Get-Content $ConfigPath -Raw
# Utiliser des chaînes à guillemets simples pour que ${1} et ${2} soient passés tels quels au moteur -replace (références aux groupes de capture)
$newContent = $content -replace '(#define\s+FIRMWARE_VERSION\s+")[^"]*(")', ('${1}' + $Version + '${2}')
if ($content -eq $newContent) {
    Write-Host "FIRMWARE_VERSION inchangé ou motif non trouvé."
} else {
    Set-Content -Path $ConfigPath -Value $newContent -NoNewline
    Write-Host "FIRMWARE_VERSION mis à jour: $Version"
}

# Répertoire de sortie = firmware/, fichier = firmware_1_0_3.bin (versionné par git)
$versionUnderscore = $Version -replace '\.', '_' -replace '-', '_'
$outputName = "firmware_$versionUnderscore.bin"
$outputDir = Join-Path $PSScriptRoot "firmware"
$outputPath = Join-Path $outputDir $outputName

# Build
Push-Location $PSScriptRoot
try {
    Write-Host "Build en cours (env: $Env)..."
    & pio run -e $Env
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Build échoué."
        exit $LASTEXITCODE
    }

    $binPath = Join-Path $BuildDir "firmware.bin"
    if (-not (Test-Path $binPath)) {
        Write-Error "Fichier non généré: $binPath"
        exit 1
    }

    New-Item -ItemType Directory -Path $outputDir -Force | Out-Null
    Copy-Item -Path $binPath -Destination $outputPath -Force
    Write-Host "OK: firmware\$outputName"
} finally {
    Pop-Location
}
