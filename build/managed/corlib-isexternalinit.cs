// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

// .NET 5's marker for init accessors, which neither Mono nor corefx has. The compiler requires it by
// name to emit `init` (and so records), so without it any library using them fails with CS0518.
// .NET 5 marks it [EditorBrowsable(Never)], which in the net_4_x profile is System.dll's.

namespace System.Runtime.CompilerServices
{
    /// <summary>
    /// Reserved to be used by the compiler for tracking metadata.
    /// This class should not be used by developers in source code.
    /// </summary>
    public static class IsExternalInit
    {
    }
}
