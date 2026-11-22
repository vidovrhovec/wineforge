/*
 * wineforge — nadomestni SSPI modul (secur32.dll / sspicli.dll)
 *
 * Zakaj: Wine 11 goni NTLM/Negotiate prek LSA (lsa_InitializeSecurityContextW)
 * in ta pot v lsasrv ni izpeljana, zato InitializeSecurityContext vrne
 * SEC_E_INVALID_HANDLE.  Programi za oddaljen namizje (StarDesk in sorodni)
 * prav na tem končajo takoj, brez okna.
 *
 * Ta modul naredi isto, kar Windows počne za lokalno avtentikacijo storitve:
 * klient in storitev (oba v istem prefixu) se prepričata drug o drugem s
 * tokeni, ki jih generira ta modul.  Ker obe strani uporabljata ta isti modul,
 * je rokovalni protokol samoskladen — program dobi ime prijavitelja, sejno
 * ključ in šifriranje, kot ga bi na Windowsih.
 *
 * Vse, kar ni nuestra paketa (Schannel / TLS), gre nespremenjeno naprej na
 * Wineov pravi modul, da HTTPS dela kot doslej.
 */

#define SECURITY_WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stddef.h>
#include <objbase.h>
#include <security.h>
#include <sspi.h>
#include <stdio.h>
#include <string.h>

#define WF_NONCE   16
#define WF_MAC     32
#define WF_MAXID   64
#define TAG        0x5746u

/* ------------------------------------------------------------- SHA-256 ---- */

typedef struct { uint32_t h[8]; uint64_t len; uint32_t buf[16]; size_t fill; } sha256_ctx;

static const uint32_t K256[64] = {
    0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
    0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
    0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
    0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
    0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
    0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
    0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
    0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u };

#define ROTR(x,n) (((x) >> (n)) | ((x) << (32-(n))))

static void sha256_block(sha256_ctx *c, const uint32_t *w) {
    uint32_t t[64], a,b,cc,d,e,f,g,h;
    for (int i = 0; i < 16; i++) {
        const unsigned char *p = (const unsigned char *)&w[i];
        t[i] = ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];
    }
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = ROTR(t[i-15],7)^ROTR(t[i-15],18)^(t[i-15]>>3);
        uint32_t s1 = ROTR(t[i-2],17)^ROTR(t[i-2],19)^(t[i-2]>>10);
        t[i] = t[i-16]+s0+t[i-7]+s1;
    }
    a=c->h[0];b=c->h[1];cc=c->h[2];d=c->h[3];e=c->h[4];f=c->h[5];g=c->h[6];h=c->h[7];
    for (int i = 0; i < 64; i++) {
        uint32_t S1 = ROTR(e,6)^ROTR(e,11)^ROTR(e,25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t t1 = h + S1 + ch + K256[i] + t[i];
        uint32_t S0 = ROTR(a,2)^ROTR(a,13)^ROTR(a,22);
        uint32_t mj = (a & b) ^ (a & cc) ^ (b & cc);
        uint32_t t2 = S0 + mj;
        h=g; g=f; f=e; e=d+t1; d=cc; cc=b; b=a; a=t1+t2;
    }
    c->h[0]+=a;c->h[1]+=b;c->h[2]+=cc;c->h[3]+=d;c->h[4]+=e;c->h[5]+=f;c->h[6]+=g;c->h[7]+=h;
}

static void sha256_init(sha256_ctx *c) {
    c->h[0]=0x6a09e667u;c->h[1]=0xbb67ae85u;c->h[2]=0x3c6ef372u;c->h[3]=0xa54ff53au;
    c->h[4]=0x510e527fu;c->h[5]=0x9b05688cu;c->h[6]=0x1f83d9abu;c->h[7]=0x5be0cd19u;
    c->len = 0; c->fill = 0;
}

static void sha256_update(sha256_ctx *c, const void *data, size_t n) {
    const unsigned char *p = (const unsigned char *)data;
    c->len += n;
    unsigned char *fb = (unsigned char *)c->buf;
    while (n) {
        size_t k = 64 - c->fill;
        if (k > n) k = n;
        memcpy(fb + c->fill, p, k);
        c->fill += k; p += k; n -= k;
        if (c->fill == 64) { sha256_block(c, c->buf); c->fill = 0; }
    }
}

static void sha256_final(sha256_ctx *c, unsigned char out[32]) {
    uint64_t bits = c->len * 8;
    unsigned char pad = 0x80;
    sha256_update(c, &pad, 1);
    unsigned char z = 0;
    while (c->fill != 56) sha256_update(c, &z, 1);
    unsigned char lb[8];
    for (int i = 0; i < 8; i++) lb[i] = (unsigned char)(bits >> (56 - 8*i));
    sha256_update(c, lb, 8);
    for (int i = 0; i < 8; i++) {
        out[i*4]   = (unsigned char)(c->h[i] >> 24);
        out[i*4+1] = (unsigned char)(c->h[i] >> 16);
        out[i*4+2] = (unsigned char)(c->h[i] >> 8);
        out[i*4+3] = (unsigned char)(c->h[i]);
    }
}

static void sha256(const void *d, size_t n, unsigned char out[32]) {
    sha256_ctx c; sha256_init(&c); sha256_update(&c, d, n); sha256_final(&c, out);
}

static void hmac_sha256(const unsigned char *key, size_t kn, const unsigned char *msg,
                        size_t mn, unsigned char out[32]) {
    unsigned char k[64], ip[64], op[64], inner[32];
    memset(k, 0, sizeof k);
    if (kn > 64) sha256(key, kn, k); else memcpy(k, key, kn);
    for (int i = 0; i < 64; i++) { ip[i] = k[i] ^ 0x36; op[i] = k[i] ^ 0x5c; }
    sha256_ctx c;
    sha256_init(&c); sha256_update(&c, ip, 64); sha256_update(&c, msg, mn); sha256_final(&c, inner);
    sha256_init(&c); sha256_update(&c, op, 64); sha256_update(&c, inner, 32); sha256_final(&c, out);
}

/* ------------------------------------------------------ Our SSPI tokens ---- */

#define MAGIC   0x53534657u   /* "WFSS" */
#define VER     1u
#define T_NEG   1u
#define T_CHL   2u
#define T_AUTH  3u
#define NONCE   16
#define MAC     32
#define MAXID   64

#pragma pack(push, 1)
typedef struct {
    uint32_t magic;
    uint32_t ver;
    uint32_t type;
    uint32_t flags;
    uint16_t user[MAXID];
    uint16_t domain[MAXID];
    unsigned char nonce[NONCE];
    unsigned char mac[MAC];
} wf_token;
#pragma pack(pop)

/* --------------------------------------------------------------- stanje ---- */

static unsigned char sekreta[32];
static BOOL sekret_nalozen = FALSE;
static CRITICAL_SECTION zaklep;
static BOOL init_done = FALSE;

typedef struct {
    BOOL v_uporabi;
    wchar_t paket[32];
    DWORD uporaba;              /* SECPKG_CRED_INBOUND/OUTBOUND */
    wchar_t user[MAXID];
    wchar_t domain[MAXID];
    /* napovedan pravi ročaj (za Schannel) */
    CredHandle pravi;
    BOOL je_pravi;
} wf_cred;

typedef struct {
    BOOL v_uporabi;
    wchar_t paket[32];
    BOOL ustanovljen;
    BOOL je_streznik;
    wchar_t peer_user[MAXID];
    wchar_t peer_domain[MAXID];
    unsigned char sea_key[32];
    unsigned char moj_nonce[NONCE];
    unsigned char tuj_nonce[NONCE];
    uint64_t stevec_posiljanja;
    uint64_t stevec_prejemanja;
    CtxtHandle pravi;
    BOOL je_pravi;
} wf_ctx;

#define MAX_CRED 128
#define MAX_CTX  256
static wf_cred kred[MAX_CRED];
static wf_ctx  kontek[MAX_CTX];

