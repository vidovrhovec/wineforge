using System;
using System.Runtime.InteropServices;
using System.Threading;

class XProbe {
    [StructLayout(LayoutKind.Sequential)]
    struct GAMEPAD {
        public ushort wButtons; public byte bLeftTrigger; public byte bRightTrigger;
        public short sThumbLX; public short sThumbLY; public short sThumbRX; public short sThumbRY;
    }
    [StructLayout(LayoutKind.Sequential)]
    struct STATE { public uint dwPacketNumber; public GAMEPAD gp; }

    [DllImport("xinput1_4.dll")] static extern uint XInputGetState(uint user, out STATE s);
    [DllImport("xinput1_4.dll")] static extern uint XInputGetCapabilities(uint user, uint flags, IntPtr caps);

    const uint A = 0x1000, B = 0x2000, X = 0x4000, Y = 0x8000, START = 0x0010, DPAD_UP = 0x0001;

    static string Names(ushort w) {
        string[] nm = { "DPAD_UP", "DPAD_DOWN", "DPAD_LEFT", "DPAD_RIGHT", "START", "BACK",
                        "LSTICK", "RSTICK", "LB", "RB", "GUIDE", "A", "B", "X", "Y" };
        ushort[] bit = { 0x0001, 0x0002, 0x0004, 0x0008, 0x0010, 0x0020, 0x0040, 0x0080,
                         0x0100, 0x0200, 0x0400, 0x1000, 0x2000, 0x4000, 0x8000 };
        string r = "";
        for (int i = 0; i < bit.Length; i++) if ((w & bit[i]) != 0) r += nm[i] + " ";
        return r == "" ? "-" : r.Trim();
    }

    static void Main() {
        for (uint i = 0; i < 4; i++) {
            STATE s;
            uint rc = XInputGetState(i, out s);
            Console.WriteLine("kontrolnik " + i + ": " + (rc == 0 ? "POVEZAN" : "ni (" + rc + ")"));
        }
        uint zadnji = 0;
        for (int k = 0; k < 900; k++) {
            STATE s;
            if (XInputGetState(0, out s) == 0) {
                if (s.dwPacketNumber != zadnji) {
                    zadnji = s.dwPacketNumber;
                    Console.WriteLine("paket " + s.dwPacketNumber + ": gumbi=" + Names(s.gp.wButtons)
                                      + " LSTICK=(" + s.gp.sThumbLX + "," + s.gp.sThumbLY + ")");
                }
            }
            Thread.Sleep(50);
        }
    }
}
