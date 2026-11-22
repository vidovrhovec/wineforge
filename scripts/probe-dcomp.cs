using System;
using System.Runtime.InteropServices;
using System.Windows.Forms;
using System.Drawing;

// Sonda: kaj od DirectComposition / DXGI verižni gradient dejansko dela.
// Namen: primerjati navadni Wine in zakrpan (d2d1-dcomp) build.
class DProbe {
    [DllImport("dcomp.dll")] static extern int DCompositionCreateDevice(IntPtr dxgiDevice, ref Guid iid, out IntPtr device);
    [DllImport("dcomp.dll")] static extern int DCompositionCreateSurfaceHandle(IntPtr device, IntPtr caps, out IntPtr handle);
    [DllImport("d3d11.dll")] static extern int D3D11CreateDevice(IntPtr adapter, int driver, IntPtr software,
        uint flags, IntPtr pFeatureLevels, uint featureLevels, uint sdkVersion, out IntPtr device, out int pDriver, out IntPtr context);

    static IntPtr Vtbl(IntPtr obj, int index) {
        return Marshal.ReadIntPtr(Marshal.ReadIntPtr(obj), index * IntPtr.Size);
    }
    [UnmanagedFunctionPointer(CallingConvention.StdCall)]
    delegate int FnVoid(out IntPtr res);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)]
    delegate int FnPtr(IntPtr self, IntPtr a, IntPtr b, out IntPtr res);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)]
    delegate int FnU32(IntPtr self, uint a, uint b);

    static void R(string kaj, int hr) {
        Console.WriteLine("  " + kaj.PadRight(34) + " -> 0x" + hr.ToString("X8") +
                          (hr == 0 ? "  USPEH" : "  napaka"));
    }

    static bool done = false;

    static void Main() {
        Console.WriteLine("=== DComp / DXGI sonda ===");
        var f = new Form { Text = "dcomp sonda", Width = 400, Height = 300, StartPosition = FormStartPosition.CenterScreen };
        f.Shown += (s, e) => {
            IntPtr dev = IntPtr.Zero, ctx = IntPtr.Zero; int driver;
            int[] tipi = new int[] { 0, 5, 1 };  // HARDWARE, WARP, NULL
            string[] imena = new string[] { "HARDWARE", "WARP", "NULL" };
            for (int ti = 0; ti < tipi.Length && dev == IntPtr.Zero; ti++)
                R("D3D11CreateDevice(" + imena[ti] + ")",
                  D3D11CreateDevice(IntPtr.Zero, tipi[ti], IntPtr.Zero, 2, IntPtr.Zero, 0, 7,
                                    out dev, out driver, out ctx));
            if (dev == IntPtr.Zero) { Console.WriteLine("  (brez D3D11 — nadaljujem samo z ravnimi izvozi)"); }

            // ravni izvozi dcomp.dll — brez tveganja
            Guid iid = new Guid("c37ea535-1837-42c2-8a6a-4050b4dbfa3b"); // IID_IDCompositionDevice
            IntPtr dcomp;
            R("CreateDevice(IDCompositionDevice)", DCompositionCreateDevice(dev, ref iid, out dcomp));
            Guid iid2 = new Guid("c8aa3b27-e28a-4fc0-b63b-c26aa165a149"); // IDCompositionDevice2
            R("CreateDevice(IDCompositionDevice2)", DCompositionCreateDevice(dev, ref iid2, out dcomp));
            Guid iidu = new Guid("00000000-0000-0000-c000-000000000046"); // IUnknown
            R("CreateDevice(IUnknown)", DCompositionCreateDevice(dev, ref iidu, out dcomp));
            R("CreateDevice(null dev, IDCompDevice)", DCompositionCreateDevice(IntPtr.Zero, ref iid, out dcomp));
            IntPtr pov;
            R("DCompositionCreateSurfaceHandle", DCompositionCreateSurfaceHandle(IntPtr.Zero, IntPtr.Zero, out pov));

            if (dev != IntPtr.Zero) {
                Console.WriteLine("  (DXGI vtable veriga izpuscena)");
            }
            Console.WriteLine("=== konec sonde ===");
            done = true;
        };
        f.Show();
        for (int k = 0; k < 40 && !done; k++) { Application.DoEvents(); System.Threading.Thread.Sleep(80); }
        Application.DoEvents();
        Environment.Exit(0);
    }

    // IDXGIDevice::GetParent(IID_IDXGIFactory2) — c# pomocnik
    static IntPtr GetFactory2(IntPtr dev) {
        Guid iid = new Guid("50c83a1c-e072-4c48-87b0-36f6fa2d0548");
        int size = Marshal.SizeOf(iid);
        IntPtr buf = Marshal.AllocHGlobal(size);
        Marshal.StructureToPtr(iid, buf, false);
        var getParent = (FnPtr)Marshal.GetDelegateForFunctionPointer(Vtbl(dev, 6), typeof(FnPtr));
        IntPtr res;
        int hr = getParent(dev, buf, IntPtr.Zero, out res);
        return hr == 0 ? res : IntPtr.Zero;
    }
}
