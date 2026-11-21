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
```

**Prvi zgon na novem računalniku je samodejen** — `setup` (teče sam, ko
kličeš katerikoli ukaz ali GUI) prenese manjkajoče: Wine, nvidia-libs,
winetricks, prenosni `cabextract` (v `engine/tools/bin`, ne rabi roota) in
zgradi predlogo prefixa. Preveri lahko ročno:

```bash
./bin/wineforge setup --check          # samo preveri, nič ne prenaša
./bin/wineforge setup --with-dotnet    # ročna priprava + .NET 4.8 in .NET 8
```

ali iz terminala:

```bash
./bin/wineforge doctor                 # diagnoza: Wine, GPU, Vulkan, CUDA
./bin/wineforge add mojapp --exe "/pot/do/App.exe" --include "/pot/do/App:Apps/App"
./bin/wineforge run mojapp             # zagon

./bin/wineforge install setup.exe      # namestitveni program -> prenosna aplikacija
./bin/wineforge pack mojapp --out dist # samostojen prenosni paket (run.sh + register.sh)
```

Klik na ikono (`wineforge desktop`) odpre GUI — če je bil računalnik
prestavljen/prazen, se motor najprej sam pripravi (prenosi), potem se
odpre okno. Nič ročnega pisanja v terminal.

## Ukazi

| Ukaz | Kaj naredi |
|---|---|
| `gui` | grafični vmesnik (zenity/yad): dodaj / namesti / zaženi / popravi / pakiraj |
| `desktop [app]` | ikona v meniju + na namizju (klik za zagon; brez appa = GUI) |
| `setup` | samodejna priprava motorja: prenosi manjkajoče, gradi predlogo |
| `shortcut` | ustvari bližnjico v meniju aplikacij |
| `doctor` | preveri Wine, `wineboot`, GPU (`nvidia-smi`), Vulkan in CUDA v prefixu |
| `info` | velikosti, različice, poti |
| `list` | seznam dodanih aplikacij |
| `add` | doda program, ki je že razširjen v mapi (prenosna aplikacija) |
| `run` | zažene aplikacijo v njenem prefixu |
| `repair` | popravi belo okno / pokvarjene podatke (WebView2, `*-Backup*`) |
| `scale` | ročno nastavi velikost vmesnika: `scale <app> 150` (100 = normalno) |
| `config` | globalne nastavitve, npr. privzeto samodejno skaliranje |
| `install` | zažene installer in iz njega naredi prenosno aplikacijo |
| `deps` | iz PE uvozov ugotovi, katere komponente program potrebuje |
| `bake` | zgradi/posodobi predlogo prefixa (vse komponente, tudi offline) |
| `prefix` | `list`, `new`, `delete`, `shell`, `explorer`, `tweaks` |
| `pack` | sestavi prenosni paket z `run.sh` in `register.sh` (ikona) |
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
| `engine/wine` | prenese Wine 11.22 (sha256 preverjen) in razpakira |
| `engine/nvidia-libs` | prenese SveSop nvidia-libs 1.0.5 |
| `engine/tools/winetricks` | prenese winetricks |
| `cabextract` | prenese prenosno kopijo v `engine/tools/bin` (brez roota) |
| `share/templates/base-prefix.tar.zst` | `bake` predlogo (DXVK, VC++/.NET, WebView2 ...) |

Od sistemskega programa potrebuješ samo: **64-bitni Python 3, curl, tar,
zstd, zenity ali yad** (GUI; na KDE je navadno že `yad`).
Zahteve: 64-bit Linux, glibc ≥ 2.31, Vulkan gonilnik. Za CUDA: NVIDIA gonilnik 580+.

## Znane pasti

* **Ne ubijaj aplikacije s `kill -9`** — WebView2/Chromium aplikacije ob tem
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
