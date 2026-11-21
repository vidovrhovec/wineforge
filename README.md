# WineForge — prenosni Wine motor

En sam skupek map, ki na kateremkoli 64-bit Linuxu požene Windows programe —
vključno z GPU pospeševanjem (NVIDIA CUDA), DXVK/VKD3D, WebView2, VC++/.NET in
32-bit programi. Brez namestitve, brez odvisnosti od sistemskega Wine.

```
WineForge/
├── bin/wineforge      ukazna vrstica (Python 3, brez zunanjih knjižnic)
├── engine/            Wine 11.22 + nvidia-libs (CUDA/NVML/NVAPI) + winetricks
├── share/             "pečena" predloga prefixa + offline predpomnilnik komponent
├── prefixes/          po en Wine prefix na aplikacijo
├── apps/              aplikacije (prenosne, z lastnim registrom)
├── data/              logi, stanje
└── dist/              izdelani prenosni paketi
```

## Hitri začetek

```bash
./bin/wineforge gui                    # grafični vmesnik (zenity ALI yad, brez terminala)
./bin/wineforge desktop                # ikona v meniju in na namizju — potem samo klik
./bin/wineforge tray                   # ikona v sistemskem pladnju (yad) z živim seznamom
```

**Prvi zgon na novem računalniku je samodejen** — `setup` (teče sam, ko
kličeš katerikoli ukaz ali GUI) prenese manjkajoče: Wine, nvidia-libs,
winetricks, prenosni `cabextract` (v `engine/tools/bin`, ne rabi roota) in
zgradi predlogo prefixa. Najprej poskuša **en prenos**, paket
`engine-<ver>.tar.zst` iz GitHub Releases (engine + predloga skupaj); če
paketa še ni (nov projekt), sestavi motor iz delov. Preveri ročno:

```bash
./bin/wineforge setup --check          # samo preveri, nič ne prenaša
./bin/wineforge setup --with-dotnet    # ročna priprava + .NET 4.8 in .NET 8
./bin/wineforge setup --offline        # samo iz cache-a (share/deps), brez interneta
./bin/wineforge setup --parts          # brez velikega paketa (posamični prenosi)
./bin/wineforge setup --manifest       # zapiši sha256 manifest vsega motorja
./bin/wineforge setup --verify         # preveri, ali je motor poškodovan
```

ali iz terminala:

```bash
./bin/wineforge doctor                 # diagnoza: Wine, GPU, Vulkan, CUDA
./bin/wineforge doctor --fix           # in popravi, kar se da
./bin/wineforge add mojapp --exe "/pot/do/App.exe" --include "/pot/do/App:Apps/App"
./bin/wineforge run mojapp             # zagon

./bin/wineforge install setup.exe      # namestitveni program -> prenosna aplikacija
./bin/wineforge pack mojapp --out dist # samostojen paket (run.sh + register.sh + ikona)
```

* **Samodejne ikone:** `add` in `install` privzeto ustvarita ikono tudi na
  namizju/meniju (klik za zagon) — izklopiš z `--no-shortcut`.

Klik na ikono (`wineforge desktop`) odpre GUI — če je bil računalnik
prestavljen/prazen, se motor najprej sam pripravi (prenosi), potem se
odpre okno. Nič ročnega pisanja v terminal.

## Ukazi

