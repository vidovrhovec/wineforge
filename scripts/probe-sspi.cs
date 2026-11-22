using System;
using System.IO;
using System.IO.Pipes;
using System.Runtime.InteropServices;
using System.Security.Principal;

class T {
  [DllImport("secur32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
  static extern int AcquireCredentialsHandleW(string pszPrincipal, string pszPackage,
      int fCredUsage, IntPtr pvLogonID, IntPtr pAuthData, IntPtr pszKeyFilePath,
      IntPtr pGetKeyPassword, out IntPtr phCred, out long ptsExpiry);

  [DllImport("secur32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
  static extern int InitializeSecurityContextW(ref IntPtr phCred, IntPtr phContext,
      string pszTargetName, uint fContextReq, IntPtr reserved1, uint TargetDataRep,
      IntPtr pInput, IntPtr reserved2, IntPtr phNewContext, out IntPtr pbOutput,
      out uint pfContextAttr, out long ptsExpiry);

  const int SECPKG_CRED_OUTBOUND = 2;

  static void Main() {
    string[] cevi = { @"lrpc\lsasspirpc", @"lsass\lsasspirpc", @"protected_storage" };
    foreach (var c in cevi) {
      try {
        using (var p = new NamedPipeClientStream(".", c, PipeDirection.Out)) {
          p.Connect(1500);
          Console.WriteLine("cev " + c + ": ODPRTO");
        }
      } catch (Exception e) {
        Console.WriteLine("cev " + c + ": " + e.GetType().Name + " (" + e.Message + ")");
      }
    }

    string[] paketi = { "Negotiate", "NTLM", "Kerberos", "MSV1_0", "CredSSP", "Schannel" };
    foreach (var pkg in paketi) {
      IntPtr cred; long exp;
      try {
        int r = AcquireCredentialsHandleW(null, pkg, SECPKG_CRED_OUTBOUND, IntPtr.Zero,
                                          IntPtr.Zero, IntPtr.Zero, IntPtr.Zero,
                                          out cred, out exp);
        Console.WriteLine("AcquireCredentialsHandle(" + pkg + ") = 0x" + r.ToString("X8"));
      } catch (Exception e) {
        Console.WriteLine("AcquireCredentialsHandle(" + pkg + "): " + e.GetType().Name);
      }
    }

    foreach (var pkg in new string[] { "Negotiate", "NTLM" }) {
      IntPtr cred, ctx = IntPtr.Zero, outBuf; long exp; uint attr = 0;
      try {
        int r = AcquireCredentialsHandleW(null, pkg, SECPKG_CRED_OUTBOUND, IntPtr.Zero,
              IntPtr.Zero, IntPtr.Zero, IntPtr.Zero, out cred, out exp);
        if (r != 0) { Console.WriteLine(pkg + ": opustim (0x" + r.ToString("X8") + ")"); continue; }
        int q = InitializeSecurityContextW(ref cred, ctx, "localhost", 0x00000002, IntPtr.Zero,
              0, IntPtr.Zero, IntPtr.Zero, ctx, out outBuf, out attr, out exp);
        Console.WriteLine("InitializeSecurityContext(" + pkg + ") = 0x" + q.ToString("X8")
            + (q == 0x00090312 ? "  <- SEC_I_CONTINUE_NEEDED, tokren generiran" : ""));
      } catch (Exception e) {
        Console.WriteLine("InitializeSecurityContext(" + pkg + "): " + e.GetType().Name);
      }
    }

    // 5) prava Negotiate avtentikacija (kot jo uporablja program)
    foreach (var tgt in new string[] { "localhost", "TERMSRV/localhost" }) {
      try {
        using (var s2 = new System.Net.Security.NegotiateStream(new MemoryStream())) {
          s2.AuthenticateAsClient(tgt, System.Net.CredentialCache.DefaultNetworkCredentials,
                                    System.Net.Security.ProtectionLevel.None, false);
          Console.WriteLine("NegotiateStream(" + tgt + ") uspeh");
        }
      } catch (Exception e) {
        Console.WriteLine("NegotiateStream(" + tgt + "): " + e.GetType().Name + " - " + e.Message);
      }
    }

    WindowsIdentity id = WindowsIdentity.GetCurrent();
    Console.WriteLine("uporabnik: " + id.Name + "  avtentikacija: " + id.AuthenticationType);
}
}
