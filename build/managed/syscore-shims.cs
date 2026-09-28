// RXDK-DotNet — tiny shims for building a minimal System.Core.dll (System.Linq) against our
// mscorlib. These types are referenced by corefx's System.Linq sources but live in assemblies /
// corlib versions we don't build. Kept minimal and behavior-preserving.
// corefx System.Linq references a handful of SR error strings. SR lives in mscorlib (IVT'd to
// System.Core) but lacks these Linq-specific members, and it can't be partial-extended across
// assemblies — so we define a System.Core-local SR with exactly the members System.Linq uses
// (same-assembly type resolution wins; CS0436 is suppressed). Messages match .NET Framework.
internal static class SR
{
    // System.Linq
    internal static string EmptyEnumerable    => "The input sequence is empty.";
    internal static string MoreThanOneElement => "Sequence contains more than one element";
    internal static string MoreThanOneMatch   => "Sequence contains more than one matching element";
    internal static string NoElements         => "Sequence contains no elements";
    internal static string NoMatch            => "Sequence contains no matching element";
    // HashSet<T> / SortedList<,> / collections
    internal static string Arg_ArrayPlusOffTooSmall => "Destination array is not long enough to copy all the items in the collection. Check array index and length.";
    internal static string Arg_HSCapacityOverflow => "HashSet capacity is too big.";
    internal static string Arg_KeyNotFoundWithKey => "The given key '{0}' was not present in the dictionary.";
    internal static string Arg_NonZeroLowerBound => "The lower bound of target array must be zero.";
    internal static string Arg_RankMultiDimNotSupported => "Only single dimensional arrays are supported for the requested action.";
    internal static string Arg_WrongType => "The value '{0}' is not of type '{1}' and cannot be used in this generic collection.";
    internal static string ArgumentOutOfRange_Index => "Index was out of range. Must be non-negative and less than the size of the collection.";
    internal static string ArgumentOutOfRange_NeedNonNegNum => "Non-negative number required.";
    internal static string ArgumentOutOfRange_SmallCapacity => "capacity was less than the current size.";
    internal static string Argument_AddingDuplicate => "An item with the same key has already been added.";
    internal static string Argument_InvalidArrayType => "Target array type is not compatible with the type of items in the collection.";
    internal static string InvalidOperation_ConcurrentOperationsNotSupported => "Operations that change non-concurrent collections must have exclusive access.";
    internal static string InvalidOperation_EnumFailedVersion => "Collection was modified; enumeration operation may not execute.";
    internal static string InvalidOperation_EnumOpCantHappen => "Enumeration has either not started or has already finished.";
    internal static string NotSupported_KeyCollectionSet => "Mutating a key collection derived from a dictionary is not allowed.";
    internal static string NotSupported_SortedListNestedWrite => "This operation is not supported on nested collections that require modifying the original SortedList.";
    internal static string Serialization_MissingKeys => "The keys for this dictionary are missing.";

    internal static string Format(string fmt, params object[] args) => string.Format(fmt, args);
    internal static string Format(string fmt, object a) => string.Format(fmt, a);
    internal static string Format(string fmt, object a, object b) => string.Format(fmt, a, b);
}

namespace System.Diagnostics.CodeAnalysis
{
    // Attribute-only marker; corefx tags iterator MoveNext helpers with it. No runtime behavior.
    [System.AttributeUsage(System.AttributeTargets.Class | System.AttributeTargets.Struct |
        System.AttributeTargets.Constructor | System.AttributeTargets.Method |
        System.AttributeTargets.Property | System.AttributeTargets.Event, Inherited = false, AllowMultiple = false)]
    internal sealed class ExcludeFromCodeCoverageAttribute : System.Attribute
    {
        public ExcludeFromCodeCoverageAttribute() { }
    }
}
