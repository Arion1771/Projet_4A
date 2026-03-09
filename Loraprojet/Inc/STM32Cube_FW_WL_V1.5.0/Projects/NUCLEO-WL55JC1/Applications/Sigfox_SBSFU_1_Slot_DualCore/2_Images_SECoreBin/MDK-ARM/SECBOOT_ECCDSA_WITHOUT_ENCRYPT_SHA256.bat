::Post build for SECBOOT_ECCDSA_WITHOUT_ENCRYPT_SHA256
:: arg1 is the build directory
:: arg2 is the elf file path+name
:: arg3 is the bin file path+name
:: arg4 is the firmware Id (1/2/3)
:: arg5 is the version
@echo off
set "projectdir=%1"
set "execname=%~n3"
set "elf=%2"
set "bin=%3"
set "fwid=%4"
set "version=%5"

set "SBSFUBootLoader=%~d0%~p0\\..\\.."
set "userAppBinary=%projectdir%\\Binary"

set "sfb=%userAppBinary%\\%execname%.sfb"
set "sign=%userAppBinary%\\%execname%.sign"
set "headerbin=%userAppBinary%\\%execname%sfuh.bin"
set "bigbinary=%userAppBinary%\\SBSFU_%execname%.bin"

set "oemkey="
set "ecckey=%SBSFUBootLoader%\\2_Images_SECoreBin\\Binary\\ECCKEY%fwid%.txt"
IF "%fwid%"=="2" (
set "priorbin=%userAppBinary%\\SBSFU_Sigfox_PushButton_DualCore_CM0PLUS.bin"
set "sbsfuelf=%SBSFUBootLoader%\\2_Images_SBSFU\\MDK-ARM\\STM32WL55JC_Nucleo_CM4\\Exe\\SB.axf"
goto setkeys
)
set "sbsfuelf=%SBSFUBootLoader%\\2_Images_SBSFU\\MDK-ARM\\STM32WL55JC_Nucleo_CM0PLUS\\Exe\\SBSFU.axf"
:setkeys
set "mapping=%SBSFUBootLoader%\\Linker_Common\\MDK-ARM\\mapping_fwimg.h"
set "magic=SFU%fwid%"
set "offset=512"

::comment this line to force python
::python is used if windows executable not found
pushd %projectdir%\..\..\..\..\..\..\Middlewares\ST\STM32_Secure_Engine\Utilities\KeysAndImages
set basedir=%cd%
popd
goto exe:
goto py:
:exe
::line for window executable
echo Postbuild with windows executable
set "prepareimage=%basedir%\\win\\prepareimage\\prepareimage.exe"
set "python="
if exist %prepareimage% (
goto postbuild
)
:py
::line for python
echo Postbuild with python script
set "prepareimage=%basedir%\\prepareimage.py"
set "python=python "
:postbuild
echo "%python%%prepareimage%" > %projectdir%\\output_%fwid%.txt

::Make sure we have a Binary sub-folder in UserApp folder
if not exist "%userAppBinary%" (
mkdir "%userAppBinary%" >> %projectdir%\\output_%fwid%.txt 2>&1
IF %ERRORLEVEL% NEQ 0 goto :error
)

:: no encryption
set "command=%python%%prepareimage% sha256 %bin% %sign% >> %projectdir%\output_%fwid%.txt 2>&1"
%command%
IF %ERRORLEVEL% NEQ 0 goto :error

:: no encryption so pack the binary file
set "command=%python%%prepareimage% pack -m %magic% -k %ecckey% -p 1 -r 44 -v %version% -f %bin% -t %sign% %sfb% -o %offset% >> %projectdir%\output_%fwid%.txt 2>&1"
%command%
IF %ERRORLEVEL% NEQ 0 goto :error
set "command=%python%%prepareimage% header -m %magic% -k %ecckey% -p 1 -r 44 -v %version% -f %bin% -t %sign%  -o %offset% %headerbin% >> %projectdir%\output_%fwid%.txt 2>&1"
%command%
IF %ERRORLEVEL% NEQ 0 goto :error

::get Header address when not contiguous with firmware
if exist %projectdir%\header_%fwid%.txt (
  del %projectdir%\header_%fwid%.txt
)
set "command=%python%%prepareimage% extract -d "SLOT_ACTIVE_%fwid%_HEADER" %mapping% > %projectdir%\header_%fwid%.txt"
%command%
IF %ERRORLEVEL% NEQ 0 goto :error
set /P header=<%projectdir%\header_%fwid%.txt >> %projectdir%\output_%fwid%.txt 2>>&1
echo header %header% >> %projectdir%\output_%fwid%.txt 2>>&1

if exist %priorbin%.baseadd (goto addpriorbin)
set extramergebin=
goto buildmergecmd
:addpriorbin
set /P priorbinbaseadd=<%priorbin%.baseadd
set extramergebin=%priorbin%@%priorbinbaseadd%;
:buildmergecmd
set "command=%python%%prepareimage% mergev2 -b %extramergebin%%headerbin%@%header% -f %sbsfuelf%;%elf% %bigbinary% >> %projectdir%\output_%fwid%.txt 2>&1"
%command%
IF %ERRORLEVEL% NEQ 0 goto :error

::clean up the intermediate file
del %sign%
del %headerbin%

exit 0

:error
echo "%command% : failed" >> %projectdir%\\output_%fwid%.txt
:: remove the elf to force the regeneration
if exist %elf%(
  del %elf%
)
echo %command% : failed

pause
exit 1

:nothingtodo
exit 0