| Ukaz | Kaj naredi |
|---|---|
| `gui` | grafični vmesnik (zenity/yad): dodaj / namesti / zaženi / popravi / pakiraj |
| `desktop [app]` | ikona v meniju + na namizju (klik za zagon; brez appa = GUI) |
| `tray` | ikona v sistemskem pladnju (yad): seznam, zagon z enim klikom |
| `setup` | samodejna priprava motorja; `--check/--offline/--parts/--verify/--manifest` |
| `update` | `git pull` + po potrebi pripravi motor (`--no-pull` samo motor) |
| `shortcut` | ustvari bližnjico v meniju aplikacij |
| `doctor` | preveri Wine, `wineboot`, GPU, Vulkan, CUDA; `--fix` popravi kar se da |
| `info` | velikosti, različice, poti |
| `list` | seznam dodanih aplikacij |
| `add` | doda program, ki je že razširjen v mapi (prenosna aplikacija; `--no-shortcut`) |
| `run` | zažene aplikacijo v njenem prefixu |
| `repair` | popravi belo okno / pokvarjene podatke (WebView2, `*-Backup*`) |
| `scale` | ročno nastavi velikost vmesnika: `scale <app> 150` (100 = normalno) |
| `config` | globalne nastavitve, npr. privzeto samodejno skaliranje |
| `install` | zažene installer in iz njega naredi prenosno aplikacijo (`--no-shortcut`) |
| `deps` | iz PE uvozov ugotovi, katere komponente program potrebuje |
| `bake` | zgradi/posodobi predlogo prefixa; `--compact` varčuje ~150 MB |
| `prefix` | `list`, `new`, `delete`, `shell`, `explorer`, `tweaks` |
| `pack` | prenosni paket z `run.sh` in `register.sh`; `--desktop` takoj ustvari ikono |
| `remove` | odstrani aplikacijo (in po želji njen prefix) |

## Kaj dobiš privzeto

* **Wine 11.22** (wow64): 64-bit in 32-bit programi v istem prefixu.
* **DXVK 3.1.1** + **VKD3D-Proton**: D3D9/10/11/12 → Vulkan (nujno za WebView2/Chromium).
* **CUDA / NVML / NVAPI / NVENC / NvOptix** (`engine/nvidia-libs`): `nvcuda.dll`
  je winelib, ki se poveže na gostiteljski `libcuda.so`; v prefixu je symlink +
  `nvcuda=native` override. Deluje na NVIDIA gonilniku 580+ (testirano: RTX 3090,
  595.99.02, compute 8.6).
* **VC++ 2005–2022, VB6, .NET Framework 4.8, .NET Desktop 8, WebView2,
  DirectX helperji (d3dx9/d3dx11/d3dcompiler), MSXML, RichEdit, XACT, pisave** —
  v predlogi prefixa, zato je prvi zagon hiter.
* **winetricks + offline predpomnilnik** (`share/verbs`): dodatne komponente
  delujejo tudi brez interneta.
* Vsaka aplikacija ima **svoj prefix** (kot na Windows), zato si programi ne
  uničijo registra/DLL-jev.

## Prenosnost

Vse poti so relativne na koren motorja; symlinki znotraj prefixov kažejo na
`engine/` prek `../../..`. Paket iz `pack` vsebuje motor + aplikacijo + prefix +
`run.sh` in deluje iz katerekoli mape ali USB ključa.

**Premik na drug računalnik:** kopiraj/drugo mapo motorja (ali paket) in poženi
karkoli — `setup` sam ugotovi, kaj manjka, in to prenese/Zgradi:

| Manjka | Naredi sam |
|---|---|
| `engine/` + `share/templates/` | **poskusi en prenos**: `engine-0.1.0.tar.zst` iz Releases, sicer spodaj |
| `engine/wine` | prenese Wine 11.22 (sha256 preverjen) in razpakira |
| `engine/nvidia-libs` | prenese SveSop nvidia-libs 1.0.5 |
| `engine/tools/winetricks` | prenese winetricks |
| `cabextract` | prenese prenosno kopijo v `engine/tools/bin` (brez roota) |
| `share/templates/base-prefix.tar.zst` | `bake` predlogo (DXVK, VC++/.NET, WebView2 ...) |

Od sistemskega programa potrebuješ samo: **64-bitni Python 3, curl, tar,
zstd, zenity ali yad** (GUI; na KDE je navadno že `yad`).
`tray` (pladnj) rabi `yad`; `kdialog` deluje kot zadnja izbira za GUI okna.

