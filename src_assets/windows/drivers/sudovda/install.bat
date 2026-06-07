@echo off
pushd %~dp0

rem --- Downgrade guard (code review) ------------------------------------------
rem This (re)installs the BUNDLED SudoVDA driver (see DriverVer in SudoVDA.inf).
rem On a machine that already has a NEWER SudoVDA driver, re-running this will
rem DOWNGRADE it and can break the virtual display
rem (STATUS_FAILED_DRIVER_ENTRY / 0xC0000365). Re-run with  install.bat /force
rem only if you intentionally want to (re)install the bundled version.
if /I not "%~1"=="/force" (
  echo *** WARNING: this installs the BUNDLED SudoVDA driver and may DOWNGRADE an
  echo *** already-installed newer version, which can break the virtual display.
  echo *** If you really intend to, re-run:   install.bat /force
  popd
  exit /b 1
)

set "CERTUTIL=certutil"
where certutil >nul 2>&1 || set "CERTUTIL=%SystemRoot%\System32\certutil.exe"

echo ================
echo Installing cert for the SudoVDA driver...

%CERTUTIL% -addstore -f root "sudovda.cer"
%CERTUTIL% -addstore -f TrustedPublisher "sudovda.cer"

echo ================
echo Removing the old driver... It's OK to show an error if you're installing the driver for the first time.

nefconc.exe --remove-device-node --hardware-id root\sudomaker\sudovda --class-guid "4D36E968-E325-11CE-BFC1-08002BE10318"

echo ================
echo Installing the new driver...

nefconc.exe --create-device-node --class-name Display --class-guid "4D36E968-E325-11CE-BFC1-08002BE10318" --hardware-id root\sudomaker\sudovda
nefconc.exe --install-driver --inf-path "SudoVDA.inf"

echo ================
echo Done!

popd