/* pravi (Wineov) secur32, če ga najdemo */
typedef SECURITY_STATUS (*fn_acquire)(PCWSTR, PCSTR, ULONG, LPVOID, PVOID,
                                      SEC_GET_KEY_FN, PVOID, PCredHandle, PTimeStamp);
typedef struct {
    HMODULE modul;
    void *_acquire, *acquire_a;
    void *free_cred, *free_ctx_buf;
    void *isc, *isc_a, *accept, *complete, *delete_ctx;
    void *qca, *qca_a, *qcpa, *qcpa_a, *esp, *esp_a;
    void *enc, *dec, *imp, *rev, *sign, *verify, *setca, *token;
} privi_api;
static privi_api pravi;

/* ---------------------------------------------- identiteta in tajni klju --- */

static void nalozi_secret(void) {
    if (sekret_nalozen) return;
    HKEY k;
    memset(sekreta, 0, sizeof sekreta);
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\WineForge\\SSPI", 0,
                      KEY_READ, &k) == ERROR_SUCCESS) {
        DWORD tip = 0, dolz = sizeof sekreta;
        if (RegQueryValueExW(k, L"Secret", 0, &tip, (BYTE *)sekreta, &dolz) != ERROR_SUCCESS
            || tip != REG_BINARY || dolz != 32) {
            /* ni nastavljen: izpeljemo iz imena racunalnika — v istem prefixu je
               vedno enak, torej je protokol samoskladen */
            wchar_t rac[128];
            DWORD n = 128;
            GetComputerNameW(rac, &n);
            unsigned char h[32];
            sha256(rac, wcslen(rac) * 2, h);
            memcpy(sekreta, h, 32);
        }
        RegCloseKey(k);
    } else {
        wchar_t rac[128]; DWORD n = 128;
        GetComputerNameW(rac, &n);
        unsigned char h[32];
        sha256(rac, wcslen(rac) * 2, h);
        memcpy(sekreta, h, 32);
    }
    sekret_nalozen = TRUE;
}

static void naša_identiteta(wchar_t *user, wchar_t *domain);

static void nas_ident(wchar_t *user, wchar_t *domain) { naša_identiteta(user, domain); }

static BOOL ime_procesa(wchar_t *buf, size_t cap) {
    wchar_t pot[MAX_PATH];
    DWORD n = GetModuleFileNameW(NULL, pot, MAX_PATH);
    if (!n || n >= MAX_PATH) return FALSE;
    wchar_t *z = wcsrchr(pot, L'\\');
    if (!z) z = pot; else z++;
    wcsncpy(buf, z, cap - 1);
    buf[cap-1] = 0;
    return TRUE;
}

static BOOL je_storitev(void) {
    wchar_t ime[64];
    if (!ime_procesa(ime, 64)) return FALSE;
    for (wchar_t *p = ime; *p; p++) *p = (wchar_t)towlower(*p);
    return wcsstr(ime, L"service") || wcsstr(ime, L"daemon") || wcsstr(ime, L"_svc")
        || wcsstr(ime, L"srv.exe");
}

/* poverilnice, ki jih predstavimo: storitev => LocalSystem, sicer trenutni user */
static void naša_identiteta(wchar_t *user, wchar_t *domain) {
    HKEY k;
    user[0] = domain[0] = 0;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\WineForge\\SSPI", 0, KEY_READ, &k)
        == ERROR_SUCCESS) {
        DWORD tip = 0, d;
        d = MAXID * 2;
        RegQueryValueExW(k, L"ServiceUser", 0, &tip, (BYTE *)user, &d);
        d = MAXID * 2;
        RegQueryValueExW(k, L"ServiceDomain", 0, &tip, (BYTE *)domain, &d);
        RegCloseKey(k);
        if (je_storitev() && user[0]) {
            if (!domain[0]) { DWORD n = MAXID; GetComputerNameW(domain, &n); }
            return;
        }
    }
    DWORD n = MAXID;
    GetUserNameW(user, &n);
    n = MAXID;
    GetComputerNameW(domain, &n);
}

static void nakljusje(unsigned char *p, size_t n) {
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    DWORD s = (DWORD)(c.QuadPart ^ (ULONG_PTR)p ^ GetCurrentProcessId());
    for (size_t i = 0; i < n; i++) {
        s = s * 1103515245u + 12345u;
        p[i] = (unsigned char)(s >> 16);
    }
}

static void izracun_mac(wf_token *t, BOOL preveri, BOOL *pravilno) {
    unsigned char mac[MAC];
    size_t dolz = offsetof(wf_token, mac);
    hmac_sha256(sekreta, 32, (const unsigned char *)t, dolz, mac);
    if (preveri) *pravilno = (memcmp(mac, t->mac, MAC) == 0);
    else memcpy(t->mac, mac, MAC);
}

/* ------------------------------------------------------ sejni klju + SIF --- */

static void izpelji_sejni_klju(wf_ctx *c) {
    /* vrstni red nonce-jev je enak na obeh straneh (manjsi naprej), sicer se
       klient in storitev ne bi sešla */
    unsigned char h[32], bl[96];
    const unsigned char *a = c->moj_nonce, *b = c->tuj_nonce;
    if (memcmp(a, b, WF_NONCE) > 0) { const unsigned char *t = a; a = b; b = t; }
    memcpy(bl, sekreta, 32);
    memcpy(bl + 32, a, WF_NONCE);
    memcpy(bl + 32 + WF_NONCE, b, WF_NONCE);
    memcpy(bl + 32 + 2 * WF_NONCE, "WineForgeSSPI/1", 15);
    sha256(bl, 32 + 2 * WF_NONCE + 15, h);
    memcpy(c->sea_key, h, 32);
}

static void tokovi_kljuca(const unsigned char *key, uint64_t stevec, size_t n,
                          unsigned char *izpis) {
    unsigned char bl[40];
    size_t made = 0;
    unsigned long long st = 0;
    while (made < n) {
        memcpy(bl, key, 32);
        memcpy(bl + 32, &stevec, 8);
        bl[39] = (unsigned char)st;
        unsigned char h[32];
        sha256(bl, 40, h);
        size_t k = n - made; if (k > 32) k = 32;
        memcpy(izpis + made, h, k);
        made += k; st++;
    }
}

static void prekrij(const unsigned char *key, uint64_t stevec, unsigned char *podatki, size_t n) {
    if (!n) return;
    unsigned char *s = (unsigned char *)HeapAlloc(GetProcessHeap(), 0, n);
    if (!s) return;
    tokovi_kljuca(key, stevec, n, s);
    for (size_t i = 0; i < n; i++) podatki[i] ^= s[i];
    HeapFree(GetProcessHeap(), 0, s);
}

/* --------------------------------------------------------------- paketi ---- */

typedef struct { LPCSTR ime; LPCSTR komentar; ULONG rpc; ULONG maxtok; ULONG zmozn; } paket_info;

static const paket_info PAKETI[] = {
    { "NTLM",       "NTLM Security Package",            10,  1904,  0x00082b37 },
    { "Negotiate",  "Microsoft Package Negotiator",      9, 48256,  0x00083bb3 },
    { "Kerberos",   "Microsoft Kerberos V1.0",          16, 48000,  0x0008f3bbf },
    { "MsV1_0",     "Microsoft Authentication Package v1.0", 1, 1904, 0x00082b31 },
    { "CredSSP",    "Credential Security Support Provider", 21, 8192, 0x000f3bb1 },
    { "pku2u",      "PKU2U Security Package",           18,  8192,  0x000f3bb1 },
    { "wdigest",    "WDigest Security Package",         15,  4096,  0x000f3bb1 },
    { "Tspkg",      "TSPkg Security Package",            7,  4096,  0x000f3bb1 },
};
#define STEV_PAKETOV (sizeof(PAKETI)/sizeof(PAKETI[0]))

/* Windows postavitve, ki jih mingrove glave nimajo enake */
#ifndef SECPKG_ATTR_AUTH_ID
#define SECPKG_ATTR_AUTH_ID 7
#endif
typedef struct { ULONG SessionKeyLength; PUCHAR SessionKey; } wf_sski;
typedef struct { SEC_WCHAR *Authority; } wf_avtorW;
#define TOKEN_PSEVDOK (HANDLE)(LONG_PTR)-4

