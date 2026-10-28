$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot
if (-not (Get-Command dotnet -ErrorAction SilentlyContinue)) {
    Write-Host 'The .NET 8 SDK is required. Install it from https://dotnet.microsoft.com/download/dotnet/8.0' -ForegroundColor Yellow
    exit 1
}
Write-Host 'Publishing standalone Windows x64 release...' -ForegroundColor Cyan
& dotnet publish .\src\LSVJOverlay.csproj -c Release -r win-x64 --self-contained true -p:PublishSingleFile=true -p:IncludeNativeLibrariesForSelfExtract=true -o .\publish
if ($LASTEXITCODE -ne 0) { throw "Publish failed with exit code $LASTEXITCODE" }
Write-Host 'Success! Launch .\publish\LSVJOverlay.exe (RTSS must also be running).' -ForegroundColor Green