**Integriteta:** `setup --manifest` zapiše sha256 vseh datotek motorja
(`data/engine-manifest.json`), `setup --verify` pa ugotovi poškodbe (USB,
nepopoln prenos, sistemski pomik) — `setup` nato obnovi samo poškodovane dele.
**Posodobitev:** `wineforge update` (`git pull` + po potrebi pripravi motor).
Zahteve: 64-bit Linux, glibc ≥ 2.31, Vulkan gonilnik. Za CUDA: NVIDIA gonilnik 580+.

## Znane pasti

* **WebView2 programi se v Wineu ne izrišejo** (Epic Games Launcher 2.x,
  Microsoft Teams/Outlook, marsikateri nov "Electron/WebView2" program):
  program se zažene (v dnevniku piše `Startup.Completed`), uporabnik pa vidi
  **črno okno z belim kvadratom** ali prazno belo okno. Vzrok ni WineForge,
  ampak Wine: Chromium pošilja sliko v okno po poti D3D11 shared-texture +
  DirectComposition, ki je Wine še ne podpira
  ([Wine bug 59370](https://bugs.winehq.org/show_bug.cgi?id=59370)).
  Preizkušeno in **ne pomaga**: `--disable-gpu`, `--disable-direct-composition`,
  `--in-process-gpu`, `--no-sandbox`, `--use-angle=swiftshader`, brez DXVK,
  svež profil WebView2, onemogočen `nvapi`/`nvml`, druga resolucija/DPI.
  Rešitev: uporabi Linux različico programa, če obstaja — za Epic Games
  Launcher uradni Linux AppImage/deb (<https://launcher.epicgames.com/>,
  od verzije 0.120 deluje tudi prijava in knjižnica) ali Heroic Games Launcher.
  WineForge tak program zdaj prepozna in ob `add`/`install` opozori.

* **Storitve, PowerShell in gonilniki:** Wine nima pravega Windows upravitelja
  storitev in njegov `powershell.exe` je samo "stub". Namestitveni programi,
  ki med namestitvijo kličejo PowerShell skripte ali speljejo lastne storitve
  (npr. StarDesk), zato pri namestitvi zabeležijo `err:` vrstice — program se
  **namesti, a pri zagonu pokaže napako** (manjka config/storitev).
  `install` zato po namestitvi izpiše povzetek grešn: če ga vidiš, je vzrok
  za napako aplikacije pogosto tam, ne v WineForge.
* **"Dodaj" ni "Namesti":** če v `add` izbereš namestitveni `.exe`, ga WineForge
  le registrira vsak zagon bo znova zagnal namestitev. Za installer-je uporabi
  `install` (v GUI-ju: "Namesti program").

* **Ne ubijaj aplikacije s `kill -9`** — Chromium/Electron aplikacije ob tem
  izgubijo/pokvarijo podatke in okno ostane belo. Zapri z X ali Ctrl+Q;
  za popravilo: `wineforge repair <app>`.
* Belo okno brez vrstice `Presenter:` v DXVK logu = Chromium ni ustvaril
  swapchaina → glej zgoraj (repair) oz. preveri, da je DXVK aktiven.
* Stari 32-bit InstallShield installerji včasih potrebujejo `winetricks` override.
* **Samodejno skaliranje vmesnika**: ob vsakem zagonu motor preveri, ali je
  program Electron/Chromium (resources.pak, icudtl.dat, libEGL.dll …) in mu
  samodejno doda `--force-device-scale-factor` (privzeto **1.5**), ker Chromium
  pod Wine ignorira sistemski DPI. Za Win32 programe uporabi `LogPixels`.
  Spreminjanje:
  ```bash
  ./bin/wineforge config --electron-scale 1.75   # večje ikone/pisave
  ./bin/wineforge config --win32-dpi-scale 1.25  # DPI za klasične programe
  ./bin/wineforge config --auto-scale off        # izklopi
  ./bin/wineforge scale <app> 125                # samo za en program
  ```

## Več

* `docs/ARCHITECTURE.md` — kako motor deluje (sloji, zaznavanje odvisnosti,
  pretvorba installerja v prenosno aplikacijo, stanje testiranja).