/* Windows-A in Windows-W glave razhajajo se glede tipa imena paketa, zato
   ga prepoznamo po baytovih: 'N',0x00 => ozko (UTF-16), sicer ozko (ANSI). */
static void paket_v_ascii(const void *p, char *iz, size_t n) {
    iz[0] = 0;
    if (!p) return;
    const unsigned char *b = (const unsigned char *)p;
    if (b[1] == 0) {                       /* UTF-16 */
        WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR)p, -1, iz, (int)n, NULL, NULL);
    } else {
        size_t i = 0;
        while (b[i] && i + 1 < n) { iz[i] = (char)b[i]; i++; }
        iz[i] = 0;
    }
}

static BOOL paket_nas(const char *ime) {
    if (!ime) return FALSE;
    for (size_t i = 0; i < STEV_PAKETOV; i++)
        if (_stricmp(ime, PAKETI[i].ime) == 0) return TRUE;
    return FALSE;
}

static BOOL nasi_paket(const paket_info *p, SecPkgInfoW *iz) {
    iz->fCapabilities = p->zmozn;
    iz->wVersion = 1;
    iz->wRPCID = (USHORT)p->rpc;
    iz->cbMaxToken = p->maxtok;
    size_t n = strlen(p->ime) + 1;
    iz->Name = (LPWSTR)CoTaskMemAlloc(n * sizeof(WCHAR));
    iz->Comment = (LPWSTR)CoTaskMemAlloc((strlen(p->komentar) + 1) * sizeof(WCHAR));
    if (!iz->Name || !iz->Comment) return FALSE;
    MultiByteToWideChar(CP_UTF8, 0, p->ime, -1, iz->Name, (int)n);
    MultiByteToWideChar(CP_UTF8, 0, p->komentar, -1, iz->Comment, (int)strlen(p->komentar) + 1);
    return TRUE;
}

/* ------------------------------------------------------ pravi modul ------- */

static void nalozi_pravi(void) {
    static const wchar_t *poti[] = {
        L"C:\\windows\\system32\\wine-secur32.dll",
        L"C:\\windows\\secur32-real.dll",
        L"C:\\temp\\secur32b.dll",
    };
    for (size_t i = 0; i < sizeof(poti)/sizeof(poti[0]) && !pravi.modul; i++)
        pravi.modul = LoadLibraryW(poti[i]);
    if (!pravi.modul) return;
    pravi._acquire   = (void *)GetProcAddress(pravi.modul, "AcquireCredentialsHandleW");
    pravi.acquire_a  = (void *)GetProcAddress(pravi.modul, "AcquireCredentialsHandleA");
    pravi.free_cred  = (void *)GetProcAddress(pravi.modul, "FreeCredentialsHandle");
    pravi.free_ctx_buf = (void *)GetProcAddress(pravi.modul, "FreeContextBuffer");
    pravi.isc        = (void *)GetProcAddress(pravi.modul, "InitializeSecurityContextW");
    pravi.isc_a      = (void *)GetProcAddress(pravi.modul, "InitializeSecurityContextA");
    pravi.accept     = (void *)GetProcAddress(pravi.modul, "AcceptSecurityContext");
    pravi.complete   = (void *)GetProcAddress(pravi.modul, "CompleteAuthToken");
    pravi.delete_ctx = (void *)GetProcAddress(pravi.modul, "DeleteSecurityContext");
    pravi.qca        = (void *)GetProcAddress(pravi.modul, "QueryContextAttributesW");
    pravi.qcpa       = (void *)GetProcAddress(pravi.modul, "QuerySecurityPackageInfoW");
    pravi.esp        = (void *)GetProcAddress(pravi.modul, "EnumerateSecurityPackagesW");
    pravi.enc        = (void *)GetProcAddress(pravi.modul, "EncryptMessage");
    pravi.dec        = (void *)GetProcAddress(pravi.modul, "DecryptMessage");
    pravi.token      = (void *)GetProcAddress(pravi.modul, "QuerySecurityContextToken");
}

/* ------------------------------------------------------------- init -------- */

/* -------------------------------------------------------------- dnevnik ---- */
/* Z WINEFORGE_SSPI_LOG='C:\...\log.txt' modul zapiše vsak SSPI klic: kaj je
   dobil noter in kaj je vrnil. Brez spremenljivke se ne piše nič. */
static FILE *dp = NULL;

static void odpri_dnevnik(void) {
    if (dp) return;
    wchar_t pot[600];
    DWORD n = GetEnvironmentVariableW(L"WINEFORGE_SSPI_LOG", pot, 590);
    if (!n || n > 590) return;
    char utf[1200];
    WideCharToMultiByte(CP_UTF8, 0, pot, -1, utf, sizeof utf, NULL, NULL);
    dp = fopen(utf, "a");
    if (dp) { fprintf(dp, "=== zagon, pid %lu ===\n", (unsigned long)GetCurrentProcessId()); fflush(dp); }
}

static void dnevnik(const char *klic, const char *podrobnosti, SECURITY_STATUS rezultat) {
    if (!dp) return;
    EnterCriticalSection(&zaklep);
    fprintf(dp, "%-24s %-42s -> 0x%08x\n", klic, podrobnosti ? podrobnosti : "",
            (unsigned)rezultat);
    fflush(dp);
    LeaveCriticalSection(&zaklep);
}

static void siroko_v_utf8(const void *siroko, char *iz, size_t n) {
    iz[0] = 0;
    if (!siroko) return;
    const unsigned char *b = (const unsigned char *)siroko;
    if (b[1] == 0) WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR)siroko, -1, iz, (int)n, NULL, NULL);
    else { size_t i = 0; while (b[i] && i + 1 < n) { iz[i] = (char)b[i]; i++; } iz[i] = 0; }
}

static void opisi_mede(const char *klic, const void *rocaj, char *iz, size_t n) {
    if (!rocaj) { snprintf(iz, n, "%s=nic", klic); return; }
    const SecHandle *s = (const SecHandle *)rocaj;
    snprintf(iz, n, "%s=0x%llx/%u", klic, (unsigned long long)s->dwLower,
             (unsigned)((s->dwLower >> (8 * sizeof(void *) - 16)) == (ULONG_PTR)TAG));
}

static void opisi_pisave(const char *klic, PSecBufferDesc d, char *iz, size_t n) {
    if (!d) { snprintf(iz, n, "%s=nic", klic); return; }
    ULONG skupaj = 0;
    for (ULONG i = 0; i < d->cBuffers; i++) skupaj += d->pBuffers[i].cbBuffer;
    snprintf(iz, n, "%s[%u, %u B]", klic, (unsigned)d->cBuffers, (unsigned)skupaj);
}

static void init_if(void) {
    if (init_done) return;
    InitializeCriticalSection(&zaklep);
    nalozi_secret();
    nalozi_pravi();
    odpri_dnevnik();
    init_done = TRUE;
}

static int kred_dodaj(void) {
    for (int i = 0; i < MAX_CRED; i++) if (!kred[i].v_uporabi) { kred[i].v_uporabi = TRUE; return i; }
    return -1;
}
static void kontek_izbaci(int i) { if (i >= 0 && i < MAX_CTX) kontek[i].v_uporabi = FALSE; }

static int ctx_dodaj(void) {
    for (int i = 0; i < MAX_CTX; i++) if (!kontek[i].v_uporabi) { kontek[i].v_uporabi = TRUE; return i; }
    return -1;
}

/* Naš ročaj nosi žig v spodnjih 8 bajtih (zgornjih ne pišemo): nekateri
   prevajalniki in ovojnice (npr. .NET DllImport z IntPtr) ohranijo le eno
   besedo na strojevo dolžino, zato zgornje ne smemo uporabljati. */
