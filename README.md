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
| `disc` | igre s CD/DVD ali ISO: `--iso`, `--install`, `--link`, `--eject` |
| `media` | preveri/dopolni predvajanje videa v igrah (`--fix`, brez roota) |
| `sim` | skrbniške pravice + windows storitev + identiteta (`--verify`, `--win`, `--pin`) |
| `pad` | navidezni Xbox gamepad za igre (`--start`, `--press`, `--axis`, `--dpad`, `--seq`) |
| `display` | navidezni zaslon, ustreznik IddCx (`--add`, `--shot`, `--video`, `--move`, `--click`) |
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
* **DXVK-NVAPI** (samodejno ob ustvarjanju prefixa): `nvapi64.dll`, ki ga igre
  rabijo za **DLSS, DLSS Frame Generation, Reflex in sledenje žarkov** —
  Windowski `nvapi64.dll` pod Wine ne more delati (govori z windowskim
  gonilnikom). Dodan je tudi Vulkan `reflex` sloj (`VK_ADD_LAYER_PATH`);
  izklop: `WINEFORGE_NO_REFLEX=1`.
* **Podpora za diske** (`disc`): pravi CD/DVD pogon ali ISO slika — Wine dobi
  pogon tipa **CDROM** (preverjeno), brez roota in brez ročnega montiranja.
* **Predvajanje videa** (`media`): UVODNI videi in vgrajeni predvajalniki
  potrebujejo GStreamer demuxerje/dekoderje; manjkajoče motor sam pripelje v
  `engine/tools/gstreamer` (glej *Igre in diski*).
* **winetricks + offline predpomnilnik** (`share/verbs`): dodatne komponente
  delujejo tudi brez interneta.
* Vsaka aplikacija ima **svoj prefix** (kot na Windows), zato si programi ne
  uničijo registra/DLL-jev.

## Igre in diski

### Vse teče lokalno — to ni streaming
Ko zaženeš igro, se nič ne predvaja iz interneta. Datoteke igre so na tvojem
disku, procesor in grafična kartica pa delata v tvojem računalniku.
Wine/Proton je samo **prevajalec**: Windows klice (Direct3D, XInput, registry, datotečni sistem)
pretvori v Linux ekvivalente (Vulkan, evdev, wine prefix). Povezava je
potrebna samo za to, kar potrebuje povezavo tudi na Windowsih: prijava v
Epic/Steam, spletna trgovina, oblačno shranjevanje, multiplayer.
Primer iz prakse: Resident Evil Requiem Demo ima 15,7 GB `.pak` datotek na
NVMe disku, ki jih bere in riše tvoj računalnik — enako kot na Windowsih.
Pravi *cloud gaming* (GeForce Now, Xbox Cloud) je nekaj drugega: tam slika
pride po omrežju z drugega računalnika. WineForge tega ne počne.

### Video: "Failed to create SourceReader with registered byte stream handler"
Igra zažene, pokaže Capcom/EA/Unreal logo, nato skoči okno **Aborted** ali pa se
preprosto zapre,
v dnevniku pa: `winegstreamer error: decodebin0: Your GStreamer installation
is missing a plug-in` in `err:mfplat:stream_handler_BeginCreateObject`.

Winejev Media Foundation (video v igrah in programih) deluje prek
**GStreamerja** gostitelja. Že en manjkajoč vmesnik (`qtdemux` za MP4) zadostuje, da
celotne video verige ni mogoče sestaviti — igra dobi `E_FAIL` in prekine. Preveri in popravi:

```bash
./bin/wineforge media           # kaj manjka
./bin/wineforge media --fix     # prenese manjkajoče pluginje (brez roota)
```

`--fix` prenese pluginje **iste različice** kot je sistemski GStreamer (ABI)
in jih shrani v `engine/tools/gstreamer`, `wine_env` pa zanje nastavi
`GST_PLUGIN_PATH`. Ker so v mapi motorja, potujejo z njim tudi na druge
računalnike in na USB ključek. Preverjeno: po `media --fix` je MP4 predvajanje skozi
`decodebin` uspelo, Resident Evil Requiem Demo pa je prvič prišel čez uvodni
posnetek v meni.

