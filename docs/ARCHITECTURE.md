# WineForge — arhitektura motorja

Motor je ena mapa. Vse poti so relativne na koren mape, zato ga lahko premakneš
na USB ključ, v drugo distribucijo, ali zapakiraš v AppImage.

```
WineForge/
├── bin/wineforge          # CLI (Python 3, brez zunanjih odvisnosti)
├── engine/
│   ├── wine/              # prenosljiv Wine 11.22 (wow64: 32- in 64-bit programi)
│   ├── nvidia-libs/       # nvcuda (CUDA), nvml, nvapi, nvcuvid, nvencodeapi, nvoptix
│   ├── lib/               # rezervne sistemske knjižnice (če jih gostitelj nima)
│   └── tools/winetricks   # winetricks (offline cache v share/verbs)
├── share/
│   ├── templates/base-prefix.tar.zst   # "pečena" predloga prefixa
│   ├── deps/              # predpreneseni namestitveni programi komponent
│   └── verbs/             # winetricks predpomnilnik (offline delovanje)
├── prefixes/<ime>/        # Wine prefixi (vsaka aplikacija svojega)
├── apps/<slug>/           # aplikacije: app.json + root/drive_c/... + *.reg
├── data/                  # logi, stanje peke
└── dist/                  # izdelani prenosni paketi
```

## Zakaj prefix na aplikacijo

Windows programi pišejo v `C:\Windows`, `C:\Program Files` in register. Če dva
programa delita prefix, si uničita konfiguracijo (različne verzije VC++/.NET,
različni DLL override-i, pokvarjen `system.reg`). Zato vsaka aplikacija dobi
svoj prefix, narejen iz skupne **predloge** — ta je že "pečena" z vsemi
komponentami, zato je prvi zagon hiter in predvidljiv.

## Kako se ustvari aplikacija iz namestitvenega programa

`wineforge install <setup.exe>` naredi točno tisto, kar delajo Windows
"portable app" orodja (ThinApp, Enigma, PortableApps), le da na Wine način:

1. iz predloge se naredi začasen prefix,
2. posname se **stanje pred** (seznam datotek v `drive_c` z velikostjo in mtime),
3. namestitveni program se zažene v tihem načinu (NSIS `/S`, Inno `/VERYSILENT`,
   MSI `/qn`, InstallShield `/s /v/qn`, WiX `/quiet`, Squirrel `--silent`),
