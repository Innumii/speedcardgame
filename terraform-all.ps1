[CmdletBinding()]
param(
    [Parameter(Mandatory = $true, Position = 0)]
    [ValidateSet("apply", "destroy")]
    [string]$Action,

    [switch]$AutoApprove,

    [switch]$SkipInit
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$sharedSecretsFile = Join-Path $repoRoot "infra/secrets.tfvars"

$applyOrder = @(
    "infra/data",
    "infra/services/alb",
    "infra/services/auth",
    "infra/services/cards",
    "infra/services/server"
)

$stackOrder = @($applyOrder)
if ($Action -eq "destroy") {
    [array]::Reverse($stackOrder)
}

foreach ($relativePath in $stackOrder) {
    $stackPath = Join-Path $repoRoot $relativePath
    $mainTfPath = Join-Path $stackPath "main.tf"

    if (-not (Test-Path $mainTfPath)) {
        Write-Host "Skipping $relativePath (no main.tf found)." -ForegroundColor Yellow
        continue
    }

    Write-Host "`n==> Running terraform $Action in $relativePath" -ForegroundColor Cyan
    Push-Location $stackPath
    try {
        if (-not $SkipInit) {
            terraform init -upgrade
            if ($LASTEXITCODE -ne 0) {
                throw "terraform init failed in $relativePath"
            }
        }

        $tfArgs = @($Action)
        if (Test-Path $sharedSecretsFile) {
            $tfArgs += "-var-file=$sharedSecretsFile"
        }

        $stackSecretsFile = Join-Path $stackPath "secrets.tfvars"
        if ((Test-Path $stackSecretsFile) -and ((Resolve-Path $stackSecretsFile).Path -ne (Resolve-Path $sharedSecretsFile).Path)) {
            $tfArgs += "-var-file=$stackSecretsFile"
        }
        if ($AutoApprove) {
            $tfArgs += "-auto-approve"
        }

        & terraform @tfArgs
        if ($LASTEXITCODE -ne 0) {
            throw "terraform $Action failed in $relativePath"
        }
    }
    finally {
        Pop-Location
    }
}

Write-Host "`nCompleted terraform $Action for all stacks." -ForegroundColor Green