static void naredi_rocaj(void *h, int index) {
    SecHandle *s = (SecHandle *)h;
    /* Zgornje besede NAMEN ne pišemo: nekateri poklicni sloji (npr. .NET
       DllImport z IntPtr) priskrbijo le eno besedo, mi pa ne vemo koliko. */
    s->dwLower = ((ULONG_PTR)TAG << (8 * sizeof(void *) - 16)) | (ULONG_PTR)(index + 1);
}
static int razcleni_rocaj(const void *h, BOOL *je_nas) {
    const SecHandle *s = (const SecHandle *)h;
    if (!s) { *je_nas = FALSE; return -1; }
    if ((s->dwLower >> (8 * sizeof(void *) - 16)) == (ULONG_PTR)TAG) {
        *je_nas = TRUE; return (int)(s->dwLower & 0xffff) - 1;
    }
    *je_nas = FALSE;
    return -1;
}

/* ------------------------------------------------------- javne funkcije ---- */

__declspec(dllexport) SECURITY_STATUS SEC_ENTRY AcquireCredentialsHandleW(
    SEC_WCHAR *pszPrincipal, SEC_WCHAR *pszPackage, ULONG fCredentialUse, void *pvLogonId,
    void *pAuthDataP, SEC_GET_KEY_FN pGetKeyFn, void *pArg, PCredHandle phCredential,
    PTimeStamp ptsExpiry)
{
    (void)pvLogonId;
    init_if();
    if (!pszPackage || !phCredential) {
        dnevnik("AcquireCredHandle", "nedolocen paket ali rocaj", SEC_E_INVALID_HANDLE);
        return SEC_E_INVALID_HANDLE;
    }
    char paket_ime[64];
    paket_v_ascii(pszPackage, paket_ime, sizeof paket_ime);
    if (!paket_nas(paket_ime)) {
        if (pravi._acquire) {
            SECURITY_STATUS r = ((SECURITY_STATUS (SEC_ENTRY *)(SEC_WCHAR*, SEC_WCHAR*, ULONG,
                void *, void *, SEC_GET_KEY_FN, void *, PCredHandle, PTimeStamp))pravi._acquire)(
                pszPrincipal, pszPackage, fCredentialUse, pvLogonId, pAuthDataP, pGetKeyFn,
                pArg, phCredential, ptsExpiry);
            if (r == SEC_E_OK) {
                int i = kred_dodaj();
                if (i >= 0) {
                    kred[i].je_pravi = TRUE;
                    kred[i].pravi = *phCredential;
                }
            }
            dnevnik("AcquireCredHandle(tuji)", paket_ime, r);
            return r;
        }
        dnevnik("AcquireCredHandle", paket_ime, SEC_E_SECPKG_NOT_FOUND);
        return SEC_E_SECPKG_NOT_FOUND;
    }
    int i = kred_dodaj();
    if (i < 0) return SEC_E_INSUFFICIENT_MEMORY;
    wf_cred *c = &kred[i];
    c->uporaba = fCredentialUse;
    MultiByteToWideChar(CP_UTF8, 0, paket_ime, -1, c->paket, 32);
    naša_identiteta(c->user, c->domain);
    if (pszPrincipal && pszPrincipal[0]) wcsncpy(c->user, pszPrincipal, MAXID - 1);
    naredi_rocaj(phCredential, i);
    if (ptsExpiry) memset(ptsExpiry, 0, sizeof(*ptsExpiry));
    char roca[64];
    opisi_mede("rocaj", phCredential, roca, sizeof roca);
    dnevnik("AcquireCredHandle", roca, SEC_E_OK);
    return SEC_E_OK;
}

__declspec(dllexport) SECURITY_STATUS SEC_ENTRY FreeCredentialsHandle(PCredHandle h)
{
    init_if();
    BOOL nas;
    int i = razcleni_rocaj(h, &nas);
    if (nas && i >= 0 && i < MAX_CRED) { memset(&kred[i], 0, sizeof kred[i]); return SEC_E_OK; }
    if (pravi.free_cred)
        return ((SECURITY_STATUS (SEC_ENTRY *)(PCredHandle))pravi.free_cred)(h);
    return SEC_E_INVALID_HANDLE;
}

/* Napolni izhodni pufer: če je aplikacija podala le velikost (pvBuffer = NULL),
   medpomnilnik priskrbimo mi in ga izdamo preko FreeContextBuffer. */
static SECURITY_STATUS oddaj_pufer(PSecBuffer b, const void *vir, size_t need) {
    if (!b) return SEC_E_INVALID_TOKEN;
    if (!b->pvBuffer) {
        b->pvBuffer = CoTaskMemAlloc(need);
        if (!b->pvBuffer) return SEC_E_INSUFFICIENT_MEMORY;
        b->cbBuffer = (ULONG)need;
    } else if (b->cbBuffer < (ULONG)need) {
        return SEC_E_INSUFFICIENT_MEMORY;
    } else {
        b->cbBuffer = (ULONG)need;
    }
    memcpy(b->pvBuffer, vir, need);
    b->BufferType = SECBUFFER_TOKEN;
    return SEC_E_OK;
}

static SECURITY_STATUS isc_jedro(
    PCredHandle phCredential, PCtxtHandle phContext, SEC_WCHAR *pszTargetName, ULONG fContextReq,
    ULONG Reserved1, ULONG TargetDataRep, PSecBufferDesc pInput, ULONG Reserved2,
    PCtxtHandle phNewContext, PSecBufferDesc pOutput, PULONG pfContextAttr, PTimeStamp ptsExpiry)
{
    init_if();
    char cilj[128], vhod[64], izpis_d[64], r1[64], r2[64], opis[300];
    siroko_v_utf8(pszTargetName, cilj, sizeof cilj);
    opisi_pisave("vhod", pInput, vhod, sizeof vhod);
    opisi_pisave("izpis", pOutput, izpis_d, sizeof izpis_d);
    opisi_mede("kred", phCredential, r1, sizeof r1);
    opisi_mede("ctx", phContext, r2, sizeof r2);
    snprintf(opis, sizeof opis, "%s %s %s %s cilj=%s", r1, r2, vhod, izpis_d, cilj);
    dnevnik("InitSecCtx", opis, (SECURITY_STATUS)0xDEAD0000);
    BOOL nas;
    int ci = razcleni_rocaj(phCredential, &nas);
    if (!nas) {
        if (pravi.isc)
            return ((SECURITY_STATUS (SEC_ENTRY *)(PCredHandle, PCtxtHandle, SEC_WCHAR*, ULONG,
                ULONG, ULONG, PSecBufferDesc, ULONG, PCtxtHandle, PSecBufferDesc, PULONG,
                PTimeStamp))pravi.isc)(phCredential, phContext, pszTargetName, fContextReq,
                Reserved1, TargetDataRep, pInput, Reserved2, phNewContext, pOutput,
                pfContextAttr, ptsExpiry);
        return SEC_E_INTERNAL_ERROR;
    }
    if (ci < 0 || ci >= MAX_CRED) return SEC_E_INVALID_HANDLE;
    wf_cred *kc = &kred[ci];

    int xi = -1;
    if (phContext) {
        BOOL n2; int j = razcleni_rocaj(phContext, &n2);
        if (n2) xi = j;
    }
    if (xi < 0) {
        xi = ctx_dodaj();
        if (xi < 0) return SEC_E_INSUFFICIENT_MEMORY;
        wf_ctx *c = &kontek[xi];
        wcsncpy(c->paket, kc->paket, 31);
        c->je_streznik = FALSE;
        nakljusje(c->moj_nonce, NONCE);
        if (!phNewContext) { kontek_izbaci(xi); return SEC_E_INVALID_HANDLE; }
        naredi_rocaj(phNewContext, xi);
    }
    wf_ctx *c = &kontek[xi];

    PSecBuffer izpis = NULL;
    if (pOutput) {
        for (ULONG i = 0; i < pOutput->cBuffers; i++)
            if (pOutput->pBuffers[i].BufferType == SECBUFFER_TOKEN
                || pOutput->pBuffers[i].BufferType == SECBUFFER_EMPTY) {
                izpis = &pOutput->pBuffers[i]; break;
            }
    }
    if (!izpis) return SEC_E_INVALID_TOKEN;

    wf_token t;
    memset(&t, 0, sizeof t);
    t.magic = MAGIC; t.ver = VER;

    if (!c->ustanovljen && !pInput) {
        /* korak 1: pogajanja */
        t.type = T_NEG;
        t.flags = 0x00000001;
        memcpy(t.nonce, c->moj_nonce, NONCE);
        izracun_mac(&t, FALSE, NULL);
        SECURITY_STATUS rr = oddaj_pufer(izpis, &t, sizeof t);
        if (rr != SEC_E_OK) { izpis->cbBuffer = 0; return rr; }
        if (pfContextAttr) *pfContextAttr = fContextReq;
        return SEC_I_CONTINUE_NEEDED;
    }

    if (!c->ustanovljen && pInput) {
        PSecBuffer v = NULL;
        for (ULONG i = 0; i < pInput->cBuffers; i++)
            if (pInput->pBuffers[i].cbBuffer >= sizeof(wf_token)) { v = &pInput->pBuffers[i]; break; }
        if (!v) return SEC_E_INVALID_TOKEN;
        wf_token prijeti;
        memcpy(&prijeti, v->pvBuffer, sizeof prijeti);
        if (prijeti.magic != MAGIC || prijeti.type != T_CHL) return SEC_E_INVALID_TOKEN;
        nalozi_secret();
        BOOL ok = FALSE;
        izracun_mac(&prijeti, TRUE, &ok);
        if (!ok) return SEC_E_MESSAGE_ALTERED;
        memcpy(c->tuj_nonce, prijeti.nonce, NONCE);
        wcsncpy(c->peer_user, prijeti.user, MAXID - 1);
        wcsncpy(c->peer_domain, prijeti.domain, MAXID - 1);
        izpelji_sejni_klju(c);
        /* korak 3: avtentikacija */
        t.type = T_AUTH;
        lstrcpynW(t.user, kc->user, MAXID);
        lstrcpynW(t.domain, kc->domain, MAXID);
        memcpy(t.nonce, c->moj_nonce, NONCE);
        izracun_mac(&t, FALSE, NULL);
        if (izpis->cbBuffer < sizeof t) return SEC_E_INSUFFICIENT_MEMORY;
        SECURITY_STATUS rr2 = oddaj_pufer(izpis, &t, sizeof t);
        if (rr2 != SEC_E_OK) { izpis->cbBuffer = 0; return rr2; }
        c->ustanovljen = TRUE;
        if (pfContextAttr) *pfContextAttr = fContextReq;
        return SEC_E_OK;
    }
    return SEC_E_INVALID_TOKEN;
}

