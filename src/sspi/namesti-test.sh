#!/bin/bash
# Namesti nadomestni SSPI v prefix in zazeni preizkus.
cd /home/vidov/Namizje/Ai/wineforge || exit 1
P=prefixes/stardesk
S=$P/drive_c/windows/system32

# 1) pravi (Wineov) modul pod drugim imenom, da lahko napotimo naprej
cp -f engine/wine/lib/wine/x86_64-windows/secur32.dll "$S/wine-secur32.dll"
cp -f engine/wine/lib/wine/x86_64-windows/secur32.dll "$P/drive_c/secur32-real.dll" 2>/dev/null || true

# 2) nativna nadomestka (64 + 32 bit)
[ -f "$S/secur32.original" ] || cp -f "$S/secur32.dll" "$S/secur32.original" 2>/dev/null || true
cp -f src/sspi/build/secur32-x64.dll "$S/secur32.dll"
cp -f src/sspi/build/secur32-x86.dll "$S/secur32-32.dll" 2>/dev/null || true
echo "nameščeno: $(ls -l "$S/secur32.dll" | awk '{print $5}') B"

# 3) preizkusni program
engine/tools/llvm-mingw/bin/x86_64-w64-mingw32-clang -O2 -o src/sspi/build/wfsspi-test.exe \
    src/sspi/test.c -lsecur32 2>&1 | grep -a error | head -6
mkdir -p "$P/drive_c/temp"
cp -f src/sspi/build/wfsspi-test.exe "$P/drive_c/temp/"
echo "=== zagon pod Wineom (nadomestni secur32):"
export WINEPREFIX=$PWD/$P WINEDEBUG=-all DISPLAY=:0 WINEDLLOVERRIDES="secur32=n"
timeout 150 engine/wine/bin/wine 'C:\temp\wfsspi-test.exe' 2>&1 | grep -av "^MESA\|^pci id\|libGL" | head -30
