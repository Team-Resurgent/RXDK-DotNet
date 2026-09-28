// RXDK-DotNet — minimal managed test assembly. Pure-arithmetic static methods (no allocation, no
// dependencies beyond System.Int32) to prove the Mono interpreter executes IL end-to-end on Xbox.
public static class RxdkTest
{
    public static int Add(int a, int b) { return a + b; }
    public static int Fib(int n)
    {
        int a = 0, b = 1;
        for (int i = 0; i < n; i++) { int t = a + b; a = b; b = t; }
        return a;
    }
}
