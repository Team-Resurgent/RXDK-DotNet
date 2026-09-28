// RXDK-DotNet - managed self-test suite. Exercises core CLR/runtime features through the Mono
// interpreter on the original Xbox and reports PASS/FAIL per test via Console.WriteLine, which is
// redirected to the debug serial (OutputDebugStringA) by RxdkConsole below. Tests are ordered
// roughly low-risk -> high-risk so a hard interpreter fault still leaves the last "RUN <name>" line.
using System;
using System.IO;
using System.Text;
using System.Runtime.CompilerServices;
using System.Collections.Generic;

// Redirect managed Console output to the Xbox debug UART. A single native internal call (Write) is
// the sink; everything else is idiomatic managed code, so Console.WriteLine "just works" for the
// test suite and for real apps. Console.SetOut is done in Install().
public static class RxdkConsole
{
    [MethodImpl(MethodImplOptions.InternalCall)] public static extern void Write(string s);

    sealed class Writer : TextWriter
    {
        public override Encoding Encoding { get { return Encoding.UTF8; } }
        public override void Write(string s) { if (s != null) RxdkConsole.Write(s); }
        public override void Write(char c) { RxdkConsole.Write(c.ToString()); }
        public override void Write(char[] buffer, int index, int count) { RxdkConsole.Write(new string(buffer, index, count)); }
    }

    public static void Install() { Console.SetOut(new Writer()); }
}

public static class RxdkTest
{
    static int passed, failed;

    // Manual int->string (avoids int.ToString / Console, keeping the harness purely interp->interp
    // + the RxdkConsole.Write internal call, so it doesn't depend on delegates or the JIT trampoline).
    static string IStr(int v)
    {
        if (v == 0) return "0";
        bool neg = v < 0;
        uint u = neg ? (uint)(-v) : (uint)v;
        string s = "";
        while (u > 0) { s = (char)('0' + (int)(u % 10)) + s; u /= 10; }
        return neg ? "-" + s : s;
    }

    static string cur = "";
    static void Check(string name, bool ok)
    {
        RxdkConsole.Write((ok ? "  PASS  " : "  FAIL  ") + name + "\n");
        if (ok) passed++; else failed++;
    }

    // ---- kept from the first bring-up (host still calls these directly) ----------------------
    public static int Add(int a, int b) { return a + b; }
    public static int Fib(int n)
    {
        int a = 0, b = 1;
        for (int i = 0; i < n; i++) { int t = a + b; a = b; b = t; }
        return a;
    }

    // ---- individual feature tests (each returns bool) ---------------------------------------
    static int DAdd(int a, int b) { return a + b; }
    static bool T_Delegate() { Func<int,int,int> f = DAdd; return f(20, 22) == 42; }
    static bool DTrue() { return true; }
    static bool T_DelegateBool() { Func<bool> f = DTrue; return f(); }

