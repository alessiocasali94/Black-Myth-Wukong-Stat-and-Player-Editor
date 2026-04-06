param(
    [string]$ProjectPath = "C:\Users\aboba\source\repos\Project3\Project3\Project3.vcxproj",
    [string]$Configuration = "Release",
    [string]$Platform = "x64",
    [string]$OutputDir = "C:\Users\aboba\Desktop\wikong",
    [string]$GitHubDir = "C:\Users\aboba\Desktop\vbcvbcvbv"
)

$ErrorActionPreference = "Stop"

function Get-SignToolPath {
    $candidates = @(
        "${env:ProgramFiles(x86)}\Windows Kits\10\bin",
        "$env:ProgramFiles\Windows Kits\10\bin"
    )

    foreach ($root in $candidates) {
        if (-not (Test-Path $root)) {
            continue
        }

        $tool = Get-ChildItem $root -Recurse -Filter signtool.exe -File | Sort-Object FullName -Descending | Select-Object -First 1
        if ($tool) {
            return $tool.FullName
        }
    }

    throw "signtool.exe was not found."
}

function Resolve-SigningConfiguration {
    if ($env:WIKONG_CERT_PFX -and (Test-Path $env:WIKONG_CERT_PFX) -and $env:WIKONG_CERT_PASSWORD) {
        return @{
            PfxPath = $env:WIKONG_CERT_PFX
            Password = $env:WIKONG_CERT_PASSWORD
            Subject = "external"
            Thumbprint = ""
            Source = "external"
        }
    }

    return $null
}

$projectDirectory = Split-Path $ProjectPath -Parent
$signTool = $null

& msbuild $ProjectPath /p:Configuration=$Configuration /p:Platform=$Platform

$builtExe = Join-Path $projectDirectory "$Platform\$Configuration\BlackMythWukongTrainer.exe"
if (-not (Test-Path $builtExe)) {
    throw "Built executable was not found at $builtExe"
}

New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
$packagedExe = Join-Path $OutputDir "BlackMythWukongTrainer.exe"
Copy-Item $builtExe $packagedExe -Force

$signing = Resolve-SigningConfiguration
if ($signing -ne $null) {
    $signTool = Get-SignToolPath
    & $signTool sign /fd SHA256 /f $signing.PfxPath /p $signing.Password $packagedExe
}

$hash = Get-FileHash $packagedExe -Algorithm SHA256
$hash.Hash | Set-Content (Join-Path $OutputDir "BlackMythWukongTrainer.sha256.txt")

[PSCustomObject]@{
    Executable = $packagedExe
    SignedWith = if ($signing -ne $null) { $signing.Source } else { "unsigned" }
    CertificateSubject = if ($signing -ne $null) { $signing.Subject } else { "" }
    CertificateThumbprint = if ($signing -ne $null) { $signing.Thumbprint } else { "" }
    Sha256 = $hash.Hash
} | Format-List | Out-String | Set-Content (Join-Path $OutputDir "package-info.txt")

Write-Host "Packaged executable: $packagedExe"
Write-Host "Signing source: $(if ($signing -ne $null) { $signing.Source } else { 'unsigned' })"

# Copy source files for GitHub
$sourceFiles = @(
    "main.cpp",
    "BlackMythWukongPseudoTrainerPanel.h",
    "BlackMythWukongPseudoTrainerPanel.cpp",
    "resource.h",
    "Project3.rc",
    "app.ico",
    "Project3.vcxproj",
    "Project3.vcxproj.filters",
    "README.md",
    ".gitignore",
    "package.ps1"
)

New-Item -ItemType Directory -Path $GitHubDir -Force | Out-Null
Copy-Item $packagedExe (Join-Path $GitHubDir "BlackMythWukongTrainer.exe") -Force

foreach ($file in $sourceFiles) {
    $src = Join-Path $projectDirectory $file
    if (Test-Path $src) {
        Copy-Item $src (Join-Path $GitHubDir $file) -Force
    }
}

Write-Host "GitHub files copied to: $GitHubDir"
