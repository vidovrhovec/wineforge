#!/bin/bash
# Zgradi nadomestni SSPI modul (nativna DLL) s prenosnim llvm-mingw.
set -e
cd /home/vidov/Namizje/Ai/wineforge
CC=engine/tools/llvm-mingw/bin/x86_64-w64-mingw32-clang
CC32=engine/tools/llvm-mingw/bin/i686-w64-mingw32-clang
mkdir -p src/sspi/build

# iz secur32.def naredi C datoteko z izvoznimi obvestili (/.drectve/)
PYTHONPATH=/tmp/pk python3 - <<'PY'
import io, re, pefile

def izvozi(pot):
    pe = pefile.PE(pot)
    e = getattr(pe, "DIRECTORY_ENTRY_EXPORT", None)
    return {s.name.decode() for s in e.symbols if s.name} if e else set()

# 1) lastna izvozna imena (iz vira)
izvir = io.open("src/sspi/secur32_shim.c", encoding="utf-8").read()
lastna = set(re.findall(r"__declspec\(dllexport\)\s+\w+[\w\s\*]*?SEC_ENTRY\s+(\w+)\s*\(", izvir))

# 2) vse, kar zna Wineov secur32 in ga mi nimamo -> posredujemo naprej
wine = izvozi("engine/wine/lib/wine/x86_64-windows/secur32.dll")
manjka = sorted(wine - lastna)
vrstice = [f'#pragma comment(linker, "/EXPORT:{n}")' for n in sorted(lastna)]
io.open("src/sspi/build/izvozi.c", "w").write("#include <windows.h>\n" + "\n".join(vrstice) + "\n")

# manjkajoča izvozna imena ustvarimo kot posredovalne vuece (x64: enotna
# klicna konvencija, zato grezimo do osem parametrov naprej)
vse = ["#include <windows.h>", "#include <stdint.h>", "",
       "static void *wine_secur32(void) {",
       "    static void *m = 0;", "    if (!m) m = (void *)LoadLibraryA(\"wine-secur32.dll\");",
       "    return m;", "}", "",
       "typedef int64_t (__stdcall *sedem_fn)(void *, void *, void *, void *, void *, void *,",
       "                                          void *, void *);", "",
       "static int64_t posreduj(const char *ime, void *a, void *b, void *c, void *d, void *e,",
       "                        void *f, void *g, void *h) {",
       "    void *m = wine_secur32();",
       "    sedem_fn g2 = m ? (sedem_fn)GetProcAddress((HMODULE)m, ime) : 0;",
       "    if (!g2) return (int64_t)0x80090022;   /* SEC_E_UNSUPPORTED_FUNCTION */",
       "    return g2(a, b, c, d, e, f, g, h);", "}"]
for n in manjka:
    vse.append("extern __declspec(dllexport) int64_t %s(void *a, void *b, void *c, void *d,"
               % n)
    vse.append("    void *e, void *f, void *g, void *h);")
    vse.append('int64_t %s(void *a, void *b, void *c, void *d, void *e, void *f, void *g, void *h)' % n)
    vse.append('{ return posreduj("%s", a, b, c, d, e, f, g, h); }' % n)
io.open("src/sspi/build/posreduj.c", "w").write("\n".join(vse) + "\n")
print(f"izvoznih imen: {len(lastna)} nasi + {len(manjka)} posredovanih")
print("  posredovani primeri:", ", ".join(manjka[:6]))
PY

for para in "x64:$CC" "x86:$CC32"; do
  ime=${para%%:*}; prev=${para#*:}
  echo "=== $ime"
  $prev -shared -O2 -Wall -Wno-unused-parameter -o src/sspi/build/secur32-$ime.dll \
      src/sspi/secur32_shim.c src/sspi/build/izvozi.c src/sspi/build/posreduj.c \
      -lkernel32 -ladvapi32 -lole32 2>&1 | grep -a "error" | head -8 || true
done
ls -l src/sspi/build/*.dll 2>/dev/null | awk '{print $5, $9}'

echo "=== preizkus pogajanj"
$CC -O2 -o src/sspi/build/wfsspi-test.exe src/sspi/test.c -lsecur32 -lole32 \
    2>&1 | grep -a "error" | head -6 || true

mkdir -p share/sspi
cp -f src/sspi/build/secur32-x64.dll src/sspi/build/secur32-x86.dll \
      src/sspi/build/wfsspi-test.exe share/sspi/ 2>/dev/null
echo "nameščeno v share/sspi:"
ls -l share/sspi | awk 'NR>1 {print "  " $5, $9}'