    static bool T_Console()   { System.Console.WriteLine("[console] managed Console.WriteLine works"); return true; }
    // File I/O over the real Win32 file layer (w32file-win32.c + W->A thunks). Reads from the DVD
    // (D:\assy, bundled next to the host); the title drive is read-only when booted from disc.
    // FileStream is Mono's (MonoIO -> mono_w32file_* -> w32file-win32.c + W->A thunks).
    static bool T_FileStream()
    {
        byte[] head = new byte[2]; int n;
        using (var fs = new FileStream("D:\\assy\\mscorlib.dll", FileMode.Open, FileAccess.Read))
        { n = fs.Read(head, 0, 2); }
        return n == 2 && head[0] == (byte)'M' && head[1] == (byte)'Z';
    }
    // File/Directory are corefx (direct kernel32 P/Invokes via the mono_dl fallback).
    static bool T_FileExists() { return File.Exists("D:\\assy\\mscorlib.dll") && !File.Exists("D:\\assy\\nope.xyz"); }
    // Directory.Exists uses GetFileAttributesExW (works). Directory.GetFiles/enumeration goes through
    // corefx's ntdll NtQueryDirectoryFile path, which isn't wired yet (known follow-up), so it's not
    // exercised here.
    static bool T_DirExists() { return Directory.Exists("D:\\assy") && !Directory.Exists("D:\\assy\\nope"); }
    // Directory.GetFiles opens the directory (FILE_FLAG_BACKUP_SEMANTICS -> NtCreateFile) and pages
    // entries with NtQueryDirectoryFile. FileInfo.Length reads the size GetFileAttributesExW fills.
    static bool T_GetFiles()
    {
        string[] files = Directory.GetFiles("D:\\assy");
        bool saw = false;
        for (int i = 0; i < files.Length; i++)
            if (files[i].IndexOf("mscorlib.dll") >= 0) saw = true;
        return files.Length > 0 && saw;
    }
    static bool T_FileLength() { return new FileInfo("D:\\assy\\mscorlib.dll").Length > 1000; }
    // BCryptGenRandom is a KeTickCount-seeded xorshift. Guid.NewGuid must not be Empty and must not repeat.
    static bool T_Culture()
    {
        var en = new System.Globalization.CultureInfo("en-US");
        var region = new System.Globalization.RegionInfo("US");
        var specific = System.Globalization.CultureInfo.GetCultures(System.Globalization.CultureTypes.SpecificCultures);
        return en.Name == "en-US"
            && en.NumberFormat.NumberDecimalSeparator == "."
            && en.DateTimeFormat.GetDayName(System.DayOfWeek.Sunday) == "Sunday"
            && region.TwoLetterISORegionName == "US"
            && specific != null && specific.Length > 50;
    }
    static bool T_Guid()
    {
        var a = Guid.NewGuid();
        var b = Guid.NewGuid();
        return a != Guid.Empty && b != Guid.Empty && a != b;
    }
    // DeflateStream/GZipStream P/Invoke MonoPosixHelper, which is the bundled zlib.
    static bool RoundTrip(bool gzip)
    {
        byte[] src = Encoding.ASCII.GetBytes(new string('x', 48) + "RXDK-zlib-roundtrip" + new string('y', 48));
        var ms = new MemoryStream();
        if (gzip) {
            using (var z = new System.IO.Compression.GZipStream(ms, System.IO.Compression.CompressionMode.Compress, true))
                z.Write(src, 0, src.Length);
        } else {
            using (var z = new System.IO.Compression.DeflateStream(ms, System.IO.Compression.CompressionMode.Compress, true))
                z.Write(src, 0, src.Length);
        }
        if (ms.Length <= 0) return false;
        ms.Position = 0;
        byte[] dst = new byte[src.Length];
        int got = 0;
        if (gzip) {
            using (var z = new System.IO.Compression.GZipStream(ms, System.IO.Compression.CompressionMode.Decompress, true))
                while (got < dst.Length) {
                    int k = z.Read(dst, got, dst.Length - got);
                    if (k <= 0) break;
                    got += k;
                }
        } else {
            using (var z = new System.IO.Compression.DeflateStream(ms, System.IO.Compression.CompressionMode.Decompress, true))
                while (got < dst.Length) {
                    int k = z.Read(dst, got, dst.Length - got);
                    if (k <= 0) break;
                    got += k;
                }
        }
        if (got != src.Length) return false;
        for (int i = 0; i < got; i++) if (dst[i] != src[i]) return false;
        return true;
    }
    static bool T_Deflate() { return RoundTrip(false); }
    static bool T_Gzip() { return RoundTrip(true); }
    // Public UDP socket: echo to the title address, and 127.0.0.1 bind reports WSAEADDRNOTAVAIL.
    static bool T_Udp() { return System.Net.Sockets.Socket.Loopback(); }
    static bool T_UdpError() { return System.Net.Sockets.Socket.RejectsLoopback(); }
    static bool T_Tcp() { return System.Net.Sockets.Socket.TcpEcho(); }
    static bool T_DnsName() { return System.Net.Dns.HostName(); }
    static bool T_DnsAddr() { return System.Net.Dns.ResolveNumeric(); }
    static bool T_DnsLookup() { return System.Net.Dns.ResolveName(); }
    // Writes go to T:\ (the title's persistent HDD partition). D:\ is the DVD and is read-only.
    // File.WriteAllText/ReadAllText are corefx (FileStream under the hood). The FileStream test
    // writes a fixed count and reads that same count, so it does not depend on Length/GetFileSizeEx.
    const string WritePath = "T:\\rxdk-write.txt";
    static bool T_FileWriteAll()
    {
        const string payload = "xbox-write-ok";
        File.WriteAllText(WritePath, payload);
        bool ok = File.ReadAllText(WritePath) == payload;
        File.Delete(WritePath);
        return ok;
    }
    static bool T_FileStreamWrite()
    {
        byte[] bytes = Encoding.ASCII.GetBytes("stream");
        using (var fs = new FileStream(WritePath, FileMode.Create, FileAccess.Write))
            fs.Write(bytes, 0, bytes.Length);
        byte[] got = new byte[bytes.Length];
        int n;
        using (var fs = new FileStream(WritePath, FileMode.Open, FileAccess.Read))
            n = fs.Read(got, 0, got.Length);
        bool ok = n == bytes.Length;
        for (int i = 0; ok && i < bytes.Length; i++) ok = got[i] == bytes[i];
        File.Delete(WritePath);
        return ok;
    }
    // DateTime.Now/UtcNow + TimeZoneInfo.Local (offset from the Xbox EEPROM via kernel32
    // GetTimeZoneInformation, resolved through our mono_dl P/Invoke fallback).
    static bool T_DateTime()  { var u = DateTime.UtcNow; var n = DateTime.Now; var z = System.TimeZoneInfo.Local; return u.Year >= 2000 && n.Year >= 2000 && z != null; }