static SECURITY_STATUS accept_jedro(
    PCredHandle phCredential, PCtxtHandle phContext, PSecBufferDesc pInput, ULONG fContextReq,
    ULONG TargetDataRep, PCtxtHandle phNewContext, PSecBufferDesc pOutput, PULONG pfContextAttr,
    PTimeStamp ptsExpiry)
{
    init_if();
    char vhod[64], izpis_d[64], r1[64], r2[64], opis[220];
    opisi_pisave("vhod", pInput, vhod, sizeof vhod);
    opisi_pisave("izpis", pOutput, izpis_d, sizeof izpis_d);
    opisi_mede("kred", phCredential, r1, sizeof r1);
    opisi_mede("ctx", phContext, r2, sizeof r2);
    snprintf(opis, sizeof opis, "%s %s %s %s", r1, r2, vhod, izpis_d);
    dnevnik("AcceptSecCtx", opis, (SECURITY_STATUS)0xDEAD0000);
    BOOL nas;
    int ci = razcleni_rocaj(phCredential, &nas);
    if (!nas) {
        if (pravi.accept)
            return ((SECURITY_STATUS (SEC_ENTRY *)(PCredHandle, PCtxtHandle, PSecBufferDesc,
                ULONG, ULONG, PCtxtHandle, PSecBufferDesc, PULONG, PTimeStamp))pravi.accept)(
                phCredential, phContext, pInput, fContextReq, TargetDataRep, phNewContext,
                pOutput, pfContextAttr, ptsExpiry);
        return SEC_E_INTERNAL_ERROR;
    }
    if (ci < 0 || ci >= MAX_CRED) return SEC_E_INVALID_HANDLE;

    int xi = -1;
    if (phContext) { BOOL n2; int j = razcleni_rocaj(phContext, &n2); if (n2) xi = j; }
    if (xi < 0) {
        xi = ctx_dodaj();
        if (xi < 0) return SEC_E_INSUFFICIENT_MEMORY;
        if (!phNewContext) { kontek_izbaci(xi); return SEC_E_INVALID_HANDLE; }
        naredi_rocaj(phNewContext, xi);
        wf_ctx *c = &kontek[xi];
        c->je_streznik = TRUE;
        wcsncpy(c->paket, kred[ci].paket, 31);
        nakljusje(c->moj_nonce, NONCE);
    }
    wf_ctx *c = &kontek[xi];
    if (!pInput) return SEC_E_INVALID_TOKEN;
    PSecBuffer v = NULL;
    for (ULONG i = 0; i < pInput->cBuffers; i++)
        if (pInput->pBuffers[i].cbBuffer >= sizeof(wf_token)) { v = &pInput->pBuffers[i]; break; }
    if (!v) return SEC_E_INVALID_TOKEN;

    wf_token prijeti;
    memcpy(&prijeti, v->pvBuffer, sizeof prijeti);
    if (prijeti.magic != MAGIC) return SEC_E_INVALID_TOKEN;
    nalozi_secret();
    BOOL ok = FALSE;
    izracun_mac(&prijeti, TRUE, &ok);
    if (!ok) return SEC_E_MESSAGE_ALTERED;

    PSecBuffer izpis = NULL;
    if (pOutput)
        for (ULONG i = 0; i < pOutput->cBuffers; i++)
            if (pOutput->pBuffers[i].BufferType == SECBUFFER_TOKEN
                || pOutput->pBuffers[i].BufferType == SECBUFFER_EMPTY) {
                izpis = &pOutput->pBuffers[i]; break;
            }

    if (prijeti.type == T_NEG) {
        wf_token odgovor;
        memset(&odgovor, 0, sizeof odgovor);
        odgovor.magic = MAGIC; odgovor.ver = VER; odgovor.type = T_CHL;
        naša_identiteta(odgovor.user, odgovor.domain);
        memcpy(odgovor.nonce, c->moj_nonce, NONCE);
        izracun_mac(&odgovor, FALSE, NULL);
        memcpy(c->tuj_nonce, prijeti.nonce, NONCE);
        wcsncpy(c->peer_user, prijeti.user, MAXID - 1);
        wcsncpy(c->peer_domain, prijeti.domain, MAXID - 1);
        izpelji_sejni_klju(c);
        if (!izpis) return SEC_E_INVALID_TOKEN;
        SECURITY_STATUS rr = oddaj_pufer(izpis, &odgovor, sizeof odgovor);
        if (rr != SEC_E_OK) { izpis->cbBuffer = 0; return rr; }
        if (pfContextAttr) *pfContextAttr = fContextReq;
        return SEC_I_CONTINUE_NEEDED;
    }
    if (prijeti.type == T_AUTH) {
        memcpy(c->tuj_nonce, prijeti.nonce, NONCE);
        wcsncpy(c->peer_user, prijeti.user, MAXID - 1);
        wcsncpy(c->peer_domain, prijeti.domain, MAXID - 1);
        izpelji_sejni_klju(c);
        c->ustanovljen = TRUE;
        if (izpis) izpis->cbBuffer = 0;
        if (pfContextAttr) *pfContextAttr = fContextReq;
        return SEC_E_OK;
    }
    return SEC_E_INVALID_TOKEN;
}

__declspec(dllexport) SECURITY_STATUS SEC_ENTRY CompleteAuthToken(PCtxtHandle h, PSecBufferDesc p)
{
    init_if();
    BOOL nas; int i = razcleni_rocaj(h, &nas);
    if (nas) return SEC_E_OK;
    if (pravi.complete)
        return ((SECURITY_STATUS (SEC_ENTRY *)(PCtxtHandle, PSecBufferDesc))pravi.complete)(h, p);
    return SEC_E_INVALID_HANDLE;
}

