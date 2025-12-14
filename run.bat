@echo off
gcc main.c -o Hermes
if %errorlevel% equ 0 (
    .\Hermes
)