    static bool T_IntArith()  { return 7 * 6 == 42 && 100 / 7 == 14 && 100 % 7 == 2 && (3 - 9) == -6; }
    static bool T_Unchecked() { unchecked { int x = int.MaxValue; return x + 1 == int.MinValue; } }
    static bool T_Long()      { long a = 1L << 40; long b = 1000000L * 1000000L; return a == 1099511627776L && b == 1000000000000L; }
    static bool T_ULong()     { ulong u = 0xFFFFFFFFUL * 3UL; return u == 0x2FFFFFFFDUL; }
    static bool T_Double()    { double d = 3.0 * 2.5; double e = 10.0 / 4.0; return d == 7.5 && e == 2.5; }
    static bool T_Float()     { float f = 1.5f + 2.25f; return f == 3.75f; }
    static bool T_FloatDbl()  { double d = 2.5f; return d == 2.5; }
    static bool T_Bitops()    { return (0xF0 | 0x0F) == 0xFF && (0xFF & 0x0F) == 0x0F && (0xAA ^ 0xFF) == 0x55 && (~0) == -1; }
    static bool T_Shifts()    { return (1 << 8) == 256 && (256 >> 4) == 16 && (-8 >> 1) == -4 && ((int)(0x80000000u >> 4)) == 0x08000000; }
    static bool T_Compare()   { int x = 5; return (x > 3) && (x >= 5) && !(x < 5) && (x <= 5) && (x != 6) && (x == 5); }

    static bool T_Array()
    {
        int[] a = new int[5];
        for (int i = 0; i < a.Length; i++) a[i] = i * i;
        int sum = 0; for (int i = 0; i < a.Length; i++) sum += a[i];
        return a.Length == 5 && a[3] == 9 && sum == 30;
    }
    static bool T_ArrayBounds()
    {
        int[] a = new int[3];
        try { int v = a[5]; return false; }
        catch (IndexOutOfRangeException) { return true; }
    }
    static bool T_Foreach()
    {
        int[] a = { 2, 4, 6, 8 };
        int sum = 0; foreach (int v in a) sum += v;
        return sum == 20;
    }
    static bool T_Jagged()
    {
        int[][] j = new int[2][];
        j[0] = new int[] { 1, 2 }; j[1] = new int[] { 3, 4, 5 };
        return j[0].Length == 2 && j[1][2] == 5;
    }

