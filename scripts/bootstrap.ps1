$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$Deps = Join-Path $Root "deps"
$Tmp  = Join-Path $Deps "_downloads"
New-Item -ItemType Directory -Force -Path $Deps, $Tmp, (Join-Path $Deps "models") | Out-Null

function Download-File([string]$Url, [string]$Out) {
    if (Test-Path $Out) { return }
    Write-Host "Downloading $Url"
    & curl.exe -L --fail --retry 4 --retry-delay 2 --connect-timeout 30 -o $Out $Url
    if ($LASTEXITCODE -ne 0) { throw "Download failed: $Url" }
}

$JuceDir = Join-Path $Deps "JUCE"
if (-not (Test-Path (Join-Path $JuceDir "CMakeLists.txt"))) {
    $zip = Join-Path $Tmp "juce-9.0.2.zip"
    Download-File "https://github.com/juce-framework/JUCE/archive/refs/tags/9.0.2.zip" $zip
    $extract = Join-Path $Tmp "juce_extract"
    Remove-Item -Recurse -Force $extract -ErrorAction SilentlyContinue
    Expand-Archive -Path $zip -DestinationPath $extract -Force
    Remove-Item -Recurse -Force $JuceDir -ErrorAction SilentlyContinue
    Move-Item (Join-Path $extract "JUCE-9.0.2") $JuceDir
}

$OrtDir = Join-Path $Deps "onnxruntime"
if (-not (Test-Path (Join-Path $OrtDir "include\onnxruntime_cxx_api.h"))) {
    $zip = Join-Path $Tmp "onnxruntime-win-x64-1.29.0.zip"
    Download-File "https://github.com/microsoft/onnxruntime/releases/download/v1.29.0/onnxruntime-win-x64-1.29.0.zip" $zip
    $extract = Join-Path $Tmp "ort_extract"
    Remove-Item -Recurse -Force $extract -ErrorAction SilentlyContinue
    Expand-Archive -Path $zip -DestinationPath $extract -Force
    $found = Get-ChildItem -Directory $extract | Select-Object -First 1
    if (-not $found) { throw "ONNX Runtime archive layout not recognized." }
    Remove-Item -Recurse -Force $OrtDir -ErrorAction SilentlyContinue
    Move-Item $found.FullName $OrtDir
}

$Model = Join-Path $Deps "models\htdemucs_6s_fp16weights.onnx"
$ExpectedSha = "7ce55792e2231c93fbf92de95f5fd5b3a5e6c89f7db690dfd693e8f1dce56869"
if (-not (Test-Path $Model)) {
    Download-File "https://huggingface.co/StemSplitio/htdemucs-6s-onnx/resolve/main/htdemucs_6s_fp16weights.onnx?download=true" $Model
}
$ActualSha = (Get-FileHash -Algorithm SHA256 $Model).Hash.ToLowerInvariant()
if ($ActualSha -ne $ExpectedSha) {
    Remove-Item -Force $Model -ErrorAction SilentlyContinue
    throw "Model SHA256 mismatch. Expected $ExpectedSha, got $ActualSha. Re-run bootstrap."
}

Write-Host "Dependencies are ready." -ForegroundColor Green
