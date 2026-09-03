$ErrorActionPreference = 'Stop'
$exe = 'C:\Users\SHUVL\ForgetTheBand\build\Session_artefacts\Release\ForgetTheBand.exe'
$dir = Split-Path $exe
$root = 'C:\Users\SHUVL\ForgetTheBand'
$desk = 'C:\Users\SHUVL\Desktop'

Get-Process -Name 'ForgetTheBand' -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 400

$st = Get-ChildItem -Path 'C:\Program Files (x86)\Windows Kits\10\bin' -Recurse -Filter signtool.exe -ErrorAction SilentlyContinue |
    Where-Object { $_.FullName -match '\\x64\\signtool.exe$' } |
    Sort-Object FullName -Descending |
    Select-Object -First 1
if (-not $st) { throw 'signtool not found' }
Write-Host ("signtool " + $st.FullName)
& $st.FullName sign /n 'Centrophy' /fd SHA256 /td SHA256 /tr http://timestamp.digicert.com $exe
if ($LASTEXITCODE -ne 0) {
    Write-Host 'timestamp failed, signing without timestamp'
    & $st.FullName sign /n 'Centrophy' /fd SHA256 $exe
}
Write-Host ("sign_exit " + $LASTEXITCODE)
& $st.FullName verify /pa $exe

Copy-Item -Force $exe (Join-Path $desk 'ForgetTheBand.exe')
foreach ($n in @('onnxruntime.dll','basic_pitch.onnx','NOTICE.txt')) {
    $src = Join-Path $dir $n
    if (Test-Path $src) {
        Copy-Item -Force $src (Join-Path $desk $n)
        Write-Host ("copied " + $n)
    } else {
        Write-Host ("MISSING " + $n)
    }
}

$samplesSrc = Join-Path $root 'Assets\Samples'
$deskSamples = Join-Path $desk 'Assets\Samples'
$exeSamples = Join-Path $dir 'Assets\Samples'
if (Test-Path $samplesSrc) {
    New-Item -ItemType Directory -Force -Path $deskSamples | Out-Null
    New-Item -ItemType Directory -Force -Path $exeSamples | Out-Null
    Copy-Item -Force -Recurse $samplesSrc\* $deskSamples
    Copy-Item -Force -Recurse $samplesSrc\* $exeSamples
    Write-Host ("copied Assets/Samples to Desktop and beside exe")
}

Get-ChildItem (Join-Path $desk 'ForgetTheBand.exe') | ForEach-Object {
    '{0} {1} bytes {2:yyyy-MM-dd HH:mm:ss}' -f $_.Name, $_.Length, $_.LastWriteTime
}
Get-AuthenticodeSignature (Join-Path $desk 'ForgetTheBand.exe') | Format-List Status, SignerCertificate
Write-Host ("desktop samples: " + (Test-Path (Join-Path $deskSamples 'drums\acoustic_kick.wav')))
Write-Host ("exe samples: " + (Test-Path (Join-Path $exeSamples 'drums\acoustic_kick.wav')))
