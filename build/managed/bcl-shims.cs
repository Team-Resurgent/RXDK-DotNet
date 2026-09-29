// RXDK-DotNet — shims for building the extra corefx class libraries (System.Numerics.Vectors,
// System.Runtime.Serialization) against our mscorlib. Same situation as syscore-shims.cs: corefx
// sources reference per-assembly internals that its own build generates. Each assembly compiles its
// own copy; same-assembly resolution wins (CS0436 is suppressed).

// mscorlib has SR but it is internal to corlib and cannot be extended across assemblies, so each
// library gets the handful of members its sources actually use.
internal static class SR
{
    // System.Runtime.Serialization.Primitives
    internal static string OrderCannotBeNegative => "Property 'Order' in DataMemberAttribute attribute cannot be a negative number.";

    // System.Numerics.Vectors
    internal static string Arg_ArgumentOutOfRangeException => "Specified argument was out of the range of valid values.";
    internal static string Arg_ElementsInSourceIsGreaterThanDestination => "Number of elements in source vector is greater than the destination array.";
    internal static string Arg_NullArgumentNullRef => "The method was called with a null array argument.";

    internal static string Format(string fmt, params object[] args) => string.Format(fmt, args);
    internal static string Format(string fmt, object a) => string.Format(fmt, a);
}
