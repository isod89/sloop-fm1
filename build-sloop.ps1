# SLOOP 2.6: build the firmware, then open the browser installer.
# Uses the dependencies already prepared (WSL MinUI-Build, JieLi toolchain, SDK files in build/deps/ac79).
# SLICE stays out (FELUCCA_SLICE=0): with its built-in BREAK the image does not fit the app slot.
$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot
Write-Host ""
Write-Host "  S L O O P   2.6" -ForegroundColor White
Write-Host "  ---- ---- ---- ----" -ForegroundColor DarkGray
Write-Host "== Building the firmware (WSL MinUI-Build)" -ForegroundColor Cyan
Remove-Item Env:FELUCCA_SLICE -ErrorAction SilentlyContinue
python tools/build_windows.py --distro MinUI-Build --toolchain /root/fm1-codex-deps/jieli/toolchain --sdk build/deps/ac79 --slice 0
if ($LASTEXITCODE -ne 0) { throw "The build failed" }
Write-Host "== Making the installer site" -ForegroundColor Cyan
python web/make_site.py build/felucca.fwsc 2.6 build/sloop-site
if ($LASTEXITCODE -ne 0) { throw "make_site failed" }
Write-Host ""
Write-Host "Installer: http://localhost:8766/webapp/installer/  (Chrome or Edge, FM-1 on USB)" -ForegroundColor Green
Write-Host "Editor:    http://localhost:8766/webapp/editor/" -ForegroundColor Green
Write-Host "Keep this window open during the install. Ctrl+C stops the server."
Start-Process "http://localhost:8766/webapp/installer/"
python -m http.server 8766 --bind 127.0.0.1 --directory build/sloop-site
