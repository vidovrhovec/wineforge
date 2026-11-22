/* Preizkus nadomestnega SSPI modula: celoten rokovalni protokol klient<->storitev,
   sejni kljuc, sifriranje.  Prevajamo z llvm-mingw, zazene se pod Wineom. */
#define SECURITY_WIN32
#include <windows.h>
#include <security.h>
#include <stdio.h>
#include <string.h>

static SecBufferDesc Bufferi(SecBuffer *b, ULONG n) {
    SecBufferDesc d; d.ulVersion = SECBUFFER_VERSION; d.cBuffers = n; d.pBuffers = b;
    return d;
}

static ULONG stekel_cep = 0;

int main(void) {
    ULONG stev = 0; SecPkgInfoW *pak = NULL;
    SECURITY_STATUS r = EnumerateSecurityPackagesW(&stev, &pak);
    printf("EnumerateSecurityPackagesW = 0x%08x  stevilo=%lu\n", r, stev);
    for (ULONG i = 0; i < stev && pak; i++) printf("   %2lu. %S  (rpc %u, max %lu)\n",
                                                   i + 1, pak[i].Name, pak[i].wRPCID,
                                                   pak[i].cbMaxToken);
    if (pak) FreeContextBuffer(pak);

    CredHandle kuca, kstreznik, pusto;
    TimeStamp cas;
    r = AcquireCredentialsHandleW(NULL, L"NTLM", SECPKG_CRED_OUTBOUND, NULL, NULL, NULL, NULL,
                                  &kuca, &cas);
    printf("AcquireCredentialsHandle(klient, NTLM) = 0x%08x\n", r);
    if (r) return 1;
    r = AcquireCredentialsHandleW(NULL, L"NTLM", SECPKG_CRED_INBOUND, NULL, NULL, NULL, NULL,
                                  &kstreznik, &cas);
    printf("AcquireCredentialsHandle(streznik, NTLM) = 0x%08x\n", r);
    if (r) return 1;

    unsigned char t1[4096], t2[4096], t3[4096];
    SecBuffer b1 = { sizeof t1, SECBUFFER_TOKEN, t1 };
    SecBufferDesc v1 = Bufferi(&b1, 1);
    CtxtHandle kc = {0}, ks = {0}, knov;

    r = InitializeSecurityContextW(&kuca, NULL, L"localhost", ISC_REQ_CONFIDENTIALITY, 0, 0,
                                   NULL, 0, &kc, &v1, &stekel_cep, &cas);
    printf("1) InitializeSecurityContext (pogajanja) = 0x%08x  token=%lu B\n", r, b1.cbBuffer);
    if (r != SEC_I_CONTINUE_NEEDED) return 2;

    SecBuffer b2 = { sizeof t2, SECBUFFER_TOKEN, t2 };
    SecBufferDesc v2 = Bufferi(&b2, 1);
    SecBuffer vstop1 = { b1.cbBuffer, SECBUFFER_TOKEN, b1.pvBuffer };
    SecBufferDesc ul1 = Bufferi(&vstop1, 1);
    r = AcceptSecurityContext(&kstreznik, NULL, &ul1, ISC_REQ_CONFIDENTIALITY, 0,
                              &ks, &v2, &stekel_cep, &cas);
    printf("2) AcceptSecurityContext (izziv)        = 0x%08x  token=%lu B\n", r, b2.cbBuffer);
    if (r != SEC_I_CONTINUE_NEEDED) return 3;

    SecBuffer b3 = { sizeof t3, SECBUFFER_TOKEN, t3 };
    SecBufferDesc v3 = Bufferi(&b3, 1);
    SecBuffer vstop2 = { b2.cbBuffer, SECBUFFER_TOKEN, b2.pvBuffer };
    SecBufferDesc ul2 = Bufferi(&vstop2, 1);
    r = InitializeSecurityContextW(&kuca, &kc, L"localhost", ISC_REQ_CONFIDENTIALITY, 0, 0,
                                   &ul2, 0, &kc, &v3, &stekel_cep, &cas);
    printf("3) InitializeSecurityContext (avtent.)  = 0x%08x  token=%lu B\n", r, b3.cbBuffer);
    if (r != SEC_E_OK && r != SEC_I_CONTINUE_NEEDED) return 4;

    SecBuffer vstop3 = { b3.cbBuffer, SECBUFFER_TOKEN, b3.pvBuffer };
    SecBufferDesc ul3 = Bufferi(&vstop3, 1);
    SecBuffer prazno = { 0, SECBUFFER_TOKEN, NULL };
    SecBufferDesc izpr = Bufferi(&prazno, 1);
    r = AcceptSecurityContext(&kstreznik, &ks, &ul3, ISC_REQ_CONFIDENTIALITY, 0,
                              &ks, &izpr, &stekel_cep, &cas);
    printf("4) AcceptSecurityContext (potrditev)    = 0x%08x  %s\n", r,
           r == SEC_E_OK ? "POSTAVLJENO" : "napaka");
    if (r != SEC_E_OK) return 5;

    /* kdo je prijaven na drugi strani */
    SecPkgContext_NamesW imena;
    r = QueryContextAttributesW(&ks, SECPKG_ATTR_NAMES, &imena);
    printf("QueryContextAttributes(NAMES, streznik) = 0x%08x  -> %S\n", r,
           r == SEC_E_OK ? imena.sUserName : L"(nil)");

    /* sejna kljuca se morata ujemati */
    struct { ULONG dolz; PUCHAR kljuc; } k1, k2;
    r = QueryContextAttributesW(&kc, SECPKG_ATTR_SESSION_KEY, &k1);
    int r2 = QueryContextAttributesW(&ks, SECPKG_ATTR_SESSION_KEY, &k2);
    if (r == SEC_E_OK && r2 == SEC_E_OK)
        printf("sejna kljuca: %lu B, enaka=%s\n", k1.dolz,
               memcmp(k1.kljuc, k2.kljuc, k1.dolz) == 0 ? "DA" : "NE");

    /* sifriranje prek cevi (SIMULIRANO): klient sifrirja, streznik despifrirja */
    char geslo[64] = "Pozdravljen, StarDesk!";
    unsigned char poslat[64];
    strcpy((char *)poslat, geslo);
    SecBuffer od = { 16, SECBUFFER_TOKEN, t1 };
    SecBuffer od2 = { (ULONG)strlen(poslat), SECBUFFER_DATA, poslat };
    SecBuffer zasta[2] = { od, od2 };
    SecBufferDesc sifrir = Bufferi(zasta, 2);
    r = EncryptMessage(&kc, 0, &sifrir, 1);
    printf("EncryptMessage = 0x%08x  (vsebina po sifriranju: %s)\n", r,
           memcmp(poslat, geslo, strlen(geslo)) ? "spremenjena OK" : "ENAKA - napaka");

    unsigned char nazaj[64];
    memcpy(nazaj, poslat, zasta[1].cbBuffer);
    SecBuffer pri1 = { zasta[0].cbBuffer, SECBUFFER_TOKEN, t1 };
    SecBuffer pri2 = { zasta[1].cbBuffer, SECBUFFER_DATA, nazaj };
    SecBuffer sprej[2] = { pri1, pri2 };
    SecBufferDesc desifrir = Bufferi(sprej, 2);
    ULONG qop = 0;
    r2 = DecryptMessage(&ks, &desifrir, 1, &qop);
    nazaj[sprej[1].cbBuffer] = 0;
    printf("DecryptMessage = 0x%08x  -> \"%s\"  %s\n", r2, (char *)nazaj,
           strcmp((char *)nazaj, geslo) == 0 ? "SE UJEMA" : "SE NE UJEMA");

    /* Schannel (TLS) mora iti naprej na pravi modul */
    r = AcquireCredentialsHandleW(NULL, L"Schannel", SECPKG_CRED_OUTBOUND, NULL, NULL, NULL, NULL,
                                  &pusto, &cas);
    printf("Schannel (mora biti S_OK iz pravega modula) = 0x%08x\n", r);

    DeleteSecurityContext(&kc);
    DeleteSecurityContext(&ks);
    FreeCredentialsHandle(&kuca);
    FreeCredentialsHandle(&kstreznik);
    printf("\nKONEC PREIZKUSA\n");
    return 0;
}
