$DIR = "s:\vegyes\falconBoard\FalconBoard"
$PRO = "FalconBoard.pro"
$HU_TS = "..\translations\FalconBoard_hu.ts"

# Set working directory
Set-Location -Path $DIR

# Set console encoding to UTF-8
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
[Console]::InputEncoding = [System.Text.Encoding]::UTF8

# Output informational messages
echo "Csak egyszer futtasd ezt egy CMD ablakban. Ezután jöhetnek a következők:"
echo "Ezeket többször is használhatod:"
echo "d:\Qt\5.15.2\msvc2019_64\bin\lupdate $PRO"
echo "d:\Qt\5.15.2\msvc2019_64\bin\linguist $HU_TS"
echo "d:\Qt\5.15.2\msvc2019_64\bin\lrelease-pro -removeidentical $PRO"
echo 'A project fájlba írd be: "TRANSLATIONS=<1.ts fájl>[ <2. ts fájl>[ ...]]"'

# Path to the vcvars64.bat file
$vcvarsPath = "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

# Start cmd, run vcvars64.bat, and keep the environment variables in PowerShell
cmd /c "`"$vcvarsPath`" & set" | ForEach-Object {
    if ($_ -match '^(.*?)=(.*)$') {
        [System.Environment]::SetEnvironmentVariable($matches[1], $matches[2], [System.EnvironmentVariableTarget]::Process)
    }
}