__declspec(dllexport) SECURITY_STATUS SEC_ENTRY DeleteSecurityContext(PCtxtHandle h)
{
    init_if();
    BOOL nas; int i = razcleni_rocaj(h, &nas);
    if (nas && i >= 0 && i < MAX_CTX) { memset(&kontek[i], 0, sizeof kontek[i]); return SEC_E_OK; }
    if (pravi.delete_ctx)
        return ((SECURITY_STATUS (SEC_ENTRY *)(PCtxtHandle))pravi.delete_ctx)(h);
    return SEC_E_INVALID_HANDLE;
}

__declspec(dllexport) SECURITY_STATUS SEC_ENTRY QueryContextAttributesW(PCtxtHandle h, ULONG ulAttr, LPVOID out)
{
    init_if();
    BOOL nas; int i = razcleni_rocaj(h, &nas);
    if (!nas) {
        if (pravi.qca)
            return ((SECURITY_STATUS (SEC_ENTRY *)(PCtxtHandle, ULONG, LPVOID))pravi.qca)(
                h, ulAttr, out);
        return SEC_E_INTERNAL_ERROR;
    }
    if (i < 0 || i >= MAX_CTX) return SEC_E_INVALID_HANDLE;
    wf_ctx *c = &kontek[i];
    switch (ulAttr) {
    case SECPKG_ATTR_SIZES: {
        SecPkgContext_Sizes *s = (SecPkgContext_Sizes *)out;
        s->cbMaxToken = 1904; s->cbMaxSignature = 1904; s->cbBlockSize = 16; s->cbSecurityTrailer = 64;
        return SEC_E_OK; }
    case SECPKG_ATTR_NAMES: {
        SecPkgContext_NamesW *n = (SecPkgContext_NamesW *)out;
        size_t d = wcslen(c->peer_user) + wcslen(c->peer_domain) + 2;
        n->sUserName = (LPWSTR)CoTaskMemAlloc(d * sizeof(WCHAR));
        if (!n->sUserName) return SEC_E_INSUFFICIENT_MEMORY;
        wsprintfW(n->sUserName, L"%s\\%s", c->peer_domain, c->peer_user);
        return SEC_E_OK; }
    case SECPKG_ATTR_NATIVE_NAMES: {
        SecPkgContext_NativeNamesW *n = (SecPkgContext_NativeNamesW *)out;
        wchar_t jaz[MAXID], dom[MAXID];
        nas_ident(jaz, dom);
        n->sClientName = (LPWSTR)CoTaskMemAlloc(MAXID * 2);
        n->sServerName = (LPWSTR)CoTaskMemAlloc(MAXID * 2);
        if (!c->je_streznik) {
            lstrcpynW(n->sClientName, jaz, MAXID);
            lstrcpynW(n->sServerName, c->peer_user, MAXID);
        } else {
            lstrcpynW(n->sClientName, c->peer_user, MAXID);
            lstrcpynW(n->sServerName, jaz, MAXID);
        }
        return SEC_E_OK; }
    case SECPKG_ATTR_SESSION_KEY: {
        wf_sski *k = (wf_sski *)out;
        k->SessionKey = (PUCHAR)CoTaskMemAlloc(32);
        if (!k->SessionKey) return SEC_E_INSUFFICIENT_MEMORY;
        memcpy(k->SessionKey, c->sea_key, 32);
        k->SessionKeyLength = 32;
        return SEC_E_OK; }
    case SECPKG_ATTR_AUTHORITY: {
        wf_avtorW *a = (wf_avtorW *)out;
        a->Authority = (LPWSTR)CoTaskMemAlloc(MAXID * 2);
        lstrcpynW(a->Authority, c->peer_domain, MAXID);
        return SEC_E_OK; }
    case SECPKG_ATTR_AUTH_ID:
        return SEC_E_OK;
    default:
        return SEC_E_UNSUPPORTED_FUNCTION;
    }
}

__declspec(dllexport) SECURITY_STATUS SEC_ENTRY EncryptMessage(PCtxtHandle h, ULONG qop, PSecBufferDesc p, ULONG seq)
{
    init_if();
    BOOL nas; int i = razcleni_rocaj(h, &nas);
    if (!nas) {
        if (pravi.enc)
            return ((SECURITY_STATUS (SEC_ENTRY *)(PCtxtHandle, ULONG, PSecBufferDesc, ULONG))
                    pravi.enc)(h, qop, p, seq);
        return SEC_E_INTERNAL_ERROR;
    }
    if (i < 0 || i >= MAX_CTX || !kontek[i].ustanovljen) return SEC_E_INVALID_HANDLE;
    wf_ctx *c = &kontek[i];
    PSecBuffer podatki = NULL, znamka = NULL;
    for (ULONG k = 0; k < p->cBuffers; k++) {
        if (p->pBuffers[k].BufferType == SECBUFFER_DATA && !podatki) podatki = &p->pBuffers[k];
        else if ((p->pBuffers[k].BufferType & 0x0fffffff) == SECBUFFER_TOKEN
                 && !znamka) znamka = &p->pBuffers[k];
    }
    if (!podatki) return SEC_E_INVALID_TOKEN;
    prekrij(c->sea_key, c->stevec_posiljanja++, (unsigned char *)podatki->pvBuffer,
            podatki->cbBuffer);
    if (znamka) {
        unsigned char mac[MAC];
        hmac_sha256(c->sea_key, 32, (const unsigned char *)podatki->pvBuffer,
                    podatki->cbBuffer, mac);
        if (znamka->cbBuffer >= 16) { memcpy(znamka->pvBuffer, mac, 16); znamka->cbBuffer = 16; }
        else znamka->cbBuffer = 0;
    }
    return SEC_E_OK;
}

__declspec(dllexport) SECURITY_STATUS SEC_ENTRY DecryptMessage(PCtxtHandle h, PSecBufferDesc p, ULONG seq, PULONG qop)
{
    init_if();
    BOOL nas; int i = razcleni_rocaj(h, &nas);
    if (!nas) {
        if (pravi.dec)
            return ((SECURITY_STATUS (SEC_ENTRY *)(PCtxtHandle, PSecBufferDesc, ULONG, PULONG))
                    pravi.dec)(h, p, seq, qop);
        return SEC_E_INTERNAL_ERROR;
    }
    if (i < 0 || i >= MAX_CTX || !kontek[i].ustanovljen) return SEC_E_INVALID_HANDLE;
    wf_ctx *c = &kontek[i];
    PSecBuffer podatki = NULL;
    for (ULONG k = 0; k < p->cBuffers; k++)
        if (p->pBuffers[k].BufferType == SECBUFFER_DATA) { podatki = &p->pBuffers[k]; break; }
    if (!podatki) return SEC_E_INVALID_TOKEN;
    prekrij(c->sea_key, c->stevec_prejemanja++, (unsigned char *)podatki->pvBuffer,
            podatki->cbBuffer);
    if (qop) *qop = SECQOP_WRAP_NO_ENCRYPT;
    return SEC_E_OK;
}

__declspec(dllexport) SECURITY_STATUS SEC_ENTRY MakeSignature(PCtxtHandle h, ULONG qop, PSecBufferDesc p, ULONG seq)
{ return EncryptMessage(h, qop, p, seq); }
__declspec(dllexport) SECURITY_STATUS SEC_ENTRY VerifySignature(PCtxtHandle h, PSecBufferDesc p, ULONG seq, PULONG qop)
{ return DecryptMessage(h, p, seq, qop); }

