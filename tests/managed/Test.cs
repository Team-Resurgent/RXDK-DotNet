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

    static void Check(string name, bool ok)
    {
        Console.WriteLine((ok ? "  PASS  " : "  FAIL  ") + name);
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
    static bool T_IntToString() { return 42.ToString() == "42" && (-7).ToString() == "-7"; }
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
    static void Run(string name, Func<bool> t)
    {
        Console.WriteLine("RUN " + name);
        try { Check(name, t()); }
        catch (Exception e) { Console.WriteLine("  EXC   " + name + ": " + e.GetType().Name); failed++; }
    }

    public static int RunAll()
    {
        RxdkConsole.Write("RunAll: entered (raw sink)\n");
        Console.WriteLine("=== RXDK-DotNet managed self-test (via Console.WriteLine) ===");
        passed = 0; failed = 0;
        Run("IntArith",   T_IntArith);
        Run("Unchecked",  T_Unchecked);
        Run("Long",       T_Long);
        Run("ULong",      T_ULong);
        Run("Double",     T_Double);
        Run("Float",      T_Float);
        Run("FloatToDbl", T_FloatDbl);
        Run("Bitops",     T_Bitops);
        Run("Shifts",     T_Shifts);
        Run("Compare",    T_Compare);
        Run("Array",      T_Array);
        Run("ArrayBounds",T_ArrayBounds);
        Run("Foreach",    T_Foreach);
        Run("Jagged",     T_Jagged);
        Run("String",     T_String);
        Run("StringApi",  T_StringApi);
        Run("IntToString",T_IntToString);
        Run("Parse",      T_Parse);
        Run("Struct",     T_Struct);
        Run("Box",        T_Box);
        Run("StaticField",T_StaticField);
        Run("Instance",   T_Instance);
        Run("Virtual",    T_Virtual);
        Run("Interface",  T_Interface);
        Run("Enum",       T_Enum);
        Run("Switch",     T_Switch);
        Run("Recursion",  T_Recursion);
        Run("RefOut",     T_RefOut);
        Run("Params",     T_Params);
        Run("Generics",   T_Generics);
        Run("Exceptions", T_Exceptions);
        Run("ExcRethrow", T_ExcRethrow);
        Run("NullRef",    T_NullRef);
        Run("DivZero",    T_DivZero);
        Console.WriteLine("=== SUMMARY passed=" + passed + " failed=" + failed + " ===");
        return failed;
    }
}
