@echo off
chcp 65001

cd "%~dp0Msp\MDK-ARM"
start F4HOST.uvprojx
exit