@echo off
echo ====================================
echo   KIEM TRA CAU HINH MAY TINH (SPEC)
echo ====================================
echo.
echo --- CPU ---
wmic cpu get name | findstr /v "Name"
echo.
echo --- RAM ---
wmic computersystem get TotalPhysicalMemory | findstr /v "TotalPhysicalMemory"
echo.
echo --- GPU ---
wmic path win32_VideoController get name | findstr /v "Name"
echo ====================================