Če uporabljaš Proton/GE neposredno (npr. Epicov Linux launcher z GE-Proton),
je isti problem druge narave: GE ima **svoje** GStreamer pluginje, ti pa so na
Archem prestari — `libgstlibav.so` rabi `libvpx.so.6`, katerega v paketu
ni več, zato se FFmpeg dekoder ne naloži. Popravek: v `files/lib/x86_64-linux-gnu`
protonove mape položi `libvpx.so.6` (npr. iz Debiana 11). Potem video deluje.

### Igre s CD/DVD ali ISO slike

```bash
./bin/wineforge disc                       # kaj je v pogonu + najdeni installerji
./bin/wineforge disc --iso ~/igre/igra.iso # naloži sliko (zanka, brez roota)
./bin/wineforge disc --install --name igra # namesti in naredi prenosno aplikacijo
./bin/wineforge disc --link <prefix>       # poveži disk v obstoječ prefix
./bin/wineforge disc --eject               # odmontiraj in sprosti zanko
```

Kaj se zgodi: `udisks2` naredi zanko in montažo (to sme tudi navaden uporabnik,
root ni potreben), Wine pa samodejno dobi pogon (npr. `F:`) s pravilnim tipom —
`dir f:` v Wineu izpiše `Nosilec v pogonu f je CDROM` vključno s serijsko
številko nosilca. Diski se povežejo v vsak prefix samodejno (`link_discs`),
`autorun.inf` se razčleni, `setup.exe` / `install.msi` pa se ponudi v namestitev.

Omejitve (niso posledica motorja, ampak Winea):

* **zaščita nosilcev**: SafeDisc, SecuROM, Laserlight in starejši StarForce so
  *kernelni gonilniki* — pod Wine ne delujejo nikoli. Za takšne igre je edina
  pot digitalna kopija (GOG/Steam) ali pa pravi Windows.
* CD-Audio sledi na glasbenem CD-ju (`audio01.cda`) Wine ne zna predvajati
  (podatkovni CD-ji z datotekami so brez težav).
* Windows samodejni zagon (`autoplay`) ni podprt; installer zažene `disc
  --install` ali ročno.

### Kaj novejše igre potrebujejo (in kaj je že tu)

| Potreba | Kaj je to | Stanje v WineForge |
|---|---|---|
| DXVK (D3D9/10/11) | prevaja Direct3D v Vulkan | v predlogi prefixa |
| VKD3D-Proton (D3D12) | D3D12 → Vulkan | v predlogi prefixa |
| DXVK-NVAPI | DLSS, Frame Generation, Reflex, RT detekcija | samodejno ob ustvarjanju prefixa |
| Media Foundation | uvodni videi, predvajalniki | `media --fix` (vgrajeni plugini) |
| VC++ 2005–2022 | skoraj vsaka igra | v predlogi prefixa |
| .NET FW 4.8 / .NET 8 | launcherji, orodja | v predlogi prefixa |
| DirectStorage | hitrejše nalaganje (RTX IO) | Wine še nima — igre same padejo na navadno branje datotek (deluje, le nalaganje je malo daljše) |
| DirectPlay | stare LAN igre (Diablo II, StarCraft) | `dpnet`/`dplayx` vgrajena v Wine |
| XAudio2/XInput | zvok in gamepad | v predlogi prefixa |
| CUDA/NVENC/NVML | CUDA pospešek, kodiranje | `engine/nvidia-libs` |

### Kaj pod Wine ne bo delalo (tudi ne v Bottlih ali Protonu)