4. posname se **stanje po** in izračuna razlika (nove/spremenjene datoteke),
5. izvozita se `HKCU` in `HKLM` register; poti začasnega prefixa se prepišejo v
   `C:\`, da je vse prenosno,
6. razlika se shrani v `apps/<slug>/root/drive_c/...`, register v
   `apps/<slug>/user.reg` + `machine.reg`, glavni `.exe` se zazna samodejno.

Ob prvem zagonu (`wineforge run <app>`) se prefix naredi iz predloge, vanj se
prekopirajo datoteke aplikacije in uvozi register. Rezultat: program se obnaša,
kot da je nameščen, a je v celoti prenosljiv.

## Sloji, ki jih motor nastavi samodejno

| Sloj | Kaj prinese | Kako |
|---|---|---|
| DXVK | D3D8/9/10/11 → Vulkan (brez tega WebView2/Chromium okno ostane prazno) | winetricks `dxvk`, override `*d3d11,*dxgi,*d3d9,*d3d8,*d3d10core=native` |
| VKD3D-Proton | D3D12 → Vulkan | winetricks `vkd3d`, override `*d3d12,*d3d12core` |
| CUDA / NVML / NVAPI | `nvcuda.dll` relay na Linux `libcuda.so` (NVIDIA gonilnik) | nvidia-libs: symlink v `system32` + override `nvcuda,nvml,nvapi64=native` |
| VC++ 2005–2022 | `msvcr*/msvcp*/vcruntime140*`, MFC | winetricks `vcrun2005..vcrun2022` |
| .NET Framework 4.8 | `mscoree.dll` aplikacije | winetricks `dotnet48` |
| .NET Desktop 8 | `hostfxr.dll`, `coreclr.dll` | winetricks `dotnetdesktop8` |
| WebView2 | Edge/Chromium komponenta za Tauri/Electron aplikacije | uradni `MicrosoftEdgeWebview2Setup.exe /silent` |
| DirectX 9 helperji | `d3dx9_*.dll`, `d3dx11_43`, `d3dcompiler_*` | winetricks `d3dx9`, `d3dx11_43`, `d3dcompiler_43/47` |
| Pisave | Arial/Times/Courier/Tahoma … | winetricks `corefonts`, `tahoma`, `fontsmooth=rgb` |
| MSXML, RichEdit, XACT | `msxml3/6`, `riched20`, `xinput`, `xaudio2` | winetricks |

## Analiza odvisnosti (kaj program potrebuje)

`wineforge deps <exe ali mapa>` prebere PE uvozne tabele (lasten parser, brez
zunanjih orodij) in iz uvoženih DLL-jev ugotovi komponente:

```
msvcp140.dll, vcruntime140.dll  -> vcrun2015-2022
mscoree.dll                     -> dotnet48
d3dx9_43.dll                    -> d3dx9
WebView2Loader.dll              -> webview2
nvcuda.dll                      -> CUDA (nvidia-libs)
```

To je osnova za samodejno zaznavanje dodatkov ob namestitvi.

## Prenosni paket

`wineforge pack <app>` zloži v `dist/<app>/`: celoten motor, predlogo, aplikacijo,
njen prefix in `run.sh`. Mape se kopirajo s trdimi povezavami (`cp -al`), tako da
na istem disku paket ne porabi dodatnega prostora; ko ga preneseš na USB, postane
samostojen. `--compress` doda `tar.zst`.

## WebView2 / Chromium aplikacije (Tauri, Electron) — pomembno

Aplikacije, ki uporabljajo WebView2 (Edge/Chromium), so pod Wine občutljive na
nenaden izklop. Če proces ubiješ s `kill -9` (ali se sesuje med pisanjem v
podatkovno mapo), se pri naslednjem zagonu pogosto zgodi:

* podatkovna mapa aplikacije se preimenuje v `<mapa>-BackupByAI_...` in ostane
  prazna, ali
* profil Chromiuma (`EBWebView`) ostane zaklenjen/pokvarjen,

rezultat je **belo/prazno okno**, v logu DXVK pa ni vrstice `Presenter:`
(swapchain se nikoli ne ustvari). Diagnostični znak: `grep -c Presenter <log>`
vrne 0, pri delujočem zagonu pa vsaj 1.

Zato:

* aplikacijo vedno zapri z gumbom X / Ctrl+Q (ne s `kill -9`),
* za popravilo uporabi `wineforge repair <app>` (ponastavi WebView2 profil) in
  `wineforge repair <app> --restore` (obnovi podatke iz `*-Backup*` mape).

Enako velja za `wineserver -k` — ta je "mehak" in je v redu, `kill -9` pa ne.

## Prenosnost symlinkov

`nvcuda.dll` in ostale NVIDIA knjižnice so v prefixu **relativni symlinki** v
`engine/nvidia-libs/`, ne absolutni. Zato paket deluje, tudi če ga premakneš
drugam. Enako velja za symlink programa (`C:\Apps\<App>` → `apps/<app>/inc/...`),
ki ga `pack` prepiše na pot znotraj paketa.

## Stanje testiranja

| Aplikacija | Tip | Stanje |
|---|---|---|
| Notepad++ 8.7.1 | NSIS installer | ✓ `install`: 124 datotek/17 MB, samodejno zaznan exe, zagon |
| WinMerge 2.16.44 | Inno Setup installer | ✓ `install`: 214 datotek/26 MB, zagon |
| PuTTY 0.83 | prenosni .exe | ✓ `add` + `run` (okno "PuTTY Configuration") |
| Takehouse AI Music Creator 1.0.0 | Tauri 2 + WebView2 + CUDA | ✓ okno, UI, CUDA (`--device CUDA0`, RTX 3090) |
| CUDA test (`cudatest.exe`) | konzola | ✓ `cuInit success`, SM 8.6, LUID |

Znane omejitve:

* 32-bit programi tečejo prek wow64 (Wine 11.22) — delujejo, a nekateri stari
  installerji (InstallShield 5/6) raje uporabijo `winetricks` override.
* Lokalni "writer" model v Takehouse je CPU-only build (`llama-server`), zato
  teče na CPU; generiranje glasbe prek `ace-server` uporablja CUDA.

## Windows različica v prefixu (pomembno)

Nekateri installerji (Inno Setup, deloma NSIS) preverijo različico Windows in
odklonijo namestitev z *"This program does not support the version of Windows
your computer is running"*. Predloga prefixa je bila pečena z winetricks verbi,
ki so pustili `Windows Server 2003 (5.2, build 3790)`.

Zato `prefix_new()` po razpakiranju predloge **vedno** požene
`setup_defaults()`, ki med drugim:

```bash
wine reg add "HKCU\Software\Wine" /v Version /d win10 /f
wine winecfg -v win10      # posodobi tudi HKLM\...\CurrentVersion (build 19045)
```

Brez tega `install` ne namesti ničesar ("novih/spremenjenih datotek: 0").
Preveri z: `wine cmd /c ver` → `Microsoft Windows 10.0.19045`.

## Grafični vmesnik

`wineforge gui` (zenity) ponuja: dodaj prenosni program, namesti iz installerja,
zaženi, popravi (belo okno), naredi prenosni paket, odpri mapo programov,
diagnoza. Za vsako aplikacijo lahko ustvariš bližnjico v meniju:
`wineforge shortcut <app>`.

## Samodejno skaliranje vmesnika

Chromium/Electron pod Wine **ne upošteva** DPI-ja, ki ga nastavi Wine
(`LogPixels`), zato so okna na večjih zaslonih videti premajhna. Zato motor
ob vsakem `run` (in ob `add`/`install`) požene `auto_scale()`:

1. `find_electron_exe()` preveri mapo exe-ja in do 3 nivojev podmap za datoteke
   `resources.pak`, `icudtl.dat`, `v8_context_snapshot.bin`, `libEGL.dll`
   (tako prepozna tudi launcherje, ki poganjajo pravi Electron exe v `App/…`);
2. če je Electron in v `args` še ni `--force-device-scale-factor`, ga doda iz
   `data/config.json` → `electron_scale` (privzeto 1.5);
3. sicer (klasičen Win32 program) po potrebi nastavi `LogPixels`
   = `96 × win32_dpi_scale`.

Nastavitve: `wineforge config`, per-app pa `wineforge scale <app> <odstotek>`.

## Zaznani tipi namestitvenih programov

| Tip | Tihi ukazi |
|---|---|
| MSI | `/qn /norestart` |
| Inno Setup | `/VERYSILENT /SUPPRESSMSGBOXES /NORESTART /SP-` |
| NSIS | `/S` |
| InstallShield | `/s /v/qn` |
| WiX Burn | `/quiet /norestart` |
| Qt Installer Framework (Qt Creator) | `--accept-licenses --default-answer --confirm-command install` |
| Squirrel | `--silent` |

Če tipa ni mogoče ugotoviti, se installer zažene grafično — namestitev lahko
opraviš ročno, motor pa spremembe vseeno zajame (primerja stanje `drive_c`
pred in po). Začasni prefix se po namestitvi pobriše; za diagnozo ga obdržiš z
`--keep-staging`.