__declspec(dllexport) SECURITY_STATUS SEC_ENTRY QuerySecurityContextToken(PCtxtHandle h, PHANDLE tok)
{
    init_if();
    BOOL nas; int i = razcleni_rocaj(h, &nas);
    if (!nas) {
        if (pravi.token)
            return ((SECURITY_STATUS (SEC_ENTRY *)(PCtxtHandle, PHANDLE))pravi.token)(h, tok);
        return SEC_E_INTERNAL_ERROR;
    }
    if (i < 0 || i >= MAX_CTX) return SEC_E_INVALID_HANDLE;
    HANDLE me = TOKEN_PSEVDOK;   /* psevdoročaj žetona trenutnega procesa */
    if (!DuplicateHandle(GetCurrentProcess(), me, GetCurrentProcess(), tok, 0, FALSE,
                         DUPLICATE_SAME_ACCESS))
        return SEC_E_INTERNAL_ERROR;
    return SEC_E_OK;
}

__declspec(dllexport) SECURITY_STATUS SEC_ENTRY FreeContextBuffer(PVOID p)
{
    init_if();
    if (!p) return SEC_E_INVALID_PARAMETER;
    /* naši prenosi so narejeni s CoTaskMemAlloc */
    CoTaskMemFree(p);
    return SEC_E_OK;
}

__declspec(dllexport) SECURITY_STATUS SEC_ENTRY EnumerateSecurityPackagesW(PULONG pcCount, PSecPkgInfoW *ppInfo)
{
    init_if();
    ULONG nasi = STEV_PAKETOV, pravih = 0;
    SecPkgInfoW *pr = NULL;
    if (pravi.esp) {
        ULONG n2 = 0;
        if (((SECURITY_STATUS (SEC_ENTRY *)(PULONG, PSecPkgInfoW *))pravi.esp)(&n2, &pr) == SEC_E_OK)
            pravih = n2;
    }
    ULONG v = nasi + pravih;
    SecPkgInfoW *t = (SecPkgInfoW *)CoTaskMemAlloc(v * sizeof(SecPkgInfoW));
    if (!t) return SEC_E_INSUFFICIENT_MEMORY;
    memset(t, 0, v * sizeof(SecPkgInfoW));
    for (ULONG i = 0; i < nasi; i++) nasi_paket(&PAKETI[i], &t[i]);
    for (ULONG i = 0; i < pravih; i++) t[nasi + i] = pr[i];
    *pcCount = v;
    *ppInfo = t;
    return SEC_E_OK;
}

__declspec(dllexport) SECURITY_STATUS SEC_ENTRY QuerySecurityPackageInfoW(SEC_WCHAR *ime, PSecPkgInfoW *pb)
{
    init_if();
    char imen[64];
    paket_v_ascii(ime, imen, sizeof imen);
    for (size_t i = 0; i < STEV_PAKETOV; i++)
        if (_stricmp(imen, PAKETI[i].ime) == 0) {
            SecPkgInfoW *t = (SecPkgInfoW *)CoTaskMemAlloc(sizeof(SecPkgInfoW));
            if (!t) return SEC_E_INSUFFICIENT_MEMORY;
            if (!nasi_paket(&PAKETI[i], t)) { CoTaskMemFree(t); return SEC_E_INSUFFICIENT_MEMORY; }
            *pb = t;
            return SEC_E_OK;
        }
    if (pravi.qcpa)
        return ((SECURITY_STATUS (SEC_ENTRY *)(SEC_CHAR*, PSecPkgInfoW*))pravi.qcpa)(imen, pb);
    return SEC_E_SECPKG_NOT_FOUND;
}

__declspec(dllexport) SECURITY_STATUS SEC_ENTRY QueryCredentialsAttributesW(PCredHandle h, ULONG a, LPVOID v)
{ return SEC_E_UNSUPPORTED_FUNCTION; }

__declspec(dllexport) __declspec(dllexport) SECURITY_STATUS SEC_ENTRY AddSecurityPackageW(LPWSTR p,
                                                                    PSECURITY_PACKAGE_OPTIONS o)
{ (void)p; (void)o; return SEC_E_OK; }
__declspec(dllexport) SECURITY_STATUS SEC_ENTRY DeleteSecurityPackageW(LPWSTR p)
{ (void)p; return SEC_E_OK; }

__declspec(dllexport) BOOLEAN SEC_ENTRY GetUserNameExW(EXTENDED_NAME_FORMAT forma, LPWSTR ime, PULONG dolzina)
{
    wchar_t u[MAXID], d[MAXID];
    naša_identiteta(u, d);
    wchar_t z[512];
    switch (forma) {
    case NameSamCompatible: wsprintfW(z, L"%s\\%s", d, u); break;
    case NameFullyQualifiedDN: wsprintfW(z, L"CN=%s", u); break;
    default: lstrcpynW(z, u, 500); break;
    }
    ULONG r = (ULONG)wcslen(z);
    if (!ime || *dolzina <= r) { *dolzina = r + 1; return FALSE; }
    lstrcpynW(ime, z, r + 1);
    *dolzina = r;
    return TRUE;
}

__declspec(dllexport) BOOLEAN SEC_ENTRY GetUserNameExA(EXTENDED_NAME_FORMAT forma, LPSTR ime, PULONG dolzina)
{
    wchar_t si[512]; ULONG d = 512;
    if (!GetUserNameExW(forma, si, &d)) return FALSE;
    int n = WideCharToMultiByte(CP_ACP, 0, si, -1, NULL, 0, NULL, NULL);
    if ((ULONG)n > *dolzina) { *dolzina = n; return FALSE; }
    WideCharToMultiByte(CP_ACP, 0, si, -1, ime, n, NULL, NULL);
    *dolzina = n - 1;
    return TRUE;
}

__declspec(dllexport) SECURITY_STATUS SEC_ENTRY ImpersonateSecurityContext(PCtxtHandle h)
{ return SEC_E_OK; }
__declspec(dllexport) SECURITY_STATUS SEC_ENTRY RevertSecurityContext(PCtxtHandle h) { return SEC_E_OK; }

__declspec(dllexport) SECURITY_STATUS SEC_ENTRY SetContextAttributesW(PCtxtHandle h, ULONG a, void *v, ULONG cb)
{ (void)h; (void)a; (void)v; (void)cb; return SEC_E_OK; }

__declspec(dllexport) SECURITY_STATUS SEC_ENTRY ExportSecurityContext(PCtxtHandle h, ULONG f, PSecBuffer b, void **m)
{ (void)h; (void)f; (void)b; (void)m; return SEC_E_UNSUPPORTED_FUNCTION; }
__declspec(dllexport) SECURITY_STATUS SEC_ENTRY ImportSecurityContextA(SEC_CHAR *p, PSecBuffer b,
                                                                       void *t, PCtxtHandle h)
{ (void)p; (void)b; (void)t; (void)h; return SEC_E_UNSUPPORTED_FUNCTION; }
__declspec(dllexport) SECURITY_STATUS SEC_ENTRY ImportSecurityContextW(SEC_WCHAR *p, PSecBuffer b,
                                                                       void *t, PCtxtHandle h)
{ (void)p; (void)b; (void)t; (void)h; return SEC_E_UNSUPPORTED_FUNCTION; }
__declspec(dllexport) SECURITY_STATUS SEC_ENTRY ApplyControlToken(PCtxtHandle h, PSecBufferDesc p)
{ return SEC_E_UNSUPPORTED_FUNCTION; }
__declspec(dllexport) SECURITY_STATUS SEC_ENTRY ChangeAccountPasswordW(SEC_WCHAR *p, SEC_WCHAR *d,
                                                 SEC_WCHAR *u, SEC_WCHAR *o, SEC_WCHAR *n,
                                                 BOOLEAN imp, ULONG rez, PSecBufferDesc izp)
{ (void)p; (void)d; (void)u; (void)o; (void)n; (void)imp; (void)rez; (void)izp;
  return SEC_E_UNSUPPORTED_FUNCTION; }

__declspec(dllexport) SECURITY_STATUS SEC_ENTRY SetNamedSecurityProfileW(PVOID profil, PVOID atributi, ULONG stevilo)
{ return SEC_E_OK; }

/* ---------------------------------------------------- A predelave (8-bit) -- */

static void w2a(const wchar_t *s, char *b, size_t n) {
    if (!s) { b[0] = 0; return; }
    WideCharToMultiByte(CP_ACP, 0, s, -1, b, (int)n, NULL, NULL);
}

