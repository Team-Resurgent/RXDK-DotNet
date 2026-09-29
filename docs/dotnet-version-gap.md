# The .NET version gap

Written while planning the MonoGame port, because "our runtime is .NET Framework 4.6 era, MonoGame
needs .NET 8" is the wrong summary and kept leading to the wrong conclusion. Everything below was
measured against the `mscorlib.dll` in `build-out/corlib`, not read off a version number.

## What the corlib already has

`scripts/build-corlib.sh` passes `-define:NET_4_0;NET_4_5;NET_4_6`, which undersells it badly. The
source list it assembles pulls corefx's CoreLib and `System.Memory` sources straight into
`mscorlib.dll`. From `vendor/mono/mcs/class/corlib/corlib.dll.sources`:

| Type | Source line |
|---|---|
| `System.HashCode` | 1701 |
| `System.Index` | 1703 |
| `System.MathF` | 1704 |
| `System.Memory<T>` | 1706 |
| `System.Range` | 1710 |
| `System.ReadOnlySpan<T>` | 1712 |
| `System.Span<T>` | 1714 |
| `System.ValueTuple` | 1725 |
| `System.Buffers.ArrayPool<T>` | 1728 |
| `System.Numerics.Vector<T>` | 1769 |

None of them are dropped by the win32 exclude lists — checked against the generated
`build-out/corlib/paths.txt` and `exclpaths.txt`.

These are not just present, they are *bindable*. A probe compiled `-nostdlib` against our three
assemblies using `Span<T>`, `ReadOnlySpan<T>`, `Memory<T>`, `ReadOnlyMemory<T>`,
`ArrayPool<T>.Shared`, `MathF`, `HashCode`, `ValueTuple`, `Index`, `Range`, and `Vector<T>` compiled
clean. `stackalloc` into a `Span<int>` works.

The one miss is `System.Runtime.CompilerServices.Unsafe`: corlib has it at
`corlib.dll.sources:337` but it is `internal` there, so a title gets
`CS0122: 'Unsafe' is inaccessible due to its protection level`. The public one lives in Mono's
separate `mcs/class/System.Runtime.CompilerServices.Unsafe` assembly, which we have not built yet.

## What the C# compiler can and cannot do here

Measured by compiling one file with every construct and reading the whole error list.

Works today, against our corlib:

- Default interface members. (Compiles. Whether the interpreter *executes* them is untested.)
- `switch` expressions, property patterns, target-typed `new`, nullable annotations.
- Index and range operators on arrays: `arr[^1]`, `arr[1..3]`.
- Interpolated strings, and C# 12 collection expressions (`int[] x = [1, 2, 3];`).

Fails, but only for want of marker attributes that any assembly may declare itself:

- `record` types and `init`-only setters need `System.Runtime.CompilerServices.IsExternalInit`
  (`CS0518`).
- `required` members need `RequiredMemberAttribute` and `CompilerFeatureRequiredAttribute`
  (`CS0656`).

Fails for real, because the runtime genuinely does not support them:

- `ref` fields in a `ref struct` — `CS9064: Target runtime doesn't support ref fields`.
- Static abstract interface members — `CS8919: Target runtime doesn't support static abstract
  members in interfaces`.

The first group is a few lines of shim per assembly. The second group has to be rewritten at each
use site; the compiler gates both on runtime feature flags, and claiming support we do not have
would produce IL the interpreter cannot run.

## Why a literal .NET 8 runtime is out of scope

Two different jobs hide behind "upgrade to .NET 8", and only one is worth doing.

**Extending the API surface** is incremental and uses machinery we already have. Mono 6.13 ships
`mcs/class/System.Numerics`, `System.Numerics.Vectors`,
`System.Runtime.CompilerServices.Unsafe`, and `mcs/class/Facades/`. Each is a normal C# assembly
that builds exactly the way `scripts/build-syscore.sh` builds `System.Core.dll`. When something is
missing, this is the answer.

**Moving to the .NET 8 runtime** is not an upgrade, it is a re-platform. It means
`System.Private.CoreLib` and dotnet/runtime's MonoVM: a different runtime tree, a different corelib
with a different embedding API, a new PAL, a new globalization story, and x86 support we would be
porting from zero. It discards the 6.13 work that already boots and passes its self-test on
hardware, and it buys API surface we can otherwise get one assembly at a time.

The reason the gap is narrow in practice is that **we compile from source**. Titles and MonoGame
alike are built `-nostdlib` against our own reference set, so assembly identity, `TargetFramework`,
and `System.Runtime` versioning never come into it. A tree that calls itself `net8.0` is just C#
source until we compile it. That reduces "can we support newer .NET" to two questions that are
answered by compiling, not arguing:

1. Does the source bind against our references?
2. Can the interpreter execute the IL the compiler emits?

## Practical consequence

When a third-party tree does not compile, work through it in this order:

1. Is the type in another Mono class library we have not built yet? Build it.
2. Is it a marker attribute the compiler wants? Declare it.
3. Is it `ref` fields or static abstract interface members? Rewrite the use site.
4. Only if the answer is none of those is the tree itself the wrong choice.
