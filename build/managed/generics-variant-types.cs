// RXDK-DotNet — C# equivalent of vendor/mono/mono/mini/generics-variant-types.il, so the official
// generics.cs mini test can be built without ilasm (which the RXDK toolchain doesn't ship). These
// are global-namespace variant interfaces the test uses to exercise generic variance in the runtime.
// Assembly name must be "generics-variant-types" (generics.cs references it by that name).

// A covariant interface.
public interface MyIEnumerator<out T>
{
    bool MoveNext();
    T Current { get; }
}

// A contravariant interface (global namespace — distinct from System.Collections.Generic.IComparer<T>).
public interface IComparer<in T>
{
    bool Compare(T x, T y);
}

public interface IKeyComparer<in T> : IComparer<T>
{
}