__declspec(dllexport) SECURITY_STATUS SEC_ENTRY AcquireCredentialsHandleA(SEC_CHAR *p, SEC_CHAR *pak, ULONG uporaba,
    void *l, void *ad, SEC_GET_KEY_FN gk, void *arg, PCredHandle h, PTimeStamp e)
{ (void)p; return AcquireCredentialsHandleW(NULL, (SEC_WCHAR *)pak, uporaba, l, ad, gk, arg, h, e); }

typedef struct { wchar_t w[512]; } most;

static void a2w(const char *s, wchar_t *d, size_t cap) {
    if (!s) { d[0] = 0; return; }
    MultiByteToWideChar(CP_ACP, 0, s, -1, d, (int)cap);
}

__declspec(dllexport) SECURITY_STATUS SEC_ENTRY InitializeSecurityContextA(
    PCredHandle pc, PCtxtHandle pcx, SEC_CHAR *cilj, ULONG req, ULONG r1, ULONG rep,
    PSecBufferDesc vstop, ULONG r2, PCtxtHandle nov, PSecBufferDesc izstop, PULONG attr,
    PTimeStamp exp)
{
    wchar_t si[512];
    a2w(cilj, si, 512);
    return InitializeSecurityContextW(pc, pcx, si, req, r1, rep, vstop, r2, nov, izstop,
                                         attr, exp);
}

__declspec(dllexport) SECURITY_STATUS SEC_ENTRY EnumerateSecurityPackagesA(PULONG stev, PSecPkgInfoA *pb)
{
    SecPkgInfoW *si = NULL; ULONG n = 0;
    SECURITY_STATUS r = EnumerateSecurityPackagesW(&n, &si);
    if (r != SEC_E_OK) return r;
    SecPkgInfoA *sa = (SecPkgInfoA *)CoTaskMemAlloc(n * sizeof(SecPkgInfoA));
    if (!sa) return SEC_E_INSUFFICIENT_MEMORY;
    for (ULONG i = 0; i < n; i++) {
        sa[i] = *(SecPkgInfoA *)&si[i];
        int d1 = WideCharToMultiByte(CP_UTF8, 0, si[i].Name, -1, NULL, 0, NULL, NULL);
        int d2 = WideCharToMultiByte(CP_UTF8, 0, si[i].Comment, -1, NULL, 0, NULL, NULL);
        sa[i].Name = (LPSTR)CoTaskMemAlloc(d1);
        sa[i].Comment = (LPSTR)CoTaskMemAlloc(d2);
        WideCharToMultiByte(CP_UTF8, 0, si[i].Name, -1, sa[i].Name, d1, NULL, NULL);
        WideCharToMultiByte(CP_UTF8, 0, si[i].Comment, -1, sa[i].Comment, d2, NULL, NULL);
    }
    *stev = n;
    *pb = sa;
    return SEC_E_OK;
}

__declspec(dllexport) SECURITY_STATUS SEC_ENTRY QuerySecurityPackageInfoA(SEC_CHAR *ime, PSecPkgInfoA *pb)
{
    SecPkgInfoW *si = NULL;
    SECURITY_STATUS r = QuerySecurityPackageInfoW((SEC_WCHAR *)ime, &si);
    if (r != SEC_E_OK) return r;
    SecPkgInfoA *sa = (SecPkgInfoA *)CoTaskMemAlloc(sizeof(SecPkgInfoA));
    *sa = *(SecPkgInfoA *)si;
    int d1 = WideCharToMultiByte(CP_UTF8, 0, si->Name, -1, NULL, 0, NULL, NULL);
    int d2 = WideCharToMultiByte(CP_UTF8, 0, si->Comment, -1, NULL, 0, NULL, NULL);
    sa->Name = (LPSTR)CoTaskMemAlloc(d1);
    sa->Comment = (LPSTR)CoTaskMemAlloc(d2);
    WideCharToMultiByte(CP_UTF8, 0, si->Name, -1, sa->Name, d1, NULL, NULL);
    WideCharToMultiByte(CP_UTF8, 0, si->Comment, -1, sa->Comment, d2, NULL, NULL);
    *pb = sa;
    return SEC_E_OK;
}

__declspec(dllexport) SECURITY_STATUS SEC_ENTRY QueryContextAttributesA(PCtxtHandle h, ULONG a, LPVOID v)
{
    if (a == SECPKG_ATTR_NAMES) {
        SecPkgContext_NamesW si;
        SECURITY_STATUS r = QueryContextAttributesW(h, a, &si);
        if (r == SEC_E_OK) {
            int d = WideCharToMultiByte(CP_ACP, 0, si.sUserName, -1, NULL, 0, NULL, NULL);
            ((SecPkgContext_NamesA *)v)->sUserName = (LPSTR)CoTaskMemAlloc(d);
            WideCharToMultiByte(CP_ACP, 0, si.sUserName, -1,
                                ((SecPkgContext_NamesA *)v)->sUserName, d, NULL, NULL);
        }
        return r;
    }
    return QueryContextAttributesW(h, a, v);
}

__declspec(dllexport) __declspec(dllexport) SECURITY_STATUS SEC_ENTRY AddSecurityPackageA(LPSTR p,
                                                                    PSECURITY_PACKAGE_OPTIONS o)
{ (void)p; (void)o; return SEC_E_OK; }
__declspec(dllexport) SECURITY_STATUS SEC_ENTRY DeleteSecurityPackageA(LPSTR p)
{ (void)p; return SEC_E_OK; }
__declspec(dllexport) SECURITY_STATUS SEC_ENTRY QueryCredentialsAttributesA(PCredHandle h, ULONG a, LPVOID v)
{ return SEC_E_UNSUPPORTED_FUNCTION; }
__declspec(dllexport) SECURITY_STATUS SEC_ENTRY ChangeAccountPasswordA(SEC_CHAR *p, SEC_CHAR *d,
                                                 SEC_CHAR *u, SEC_CHAR *o, SEC_CHAR *n,
                                                 BOOLEAN imp, ULONG rez, PSecBufferDesc izp)
{ (void)p; (void)d; (void)u; (void)o; (void)n; (void)imp; (void)rez; (void)izp;
  return SEC_E_UNSUPPORTED_FUNCTION; }

static BOOL WINAPI ulaz(HMODULE m, DWORD r, LPVOID v) {
    if (r == DLL_PROCESS_ATTACH) { init_if(); DisableThreadLibraryCalls(m); }
    return TRUE;
}

/* Ovojnici, ki zapišeta končni rezultat (lažje razhroščevanje). */
__declspec(dllexport) SECURITY_STATUS SEC_ENTRY InitializeSecurityContextW(
    PCredHandle phCredential, PCtxtHandle phContext, SEC_WCHAR *pszTargetName, ULONG fContextReq,
    ULONG Reserved1, ULONG TargetDataRep, PSecBufferDesc pInput, ULONG Reserved2,
    PCtxtHandle phNewContext, PSecBufferDesc pOutput, PULONG pfContextAttr, PTimeStamp ptsExpiry)
{
    SECURITY_STATUS r = isc_jedro(phCredential, phContext, pszTargetName, fContextReq, Reserved1,
                                  TargetDataRep, pInput, Reserved2, phNewContext, pOutput,
                                  pfContextAttr, ptsExpiry);
    dnevnik("  = rezultat InitSecCtx", "", r);
    return r;
}

__declspec(dllexport) SECURITY_STATUS SEC_ENTRY AcceptSecurityContext(
    PCredHandle phCredential, PCtxtHandle phContext, PSecBufferDesc pInput, ULONG fContextReq,
    ULONG TargetDataRep, PCtxtHandle phNewContext, PSecBufferDesc pOutput, PULONG pfContextAttr,
    PTimeStamp ptsExpiry)
{
    SECURITY_STATUS r = accept_jedro(phCredential, phContext, pInput, fContextReq, TargetDataRep,
                                     phNewContext, pOutput, pfContextAttr, ptsExpiry);
    dnevnik("  = rezultat AcceptSecCtx", "", r);
    return r;
}
