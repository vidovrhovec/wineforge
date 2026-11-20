#!/usr/bin/env bash
# Pripravi engine/ za WineForge: Wine (prenosljiv), nvidia-libs (CUDA) in winetricks.
#
# Uporaba:  ./scripts/setup-engine.sh [pot-do-wine-runnerja]
#
# Motor ni v gitu (832 MB binarnih datotek). Ta skripta ga sestavi iz:
#   1. WineCharm flatpaka (če je nameščen)  ->  hardlink kopija runnerja
#   2. ali prenosa Kron4ek Wine-Builds      ->  razpakiranje v engine/wine
#   3. SveSop/nvidia-libs (CUDA/NVML/NVAPI) ->  engine/nvidia-libs
#   4. winetricks (skripta)                 ->  engine/tools/winetricks
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ENGINE="$ROOT/engine"
RUNNER_ARG="${1:-}"

say() { printf '\033[1;36m==>\033[0m %s\n' "$*"; }
warn() { printf '\033[1;33m[!]\033[0m %s\n' "$*"; }
die() { printf '\033[1;31m[x]\033[0m %s\n' "$*" >&2; exit 1; }

mkdir -p "$ENGINE"/{wine,tools,nvidia-libs,lib}

# ---------------------------------------------------------------- 1) Wine ----
WCHAR="$HOME/.var/app/io.github.fastrizwaan.WineCharm/data/winecharm/Runners"
if [ ! -x "$ENGINE/wine/bin/wine" ]; then
  if [ -n "$RUNNER_ARG" ]; then
    RUNNER="$RUNNER_ARG"
  elif [ -d "$WCHAR" ]; then
    # najnovejši 11.x amd64 (wow64) runner
    RUNNER="$(ls -d "$WCHAR"/wine-11.2*-amd64-wow64 "$WCHAR"/wine-11.2*-amd64 2>/dev/null | tail -1 || true)"
  fi

  if [ -n "${RUNNER:-}" ] && [ -x "$RUNNER/bin/wine" ]; then
    say "Kopiram Wine runner: $RUNNER"
    rm -rf "$ENGINE/wine"
    cp -al "$RUNNER" "$ENGINE/wine" 2>/dev/null || cp -a "$RUNNER" "$ENGINE/wine"
  else
    warn "Wine runnerja ni bilo mogoče najti lokalno."
    cat <<'EOF'
Prenesi prenosljiv Wine (Kron4ek Wine-Builds), npr.:
  https://github.com/Kron4ek/Wine-Builds/releases
in razpakiraj tako, da bo obstajati:
  engine/wine/bin/wine

Zahtevano: glibc >= 2.31, 64-bit Linux, Vulkan gonilnik.
EOF
    die "engine/wine manjka"
  fi
fi
say "Wine: $("$ENGINE/wine/bin/wine" --version 2>/dev/null || echo '?')"

# --------------------------------------------------------- 2) nvidia-libs ----
if [ ! -f "$ENGINE/nvidia-libs/x64/nvcuda.dll" ]; then
  SRC="$HOME/.var/app/io.github.fastrizwaan.WineCharm/data/winecharm/nvidia-libs"
  if [ -f "$SRC/x64/nvcuda.dll" ]; then
    say "Kopiram nvidia-libs iz WineCharm"
    cp -a "$SRC/." "$ENGINE/nvidia-libs/"
  else
    URL="https://github.com/SveSop/nvidia-libs/releases/download/v1.0.5/nvidia-libs-v1.0.5.tar.xz"
    say "Prenašam nvidia-libs: $URL"
    tmp="$(mktemp -d)"
    curl -fL --retry 3 -o "$tmp/nvl.tar.xz" "$URL"
    tar -xJf "$tmp/nvl.tar.xz" -C "$tmp"
    cp -a "$tmp"/nvidia-libs-*/. "$ENGINE/nvidia-libs/"
    rm -rf "$tmp"
  fi
fi
[ -f "$ENGINE/nvidia-libs/x64/nvcuda.dll" ] \
  && say "nvidia-libs: OK (CUDA/NVML/NVAPI)" \
  || warn "nvidia-libs manjka — CUDA ne bo deloval (vse ostalo dela)"

# ----------------------------------------------------------- 3) winetricks ----
if [ ! -x "$ENGINE/tools/winetricks" ]; then
  say "Prenašam winetricks"
  curl -fL --retry 3 -o "$ENGINE/tools/winetricks" \
    https://raw.githubusercontent.com/Winetricks/winetricks/master/src/winetricks
  chmod +x "$ENGINE/tools/winetricks"
fi
say "winetricks: $("$ENGINE/tools/winetricks" --version 2>/dev/null | head -1)"

# ------------------------------------------------------------- 4) mape ------
mkdir -p "$ROOT"/{prefixes,apps,data/logs,share/{templates,deps,verbs},dist}

say "Končano. Preveri z:  ./bin/wineforge doctor"
