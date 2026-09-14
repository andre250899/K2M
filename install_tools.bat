@echo off
title Instalador C++ Build Tools para K2M
echo ======================================================================
echo    K2M - Instalacao de Ferramentas de Compilacao (MSVC C++20)
echo ======================================================================
echo.
echo Este script executara o instalador do Visual Studio Build Tools 2022
echo configurado para C++ Desktop (MSVC v143 + Windows SDK).
echo.
echo Requer privilégios de Administrador.
echo.

set INSTALLER_PATH="C:\Users\VOXX-PC\AppData\Local\Temp\WinGet\Microsoft.VisualStudio.2022.BuildTools.17.14.40\vs_BuildTools.exe"

if not exist %INSTALLER_PATH% (
    echo Instalador em cache nao encontrado. Baixando via winget...
    winget download --id Microsoft.VisualStudio.2022.BuildTools -d "%TEMP%"
)

echo Iniciando instalacao... (aguarde o encerramento da janela do instalador)
%INSTALLER_PATH% --passive --wait --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended

echo.
echo ======================================================================
echo    Instalacao concluida com sucesso!
echo ======================================================================
pause