    static bool T_String()
    {
        string s = "ab" + "cd";
        return s.Length == 4 && s == "abcd" && "hello".Substring(1, 3) == "ell" && "hello"[1] == 'e';
    }
    static bool T_StringApi()
    {
        return "Hello".ToUpper() == "HELLO" && "Hello".IndexOf('l') == 2 &&
               "a,b,c".Split(',').Length == 3 && "  x  ".Trim() == "x";
    }
    static bool T_StartsWith()
    {
        // regression for generics.cs test_0_fullaot_linq (Where(StartsWith("T")))
        return "one".StartsWith("T") == false && "Test".StartsWith("T") == true &&
               "one".StartsWith("o") == true  && "".StartsWith("T") == false;
    }
    static bool T_IntToString() { return 42.ToString() == "42" && (-7).ToString() == "-7"; }
    // float/double formatting via corefx Number.Windows DoubleToNumber -> _ecvt_s (win_crt_compat.c)
    static bool T_FloatToString() { return (1337.0f).ToString() == "1337" && (-1337.0f).ToString() == "-1337" &&
                                           (10.0).ToString() == "10" && (0.0).ToString() == "0" && (3.5).ToString() == "3.5"; }
    static bool T_Parse()       { return int.Parse("123") == 123; }

    struct Point { public int X, Y; public int Sum() { return X + Y; } }
    static bool T_Struct()
    {
        Point p; p.X = 3; p.Y = 4;
        Point q = p;          // value copy
        q.X = 100;
        return p.X == 3 && p.Sum() == 7 && q.X == 100;
    }
    static bool T_Box()
    {
        object o = 42;
        int back = (int)o;
        object s = "str";
        return back == 42 && (o is int) && (s is string);
    }

    static int s_counter;
    static bool T_StaticField() { s_counter = 0; for (int i = 0; i < 10; i++) s_counter++; return s_counter == 10; }

    class Counter { public int n; public void Inc() { n++; } public virtual int Kind() { return 1; } }
    class Counter2 : Counter { public override int Kind() { return 2; } }
    static bool T_Instance() { Counter c = new Counter(); c.Inc(); c.Inc(); c.Inc(); return c.n == 3; }
    static bool T_Virtual()  { Counter c = new Counter2(); return c.Kind() == 2; }

    interface IShape { int Area(); }
    class Square : IShape { int s; public Square(int side) { s = side; } public int Area() { return s * s; } }
    static bool T_Interface() { IShape sh = new Square(5); return sh.Area() == 25; }

    enum Color { Red = 1, Green = 2, Blue = 4 }
    static bool T_Enum() { Color c = Color.Green; return (int)c == 2 && (c | Color.Blue) == (Color)6; }

    static int Classify(int n) { switch (n) { case 0: return 100; case 1: return 200; case 2: return 300; default: return -1; } }
    static bool T_Switch() { return Classify(0) == 100 && Classify(2) == 300 && Classify(9) == -1; }

    static int Fact(int n) { return n <= 1 ? 1 : n * Fact(n - 1); }
    static bool T_Recursion() { return Fact(5) == 120 && Fact(10) == 3628800; }

    static void AddOut(int a, int b, out int r) { r = a + b; }
    static void Twice(ref int x) { x = x * 2; }
    static bool T_RefOut() { int r; AddOut(3, 4, out r); int y = 21; Twice(ref y); return r == 7 && y == 42; }

    static int Sum(params int[] vals) { int s = 0; foreach (int v in vals) s += v; return s; }
    static bool T_Params() { return Sum(1, 2, 3, 4) == 10 && Sum() == 0; }

    static T Max<T>(T a, T b) where T : IComparable<T> { return a.CompareTo(b) >= 0 ? a : b; }
    static bool T_Generics()
    {
        List<int> list = new List<int>();
        for (int i = 0; i < 5; i++) list.Add(i * 10);
        return list.Count == 5 && list[3] == 30 && Max<int>(3, 9) == 9;
    }

