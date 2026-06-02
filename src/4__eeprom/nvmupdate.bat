@echo off

set PROJ_PATH=%1
set BUILD_OUT_NAME=%2
set MANUF_FILE_NAME=%3

set OBJCOPY="C:\Program Files (x86)\ON Semiconductor\Ezairo 8300 SDK\arm_tools\bin\arm-none-eabi-objcopy.exe"
set SREC_CAT="C:\Program Files\srecord\bin\srec_cat"
set JFLASH="C:\Program Files\SEGGER\JLink_V850\JFlash.exe"

set RELEASE_PATH=%PROJ_PATH%/Release
set NVM_FILE=%RELEASE_PATH%/%BUILD_OUT_NAME%.nvm
set HEX_FILE=%RELEASE_PATH%/%BUILD_OUT_NAME%.hex
set MANU_FILE=%PROJ_PATH%/%MANUF_FILE_NAME%
set LOG_JFLASH_FILE=%PROJ_PATH%/jflash.log
set LOG_JLINK_FILE=%PROJ_PATH%/jlink.log
set BACKUP_HEX=%PROJ_PATH%/manuf_backup.hex
set "JLINK_SN=603006635"

echo [INFO] PROJ_PATH : %PROJ_PATH%
echo [INFO] BUILD_OUT_NAME : %BUILD_OUT_NAME%
echo [INFO] OBJCOPY : %OBJCOPY%
echo [INFO] SREC_CAT : %SREC_CAT%
echo [INFO] JFALSH : %JFLASH%
echo [INFO] SN : %JLINK_SN%

if "%~3" == "" (
    set HAS_MANUF_INPUT_OPTION=0
) else (
    set HAS_MANUF_INPUT_OPTION=1
)

if %HAS_MANUF_INPUT_OPTION% == 1 (
    if exist %MANU_FILE% (
        set USE_MANUF_FILE=1
        echo [INFO] MANUF_FILE_NAME : %MANUF_FILE_NAME%
    ) else (
        set USE_MANUF_FILE=0
        echo [INFO] MANUF_FILE_NAME : USED, BUT THERE IS NO FILE
    )
) else (
    set USE_MANUF_FILE=0
    echo [INFO] MANUF_FILE_NAME : NOT USED
)

type nul > %LOG_JFLASH_FILE%
type nul > %LOG_JLINK_FILE%

echo [INFO] MAKE .hex FROM .nvm FILE
%OBJCOPY% -I binary --change-addresses 0x40008000 -O ihex %NVM_FILE% %HEX_FILE% -v

if %USE_MANUF_FILE% == 1 (
    echo [INFO] MERGE MANUF DATA FILE
    %SREC_CAT% %HEX_FILE% -Intel -exclude 0x40008400 0x40008500 -o %HEX_FILE% -Intel
    %SREC_CAT% %HEX_FILE% -Intel %MANU_FILE% -Intel -o %HEX_FILE% -Intel
) else (
    echo [INFO] READ MANUF AREA FROM CONNECTED DEVICE
    %JFLASH% -usb%JLINK_SN% ^
             -openprj %PROJ_PATH%/ezairo8300_LOWER_4MB.jflash ^
             -readrange 0x40008400,0x400084FF ^
             -saveas %BACKUP_HEX% ^
             -exit ^
             -jflashlog %LOG_JFLASH_FILE% ^
             -jlinklog %LOG_JLINK_FILE% 

    if errorlevel 1 (
        echo [ERROR] JFLASH, FAILED TO READ. PLEASE CHECK %LOG_JFLASH_FILE% FILE
        exit /b 1
    )

    if not exist %BACKUP_HEX% (
        echo [ERROR] FAILED TO CREATED %BACKUP_HEX% FILE
        exit /b 1
    )
    
    rem REMAKE .hex WITH BACKUP FILE
    %SREC_CAT% %HEX_FILE% -Intel -exclude 0x40008400 0x40008500 -o %HEX_FILE% -Intel
    %SREC_CAT% %HEX_FILE% -Intel %BACKUP_HEX% -Intel -o %HEX_FILE% -Intel
)

echo [INFO] DOWNLOAD .hex TO DEVICE
%JFLASH% -usb%JLINK_SN% ^
         -openprj %PROJ_PATH%/ezairo8300_LOWER_4MB.jflash ^
		 -open %HEX_FILE% ^
		 -auto ^
		 -exit ^
		 -jflashlog %LOG_JFLASH_FILE% ^
		 -jlinklog %LOG_JLINK_FILE%
		 