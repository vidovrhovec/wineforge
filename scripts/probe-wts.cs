using System;
using System.Runtime.InteropServices;
using System.Text;

class W {
  [StructLayout(LayoutKind.Sequential)]
  struct WTS_SESSION_INFO { public IntPtr Reserved; public uint SessionId; public IntPtr Name; public uint State; }

  [DllImport("wtsapi32.dll")] static extern uint WTSGetActiveConsoleSessionId();
  [DllImport("wtsapi32.dll", SetLastError = true)]
  static extern bool WTSEnumerateSessionsW(IntPtr h, uint r, uint v, out IntPtr pp, out int cnt);
  [DllImport("wtsapi32.dll", SetLastError = true)]
  static extern bool WTSQueryUserToken(uint sid, out IntPtr tok);
  [DllImport("wtsapi32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
  static extern bool WTSQuerySessionInformationW(IntPtr h, uint sid, int cls, out IntPtr buf, out int cnt);
  [DllImport("kernel32.dll", SetLastError = true)] static extern uint GetCurrentProcessId();
  [DllImport("kernel32.dll", SetLastError = true)] static extern bool ProcessIdToSessionId(uint pid, out uint sid);
  [DllImport("advapi32.dll")] static extern int LsaEnumerateLogonSessions(out long count, out IntPtr luids);
  [DllImport("advapi32.dll")] static extern int LsaGetLogonSessionData(IntPtr luid, out IntPtr info);
  [DllImport("advapi32.dll")] static extern uint LsaNtStatusToWinError(int status);

  static void Main() {
    Console.WriteLine("aktivna konzolna seja: " + WTSGetActiveConsoleSessionId());
    uint pid = GetCurrentProcessId(); uint sid = 999;
    bool ok = ProcessIdToSessionId(pid, out sid);
    Console.WriteLine("ProcessIdToSessionId(" + pid + ") = " + ok + " seja=" + sid);

    IntPtr arr; int n;
    if (WTSEnumerateSessionsW(IntPtr.Zero, 0, 1, out arr, out n)) {
      Console.WriteLine("WTSEnumerateSessions: " + n + " sej");
      int sz = Marshal.SizeOf(typeof(WTS_SESSION_INFO));
      for (int i = 0; i < n; i++) {
        var si = (WTS_SESSION_INFO)Marshal.PtrToStructure(IntPtr.Add(arr, i * sz), typeof(WTS_SESSION_INFO));
        string ime = si.Name == IntPtr.Zero ? "?" : Marshal.PtrToStringUni(si.Name);
        Console.WriteLine("  seja " + si.SessionId + "  " + ime + "  stanje=" + si.State);
      }
    } else Console.WriteLine("WTSEnumerateSessions NAPAKA " + Marshal.GetLastWin32Error());

    IntPtr tok;
    bool q = WTSQueryUserToken(1, out tok);
    Console.WriteLine("WTSQueryUserToken(seja 1) = " + q + "  zadnja napaka=" + Marshal.GetLastWin32Error());

    IntPtr ubuf = IntPtr.Zero; int cb = 0;
    bool u = WTSQuerySessionInformationW(IntPtr.Zero, 1, 5, out ubuf, out cb); // WTSUserName
    Console.WriteLine("WTSQuerySessionInformation(UserName) = " + u + " -> " +
                      (ubuf == IntPtr.Zero ? "(prazno)" : "'" + Marshal.PtrToStringUni(ubuf) + "'") +
                      "  napaka=" + Marshal.GetLastWin32Error());

    long st; long cnt2; IntPtr lu;
    int s = LsaEnumerateLogonSessions(out cnt2, out lu);
    Console.WriteLine("LsaEnumerateLogonSessions status=0x" + s.ToString("X8") +
                      " (Win32 " + LsaNtStatusToWinError(s) + ")  stevilo=" + cnt2);
    if (s == 0 && cnt2 > 0) {
      for (int i = 0; i < Math.Min(cnt2, 4); i++) {
        IntPtr info;
        int g = LsaGetLogonSessionData(IntPtr.Add(lu, i * 8), out info);
        Console.WriteLine("  seja " + i + ": LsaGetLogonSessionData = 0x" + g.ToString("X8"));
        if (g == 0 && info != IntPtr.Zero) {
          // SECURITY_LOGON_SESSION_DATA: paket 4 = Username (LSA_UNICODE_STRING), 5 = LogonDomain, 7 = LogonType
          var pkt = Marshal.ReadIntPtr(info, 6 * 8);
          Console.WriteLine("     paket(6) = 0x" + pkt.ToString("x"));
        }
      }
    }
  }
}