    static bool T_Exceptions()
    {
        int stage = 0;
        try { stage = 1; throw new InvalidOperationException("boom"); }
        catch (InvalidOperationException) { stage = 2; }
        finally { stage += 10; }
        return stage == 12;
    }
    static bool T_ExcRethrow()
    {
        try
        {
            try { throw new ArgumentException("x"); }
            catch (ArgumentException) { throw; }
        }
        catch (Exception e) { return e.Message == "x"; }
    }
    static bool T_NullRef()
    {
        string s = null;
        try { int n = s.Length; return false; }
        catch (NullReferenceException) { return true; }
    }
    static bool T_DivZero()
    {
        int z = 0;
        try { int q = 7 / z; return false; }
        catch (DivideByZeroException) { return true; }
    }

    // ---- runner -----------------------------------------------------------------------------
    // PASS/FAIL lines go via the raw RxdkConsole.Write sink; the Console test proves managed
    // System.Console.WriteLine also works (routes to the debug UART). Delegates work through the
    // native->interp trampoline (Delegate/DelegateBool below).
    public static int RunAll()
    {
        RxdkConsole.Write("=== RXDK-DotNet managed self-test (raw sink) ===\n");
        passed = 0; failed = 0;
        try { Check("Delegate",     T_Delegate()); }     catch (Exception e) { Exc("Delegate", e); }
        try { Check("DelegateBool", T_DelegateBool()); } catch (Exception e) { Exc("DelegateBool", e); }
        try { Check("Console",      T_Console()); }      catch (Exception e) { Exc("Console", e); }
        try { Check("DateTime",     T_DateTime()); }     catch (Exception e) { Exc("DateTime", e); }
        try { Check("FileExists",   T_FileExists()); }   catch (Exception e) { Exc("FileExists", e); }
        try { Check("FileStream",   T_FileStream()); }   catch (Exception e) { Exc("FileStream", e); }
        try { Check("DirExists",    T_DirExists()); }    catch (Exception e) { Exc("DirExists", e); }
        try { Check("FileWriteAll", T_FileWriteAll()); } catch (Exception e) { Exc("FileWriteAll", e); }
        try { Check("FileStreamWrite", T_FileStreamWrite()); } catch (Exception e) { Exc("FileStreamWrite", e); }
        try { Check("GetFiles",     T_GetFiles()); }     catch (Exception e) { Exc("GetFiles", e); }
        try { Check("FileLength",   T_FileLength()); }   catch (Exception e) { Exc("FileLength", e); }
        try { Check("Guid",         T_Guid()); }         catch (Exception e) { Exc("Guid", e); }
        try { Check("Culture",      T_Culture()); }      catch (Exception e) { Exc("Culture", e); }
        try { Check("Deflate",      T_Deflate()); }      catch (Exception e) { Exc("Deflate", e); }
        try { Check("Gzip",         T_Gzip()); }         catch (Exception e) { Exc("Gzip", e); }
        try { Check("Udp",          T_Udp()); }          catch (Exception e) { Exc("Udp", e); }
        try { Check("UdpError",     T_UdpError()); }     catch (Exception e) { Exc("UdpError", e); }
        try { Check("Tcp",          T_Tcp()); }          catch (Exception e) { Exc("Tcp", e); }
        try { Check("DnsName",      T_DnsName()); }      catch (Exception e) { Exc("DnsName", e); }
        try { Check("DnsAddr",      T_DnsAddr()); }      catch (Exception e) { Exc("DnsAddr", e); }
        try { Check("DnsLookup",    T_DnsLookup()); }    catch (Exception e) { Exc("DnsLookup", e); }
        // System.Console.WriteLine now WORKS (routes to the debug UART via the PAL console handle).
        // The fix: Environment.NewLine was returning null because icall-windows.c failed to compile
        // (shlobj.h), so its mono_icall_get_new_line was shadowed by a null-returning stub, NREing
        // System.Console's cctor. The harness still uses the RxdkConsole.Write sink for its PASS/FAIL
        // lines (lower-risk, no dependency on the Console stream setup), but Console.WriteLine is live.
        try { Check("IntArith",    T_IntArith()); }    catch (Exception e) { Exc("IntArith", e); }
        try { Check("Unchecked",   T_Unchecked()); }   catch (Exception e) { Exc("Unchecked", e); }
        try { Check("Long",        T_Long()); }        catch (Exception e) { Exc("Long", e); }
        try { Check("ULong",       T_ULong()); }       catch (Exception e) { Exc("ULong", e); }
        try { Check("Double",      T_Double()); }      catch (Exception e) { Exc("Double", e); }
        try { Check("Float",       T_Float()); }       catch (Exception e) { Exc("Float", e); }
        try { Check("FloatToDbl",  T_FloatDbl()); }    catch (Exception e) { Exc("FloatToDbl", e); }
        try { Check("Bitops",      T_Bitops()); }      catch (Exception e) { Exc("Bitops", e); }
        try { Check("Shifts",      T_Shifts()); }      catch (Exception e) { Exc("Shifts", e); }
        try { Check("Compare",     T_Compare()); }     catch (Exception e) { Exc("Compare", e); }
        try { Check("Array",       T_Array()); }       catch (Exception e) { Exc("Array", e); }
        try { Check("ArrayBounds", T_ArrayBounds()); } catch (Exception e) { Exc("ArrayBounds", e); }
        try { Check("Foreach",     T_Foreach()); }     catch (Exception e) { Exc("Foreach", e); }
        try { Check("Jagged",      T_Jagged()); }      catch (Exception e) { Exc("Jagged", e); }
        try { Check("String",      T_String()); }      catch (Exception e) { Exc("String", e); }
        try { Check("StringApi",   T_StringApi()); }   catch (Exception e) { Exc("StringApi", e); }
        try { Check("StartsWith",  T_StartsWith()); }  catch (Exception e) { Exc("StartsWith", e); }
        try { Check("IntToString", T_IntToString()); } catch (Exception e) { Exc("IntToString", e); }
        try { Check("FloatToString", T_FloatToString()); } catch (Exception e) { Exc("FloatToString", e); }
        try { Check("Parse",       T_Parse()); }       catch (Exception e) { Exc("Parse", e); }
        try { Check("Struct",      T_Struct()); }      catch (Exception e) { Exc("Struct", e); }
        try { Check("Box",         T_Box()); }         catch (Exception e) { Exc("Box", e); }
        try { Check("StaticField", T_StaticField()); } catch (Exception e) { Exc("StaticField", e); }
        try { Check("Instance",    T_Instance()); }    catch (Exception e) { Exc("Instance", e); }
        try { Check("Virtual",     T_Virtual()); }     catch (Exception e) { Exc("Virtual", e); }
        try { Check("Interface",   T_Interface()); }   catch (Exception e) { Exc("Interface", e); }
        try { Check("Enum",        T_Enum()); }        catch (Exception e) { Exc("Enum", e); }
        try { Check("Switch",      T_Switch()); }      catch (Exception e) { Exc("Switch", e); }
        try { Check("Recursion",   T_Recursion()); }   catch (Exception e) { Exc("Recursion", e); }
        try { Check("RefOut",      T_RefOut()); }      catch (Exception e) { Exc("RefOut", e); }
        try { Check("Params",      T_Params()); }      catch (Exception e) { Exc("Params", e); }
        try { Check("Generics",    T_Generics()); }    catch (Exception e) { Exc("Generics", e); }
        try { Check("Exceptions",  T_Exceptions()); }  catch (Exception e) { Exc("Exceptions", e); }
        try { Check("ExcRethrow",  T_ExcRethrow()); }  catch (Exception e) { Exc("ExcRethrow", e); }
        try { Check("NullRef",     T_NullRef()); }     catch (Exception e) { Exc("NullRef", e); }
        try { Check("DivZero",     T_DivZero()); }     catch (Exception e) { Exc("DivZero", e); }
        RxdkConsole.Write("=== SUMMARY passed=" + IStr(passed) + " failed=" + IStr(failed) + " ===\n");
        return failed;
    }

    static void Exc(string name, Exception e)
    {
        RxdkConsole.Write("  EXC   " + name + ": " + e.GetType().Name + ": " + e.Message + "\n");
        failed++;
    }
}