* **Jedrski anti-cheat**: Easy Anti-Cheat, BattlEye, Riot Vanguard, FACEIT —
  zahtevajo Windows gonilnik, zato teh iger ni mogoče igrati (tudi ne v
  Protonu/Bottlih). Seznam delujočih: [ProtonDB](https://www.protondb.com) in
  [WineHQ AppDB](https://appdb.winehq.org).
* **Windows storitve in PowerShell**: programi, ki med namestitvijo speljejo
  storitev ali poganjajo PowerShell skripte (npr. StarDesk, nekateri VPN-i).
  Del storitve lahko **Simuliraš** (`wineforge sim <app>`, glej spodaj),
  PowerShella pa Wine ne more nadomestiti.
* **WebView2 okna** (Epic Launcher 2.x, novi Outlook, Teams) — Wine bug 59370.
* **Stare DOS in 3dfx/glide igre ter programi za Windows 3.1**: zanje so
  boljši DOSBox Staging, PCjs ali GOG, ne Wine.

### Kako se to primerja s Protonom in Bottles

| | Proton (Steam) | Bottles | WineForge |
|---|---|---|---|
| DirectX → Vulkan | DA (DXVK/VKD3D) | DA (izbereš verzijo) | DA (v predlogi) |
| GStreamer za video | vgrajen v Proton | **običajno ga ni** — rabiš sistemske pakete ali `BOTTLES_USE_SYSTEM_GSTREAMER=1` | vgrajen v motor (`media --fix`) |
| DXVK-NVAPI (DLSS) | DA | DA (z NVIDIA runnerjem) | DA (samodejno) |
| Popravki per-igra (`protonfixes`) | DA | delno (dependencies) | ne — namesto tega `doctor --fix` in `deps` |
| Igra iz diskov | da (Steam) | ročno | `disc --iso` / pravi pogon |
| Potrebuje launcher/račun | DA (Steam) | DA (Flatpak) | ne — samo mapa |

Bottles sam o tem pravi v svojem FAQ: *"Classic Wine, Caffe, Soda, Vaniglia and
most of the Wine versions do not ship any GStreamer plugins ... if a GStreamer
installation is not found on your system, videos won't play."* Pri njih je to
ročno delo (namesti sistemske pakete in nastavi spremenljivko okolja); pri
WineForge to uredi `wineforge media --fix`, plugini pa potujejo z motorjem.

## Program, ki hoče skrbnika ali windows storitev

Nekateri programi (StarDesk, overlay orodja, nekateri VPN-i, programske
tipkovnice) zaženejo še **windows storitev** in hočejo **skrbniške pravice**.
Brez tega se pritožijo (`failed to open service 1060`) ali pa se sploh ne
zaženejo.

Wine ima **pravi SCM** (upravitelj windows storitev): `sc create` dela, njegovi
`services.exe` pa registrirane storitve tudi sam zažene. Zato storitve ni
potrebno ponarejati — installer je samo ne more registrirati, ker teh korakov
pod Wineom ne more opraviti do konca.

```bash
wineforge sim <ime>              # samodejno: najde storitve, registrira, zažene + nastavi pravice
wineforge sim <ime> --verify     # v prefixu poženi sondo: kaj PROGRAM vidi
wineforge sim <ime> --status     # kaj je registrirano in kateri pid teče
wineforge sim <ime> --win 11     # maska Windows 11 23H2 (ver = 10.0.22631)
wineforge sim <ime> --pin        # utrdi identiteto stroja (MachineGuid, HwProfileGuid, …)
wineforge sim <ime> --stop       # ustavi simulirane storitve
wineforge sim <ime> --no-admin   # razveljavi lažno povišanje

# ročno, če samodejno iskanje ne zadene (argumente loči z |):
wineforge sim <ime> --svc "MojaStoritev=C:\Program Files\Program\Storitev.exe|--silent"
```

Samodejno registrira samo binarke, ki se po imenu vedejo kot storitev
(`*service*`, `*daemon*`); ostale samo našteje in predlaga ukaz zanje, da po
nesreči ne zažene `PsExec` ali kaj podobnega.

### Kaj `sim` uredi

1. **Pravice** — v registru nastavi skrbniške politike (`EnableLUA=0`,
   `ConsentPromptBehaviorAdmin=0`, …), da program ne čaka na potrditev UAC.
2. **Storitev** — napiše jo v `HKLM\System\CurrentControlSet\Services` (isto
   kot `sc create`) in jo zažene; če se sama ne zna javiti SCM-ju, jo WineForge
   drži kot navaden proces. Ko je enkrat vpisana, jo `wineforge run <ime>`
   zažene sam pred vsakim zagonem programa.
3. **Identiteta** — `--pin` dopolni manjkajoče identifikatorje (`MachineGuid`,
   `HwProfileGuid`, `InstallDate`), da se "stroj" ne spreminja in programska
   oprema, ki veže licenco na stroj, ne sklene rogate.
4. **Raziskava** — `--verify` prevede in požene sondo v samem prefixu ter
   natisne, kaj program vidi; `--status` našteje jedrske gonilnike v
   namestitvi in ich linuxne ustreznike.

### Kaj je izmerjeno (sonda v prefixu, Wine 11.22)

| kaj program vidi | rezultat |
|---|---|
| `IsInRole(Administrator)` | **True** — Wine-ov uporabnik je skrbnik |
| pravice v žetonu | **21**, med njimi `SeDebug`, `SeShutdown`, `SeLoadDriver`, `SeTcb`, `SeImpersonate` |
| `AdjustTokenPrivileges(SeDebugPrivilege)` | **USPELO** (enako za zgornje vse) |
| storitev, registrirana z `sim` | **Running** — tako v `sc query` kot v **WMI** (`Win32_Service`) in v .NET `ServiceController` |
| `Win32_ComputerSystemProduct` (WMI) | deluje, vrne UUID → strojne preverbe grejo skozi |
| `ver` / `CurrentBuild` | `10.0.22631` po `--win 11` |
| `whoami /priv`, `whoami /groups` | nista implementirana (program, ki ju bere, ne dobi ničesar) |

Od tod sledi pomembno popravilo splošnega verovanja: **pravice pod Wine niso
ovira** — žeton je bogatejši kot na navadnem Windowsu. Ovire so drugje.

### Česa ta simulacija NE more (in zakaj)

* **jedrskih gonilnikov** (`*.sys`). `sim --status` jih najde in razloži.
  Pod Wineom ni načina, da bi jedro Windows obstalo; pod Linuxom ima vsak tak
  gonilnik svoj ustreznik:

  | windows gonilnik | kaj děla | ustreznik pod Linuxom |
  |---|---|---|
  | `ViGEmBus.sys` | navidezni Xbox/DS gamepad | `/dev/uinput` (Sunshine, AntiMicroX) |
  | lastni `*vhid*.sys` | navidezna tipkovnica / miš / pad | `/dev/uinput` |
  | `*Idd*.sys` (IddCx) | navidezni zaslon | `vkms`, Wayland virtual output |
  | `USBPcap` / USB/IP stack | preusmeritev USB naprav | `usbip` (v jedru), `usbipd` |
  | `tap/tun/wintun` | VPN vmesnik | `tun/tap` (NetworkManager) |
  | EAC / BattlEye / Vanguard / FACEIT | jedrni anti-cheat | **ni ustreznika** — igra ne teče |
  | SafeDisc / SecuROM / StarForce | stara zaščita pred kopiranjem | ponatis (GOG/Steam) |

* **SYSTEM računa in njegovih kanalov**: `PsExec -s` pod Wineom registrira svojo
  storitev, pade pa pri kopiranju ključa (`Neveljaven rokop`) — manjka pravi
  SMB + SYSTEM kanal.
* **WebView2 oken** (Epic Launcher 2.x, novi Teams, StarDesk klient in drugi
  Chromium/DirectComposition vmesniki): v poročilu Wine bug 59370 je avtor
  naredil **50+ eksperimentov** in med drugim preizkusil DXVK, llvmpipe,
  SwiftShader, `--use-angle=gl/swiftshader/vulkan`, `--disable-gpu`,
  `--disable-direct-composition` in lastne DComp pogačke — vse vreme. Razlog je
  v zanka `ANGLE → D3D11 deljene teksture → Chromium compositor → DComp`, ki je
  Wine nima; sam upodobljevalnik (renderer) deluje, pokvarjen je le izris.
  Rešitve iz uporabniškega prostora ni.

Skratka: **uporabniški del** (uporabnik, pravice, žeton, storitve, WMI,
register, licenčni identifikatorji) je pod Wineom izenačen ali celo boljši;
**jedrski del in neposredni izris (DComp) ne moreta biti**. Kjer je srž programa
v jedru, je edina zdrava pot linuxni ustreznik — za predvajanje zaslona z
igralno tipkovnico sta to **Sunshine + Moonlight** (uinput, vkms, zvočna zanka)
ali **RustDesk**, za deljenje USB naprav pa `usbip`.

## Navidezni gamepad (`pad`) — ustreznik `ViGEmBus.sys`

Tisto zgoraj ni prazna obljuba: **Linux ima za windowske jedrske gonilnike
navideznih naprav svoj ustreznik**, in Wine ga pokaže windowski igri kot pravo
napravo. `wineforge pad` naredi navidezni `Xbox 360` gamepad v **Linuxovem jedru**
(`/dev/uinput`) — brez roota, brez windowskega kernela, brez virtualnega
računalnika in brez izgube hitrosti (gre za navadni jedrski vhod).

```bash
wineforge pad --start            # ustvari napravo + daemon
wineforge pad --press a          # na voljo: A, B, X, Y, LB, RB, LT, RT,
                                 #   back, start, guide, lstick, rstick
wineforge pad --axis lx 0.9      # lx ly rx ry (od -1 do 1), lt rt (0..1)
wineforge pad --dpad desno       # desno levo gor dol none
wineforge pad --seq "a:0.2:600,b:0.2:600,start"   # niz (gumb:pritisk:pavza_ms)
wineforge pad --stop
```

Izmerjeno na tem sistemu (22. 11. 2026):

| preizkus | rezultat |
|---|---|
| naprava v jedru | `Name="WineForge Virtual Pad"`, `Bus=0003 Vendor=045e Product=028e` (Microsoft Xbox 360), vozlišči `event20 js0` |
| pravice do naprave | udev sam postavi ACL `user:vidov:rw-` na `/dev/input/js0` → ** Wine jo lahko bere brez roota in brez skupine `input` ** |
| `joy.cpl` pod Wineom | zavihek _Input_: »Connected (Input devices): **Controller (XBox 360 for Windows)**« |
| windows klic `XInputGetState` | `kontrolnik 0: POVEZAN`, nato živo: `gumbi=A`, `B`, `X`, `START`, `DPAD_RIGHT`, `LSTICK=(29490,26212)` |
To je torej ista funkcija, kot jo ima `ViGEmBus.sys` (za kar ga potrebujejo
remote-play programi), samo na pravi strani. Uporabno je tudi za igre brez
ročke, za samodejne nize v menijih in za testiranje vhodov.

Opomba: `ly`/`ry` se vpišeta z obrnjenim predznakom, ker ima Linux os Y obrnjeno
glede na XInput — zato `--axis ly 0.8` v igri pomeni **gor**.

Pogoji: naložen modul `uinput` (je, če obstaja `/dev/uinput`) in zapisljiv
`/dev/uinput`. Če ju ni, poskusi `sudo modprobe uinput` (edini morebitni root
korak; na tem sistemu ni bil potreben).

### Drugačen Wine motor

`wineforge` zna zagnati tudi kak drug prenosni Wine (npr. zakrpan build, staging):

```bash
WINEFORGE_ENGINE=$PWD/engine/alt/wine-dcomp ./bin/wineforge run igra
```

## Ali DComp zakrpe rešijo WebView2 okna? (izmerjeno 22. 11. 2026)

Namesto ugibanja sem pripel zakrpan Wine in ga izmeril ob navadnega. Zakrpe so
serija `d2d1`/`dcomp` (giang17/wine, samodejne gradnje `Mark12870/wine-d2d1-dcomp`,
`wine-11.0-887`), razpakirane v `engine/alt/wine-dcomp` — brez roota, enako kot
pri osnovnem motorju. Zagon: `WINEFORGE_ENGINE=$PWD/engine/alt/wine-dcomp bin/wineforge …`

Sonda `scripts/probe-dcomp.cs` (prenese jo `csc.exe` v prefixu) vrne:

| klic | Wine 11.22 (navadni) | zakrpan build (d2d1-dcomp) |
|---|---|---|
| `D3D11CreateDevice(HARDWARE)` | `0x80070057` | `0x80070057` |
| `D3D11CreateDevice(WARP)` | `S_OK` | `S_OK` |
| `DCompositionCreateDevice(..., IID_IUnknown)` | `E_NOTIMPL` — pogačka | **`S_OK` — pravi objekt** |
| `DCompositionCreateDevice(..., IID_IDCompositionDevice)` | `E_NOTIMPL` | `E_NOINTERFACE` |
| `DCompositionCreateDevice(..., IID_IDCompositionDevice2)` | `E_NOTIMPL` | `E_NOINTERFACE` |
| `DCompositionCreateSurfaceHandle` | **zruši proces** | `S_OK` |

Zakrpe torej **niso pogačka**: DComp objekt res nastane. Vendar objekt še vedno
**ne odgovori na vmesnik, ga aplikacije prosijo** (`IDCompositionDevice` / `-Device2`
→ `E_NOINTERFACE`), chromiumov compositor pa prav teh vmesnikov rabi.

### Kje se danes zatakne WebView2 (Epic / novi Teams / StarDesk klient)

WebView2 runtime 157.0.4341.60 se v prefix normalno **nainstalira** (692 MB, kar
zgodaj ni šlo), Epicov launcher (v2, Rust + WebView2) pod Wineom tudi naredi
okno 1280×720 — nato pa **pade sam chromiumov browser proces**, in šele to je
danes pravi zid:

```
ERROR eglv2_lib::webview::navigation::windows: WebView2 browser process exited
      mid-session; every webview is gone ... exit_code_hex=0x80000003
Watson: ApplicationName=msedgewebview2.exe; ModuleName=msedge.dll;
        ModuleOffset=191849725; ProcessType=browser; SubCode=0x80000003
```

`0x80000003` je Chromiumjev lasten `CHECK`/`IMMEDIATE_CRASH` (ukaz `int3`) — ne
Wineova izjema. Enako se zgodi na obeh motorjih in z `--no-sandbox`,
`--disable-gpu`, `--disable-direct-composition`, `--in-process-gpu` ter z
izklopljenim DXVK-NVAPI Reflex slojem (`WINEFORGE_NO_REFLEX=1`). Mini dump sem
razčlenil (`Crashpad/reports/*.dmp`) in zlom je znotraj `msedge.dll` na pomiku
`0xb6f64fd`, proces tipa `browser`.

**Zaključek:** zakrpe DComp dejansko dopolni, a WebView2 oken še ne odprejo, ker
se veriga prelomi *prej* — v chromiumovi lastni validaciji. Pogoji za ponovitev:

```bash
# 1) zakrpan motor v prefix (ne rabi roota; ~220 MB)
U=https://github.com/Mark12870/wine-d2d1-dcomp/releases/latest/download/
U+=wine-d2d1-dcomp-11.0-887-p4-x86_64.tar.zst
curl -sL "$U" -o engine/alt/dcomp.tar.zst
mkdir -p engine/alt/wine-dcomp && tar --zstd -xf engine/alt/dcomp.tar.zst -C engine/alt/wine-dcomp
mv engine/alt/wine-dcomp/wine-d2d1-dcomp-*/{bin,lib,share} engine/alt/wine-dcomp/  # izravna strukturo
rmdir engine/alt/wine-dcomp/wine-d2d1-dcomp-*
# 2) zagon poljubnega ukaza z njim
WINEFORGE_ENGINE=$PWD/engine/alt/wine-dcomp bin/wineforge run <igor>
# 3) sonda kaj DComp zmore (prevaja jo csc.exe v prefixu)
WINEPREFIX=$PWD/prefixes/<igor> engine/wine/bin/wine \
  C:\\windows\\Microsoft.NET\\Framework64\\v4.0.30319\\csc.exe /nologo \
  /out:Z:\\tmp\\dprobe.exe /reference:System.Windows.Forms.dll \
  /reference:System.Drawing.dll Z:\\tmp\\dprobe.exe
```

## Zakaj StarDesk klient še vedno ne odpre (izmerjeno 22. 11. 2026)

Klient (`bin\StarDesk.exe`) se konča s kodo **255** v ~2 s, brez okna in brez
zrušitve. Tokrat smo ga ujeli na delu:

1. **Navidezni zaslon ni bil ovira.** Z `wineforge display` smo naredili 1920×1080
   zaslon in vanj poslali klient (`DISPLAY=:7`). `winex11.drv` se naloži (torej X
   povezava je uspešna), a se klient vseeno konča s 255 → risba ni vzrok.
2. **WebView2 runtime je nameščen** (ga je namestil sam instalator:
   `C:\Program Files (x86)\Microsoft\EdgeWebView\Application\157.0.4341.60`, 692 MB);
   `WebView2Loader.dll` se naloži, a se `msedgewebview2.exe` nikoli ne zažene →
   klient umre **pred** UI-jem.
3. **Ni manjkajočega API-ja.** Klient statically uvaža 3569 funkcij iz 49 DLL-jev;
   od `wtsapi32` hoče `WTSQueryUserToken`, `WTSEnumerateSessionsW`,
   `WTSQuerySessionInformationW`, `WTSRegisterSessionNotification` — **v Wine 11.22
   jih ima vseh 6** (egzportov je 52); `userenv` (`LoadUserProfileW`,
   `CreateEnvironmentBlock`), `netapi32` in `setupapi` (poizvedba po gonilnikih) so
   prav tako prisotni.
4. **Pravi zadnji korak je SSPI/LSA.** Sled (+file) pokaže, da je zadnje pred
   izhodom odpiranje `\\.\pipe\lrpc\lsasspirpc` (Wineova cev do `lsass`), tik
   prej se naložita `Kerberos.dll` in `MSV1_0.dll`. Lastna sonda
   (`scripts/probe-sspi.cs`) v istem prefixu izmeri:

   | klic | rezultat |
   |---|---|
   | `\\.\pipe\lrpc\lsasspirpc` | **odprta** (LSA je dosegljiva) |
   | `AcquireCredentialsHandle("Negotiate"/"NTLM"/"Schannel")` | `S_OK` |
   | `AcquireCredentialsHandle("Kerberos")` | `0x8009030E` **SEC_E_NO_CREDENTIALS** |
   | `AcquireCredentialsHandle("MSV1_0"/"CredSSP")` | `0x80090305` **SEC_E_SECPKG_NOT_FOUND** |
   | `InitializeSecurityContext("Negotiate"/"NTLM", "localhost")` | `0x80090301` **SEC_E_INVALID_HANDLE** |

   Po Wineovi plati torej **klic na SSPI ne more začeti varne seje**: Kerberos nima
   poverilnic (ni domene/prijave), MSV1_0 in CredSSP kot paketa v Wineovem LSA
   sploh nista registrirana. To je tudi razlog, da `PsExec -s` prej ni mogel
   zapisati svojega ključa (`Neveljaven ročnik`).

Poleg tega ostaja jedrni del (brez tega StarDesk na Windowsih ne deluje normalno):
`sdvhid.sys`, `ViGEmBus.sys`, `SDIddDriver` (IddCx), `SDUSB(.Mon).sys` — Wine nima
jedra in jih ne more naložiti. Za dva od njiju ima WineForge že **nativna
ustreznika**: `wineforge pad` (uinput namesto ViGEmBus/sdvhid) in
`wineforge display` (namesto IddCx).

Kaj bi še lahko poskusili (vrstni red po smislu):

1. **Nadomestna SSPI knjižnica** — lasten `secur32.dll` kot proxy, ki za pakete
   Negotiate/NTLM/Kerberos vrne samoskladno lažno lokalno avtentikacijo (storitev
   in klient sta v istem prefixu, se torej vidita). Delno delo je veliko,
   uspeh ni gotov; ta sistem poleg tega **nima prekriznega prevajalnika**
   (`x86_64-w64-mingw32-gcc` / `clang` nista nameščena) — rabili bi prenosni
   `llvm-mingw`.
2. **Pravi Windows v virtualnem racunalniku** — edina pot, ki res dobi jedro,
   gonilnike, UAC, SSPI in delujoč WebView2 hkrati. `/dev/kvm` je na voljo in
   procesor podpira VT-x; manjka še QEMU in Windows ISO.

Sonde, s katerima se to ponovi:

```bash
wineforge sim <prefix> --verify          # pravice, storitve, identiteta
wine 'C:\temp\sspi.exe'                   # scripts/probe-sspi.cs — kaj vrne SSPI
wine 'C:\temp\wts.exe'                    # scripts/probe-wts.cs  — sej in WTS
```

## Navidezni zaslon (`display`) — ustreznik `SDIddDriver` / IddCx

Programi za predvajanje zaslona (StarDesk, Sunshine, Parsec) imajo na Windowsih
navidezni monitor, ki ga naredi **jedrski gonilnik IddCx**. Pod Linuxom bi ga
naredil `vkms` — a ta rabi roota in jedrski modul. WineForge ga naredi ravno
korak nizje in **brez roota**: `Xwayland` v koreninskem nacinu naredi okno s
poljubno velikostjo, ki je hkrati **pravi X zaslon** — GPU na njem dela, vanj
lahko vbrizgamo mis in tipkovnico, in ga lahko zajamemo.

```bash
wineforge display --add 1920x1080 --name prenos   # ustvari zaslon (in nadzornika)
wineforge run igra --display prenos               # zaženi igro NA njem
wineforge display --shot --name prenos            # slika zaslona
wineforge display --video --sekunde 20 --out posnetek.mp4
wineforge display --move 800 400 --name prenos    # mis (XTEST)
wineforge display --click --name prenos
wineforge display --key 39 --name prenos          # X keysym koda
wineforge display --list / --remove prenos | vse
```

Izmerjeno 22. 11. 2026 na tem sistemu (EndeavourOS, Wayland, RTX 3090):

| preizkus | rezultat |
|---|---|
| zagon | `--add 1280x720` → prikaz `:7`, **točno 1280×720**, stabilno |
| pospeševanje | `vkcube --wsi xcb` na tem zaslonu: »Selected GPU 0: NVIDIA GeForce RTX 3090«, izhod 0 → **Vulkan WSI dela, torej dela tudi DXVK** |
| Wine | `DISPLAY=:7 wine notepad.exe` in `wineforge run --display` → okna se rišejo na navidezni zaslon |
| zajem | `--shot` → PNG 1280×720 **z vsebino in kazalcem** (ne črno) |
| video | `--video --sekunde 4` → **120 sličic = 30 fps** pri 1280×720 |
| vnos | XTEST: kaza se je premaknila na (800,400), klik in tipka sprejeta |
| odpornost | `kill -9` Xwaylandu → nadzornik ga v ~6 s pobudi nazaj, zajem po tem deluje |

S tem je trojica, ki jo StarDesk počne z jedrnimi gonilniki, pokrita brez
windowskega jedra in brez virtualnega računalnika:

| StarDesk na Windowsih | tu |
|---|---|
| `SDIddDriver.sys` / IddCx (navidezni monitor) | `wineforge display` (Xwayland rootful) |
| `sdvhid.sys` / `ViGEmBus.sys` (navidezna tipkovnica, miš, ročka) | `wineforge pad` (`/dev/uinput`) |
| zajem + stream | `display --video` / `--shot` (ffmpeg x11grab) |

**Kaj je drugače od jedrskega gonilnika (pošteno):** zaslon je sestavljen na
ravni sejanja, ne na DRM CRTC-ju. Program, ki našteva monitorje prek DRM
(`kscreen-doctor`, `xrandr` na pravih izhodih) ali gre v **exclusive fullscreen**,
jega ne bo videl — navidezni zaslon je okno v seji. Za resnično glavo brez
monitorja dodaš `--add fullscreen` (pokrije cel zaslon), pravo jedrsko
rešitev (`vkms`, DRM lease) pa dobiš samo z rootom in modulom.

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
