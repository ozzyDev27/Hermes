@echo off
g++ main.cpp -o Hermes
if %errorlevel% equ 0 (
    echo yippee
    .\Hermes
)
