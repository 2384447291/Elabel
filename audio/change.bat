@echo off
REM ------------------------------------------------------------
REM   文件名：wav2pcm.bat
REM   说明：遍历当前目录下所有 .wav，将其转换为 .pcm
REM ------------------------------------------------------------

REM 检查当前目录下是否有 .wav 文件
set found=0
for %%F in (*.wav) do (
    set found=1
    goto :hasWave
)
:hasWave
if "%found%"=="0" (
    echo 未找到任何 .wav 文件，程序退出。
    pause
    exit /b 1
)

REM 开始转换
for %%F in (*.wav) do (
    echo 正在将 "%%F" 转换为 "%%~nF.pcm" ...
    ffmpeg -i "%%F" -f s24le -ar 24000 -ac 1 -acodec pcm_s24le "%%~nF.pcm"
    if errorlevel 1 (
        echo [错误] 转换 "%%F" 时出现问题，请检查 FFmpeg 是否可用。
        pause
        exit /b 1
    )
)

echo 所有文件已成功转换完成。
pause
