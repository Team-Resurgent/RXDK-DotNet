// The rest of libxgraphics: swizzle, compression, resource headers, shader tools,
// and saving a texture. Math lives in Math.cs. Cube and volume images are here
// because libd3dx8 has no volume-from-file loader.
using System;
using System.Runtime.InteropServices;

namespace Rxdk
{
    [StructLayout(LayoutKind.Sequential)]
    public struct Rect
    {
        public int Left, Top, Right, Bottom;
        public Rect(int left, int top, int right, int bottom)
        {
            Left = left;
            Top = top;
            Right = right;
            Bottom = bottom;
        }
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Point
    {
        public int X, Y;
        public Point(int x, int y) { X = x; Y = y; }
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Box
    {
        public uint Left, Top, Right, Bottom, Front, Back;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Point3
    {
        public uint U, V, W;
    }

    [Flags]
    public enum CompressFlags : uint
    {
        Premultiply = 0x1,
        NeedAlpha0 = 0x2,
        NeedAlpha1 = 0x4,
        ProtectNonZero = 0x8
    }

    public sealed class ShaderCompile
    {
        public int Status;
        public int ShaderType;
        public byte[] Shader;
        public byte[] Constants;
        public string Errors;
        public string Listing;
        public string MachineListing;
    }

    public sealed class CubeTexture : IDisposable
    {
        readonly IntPtr handle;
        bool disposed;

        CubeTexture(IntPtr handle) { this.handle = handle; }
        internal IntPtr Handle { get { return handle; } }

        public static unsafe CubeTexture FromMemory(byte[] data)
        {
            if (data == null)
                throw new ArgumentNullException("data");
            fixed (byte* p = data)
                return Adopt(GfxNative.CubeFromMemory((IntPtr)p, data.Length));
        }

        public static unsafe CubeTexture FromFile(string path)
        {
            if (path == null)
                throw new ArgumentNullException("path");
            byte[] ansi = ToAnsi(path);
            fixed (byte* p = ansi)
                return Adopt(GfxNative.CubeFromFile((IntPtr)p));
        }

        static CubeTexture Adopt(IntPtr handle)
        {
            if (handle == IntPtr.Zero)
                throw new InvalidOperationException("texture");
            return new CubeTexture(handle);
        }

        public void Dispose()
        {
            if (disposed)
                return;
            disposed = true;
            GfxNative.Release(handle);
            GC.SuppressFinalize(this);
        }

        ~CubeTexture() { Dispose(); }

        static byte[] ToAnsi(string text)
        {
            byte[] bytes = new byte[text.Length + 1];
            for (int i = 0; i < text.Length; i++)
                bytes[i] = text[i] < 128 ? (byte)text[i] : (byte)'?';
            return bytes;
        }
    }

    public sealed class VolumeTexture : IDisposable
    {
        readonly IntPtr handle;
        bool disposed;

        VolumeTexture(IntPtr handle) { this.handle = handle; }
        internal IntPtr Handle { get { return handle; } }

        public static VolumeTexture Create(int width, int height, int depth, int mipLevels, SurfaceFormat format)
        {
            if (width <= 0 || height <= 0 || depth <= 0)
                throw new ArgumentOutOfRangeException("width");
            IntPtr handle = GfxNative.VolumeCreate(width, height, depth, mipLevels, (int)format);
            if (handle == IntPtr.Zero)
                throw new OutOfMemoryException();
            return new VolumeTexture(handle);
        }

        public void Dispose()
        {
            if (disposed)
                return;
            disposed = true;
            GfxNative.Release(handle);
            GC.SuppressFinalize(this);
        }

        ~VolumeTexture() { Dispose(); }
    }

    public static class XGraphics
    {
        public static bool IsSwizzled(SurfaceFormat format)
        {
            return Xg.Swizzled((uint)format) != 0;
        }

        public static int BytesPerPixel(SurfaceFormat format)
        {
            return (int)Xg.BytesPerPixel((uint)format);
        }

        public static unsafe void Swizzle(byte[] source, byte[] destination, int width, int height, int bytesPerPixel)
        {
            Pin(source, destination);
            fixed (byte* s = source)
            fixed (byte* d = destination)
                Xg.Swizzle((IntPtr)s, 0, IntPtr.Zero, (IntPtr)d, width, height, IntPtr.Zero, bytesPerPixel);
        }

        public static unsafe void Swizzle(byte[] source, int pitch, Rect sourceRect, byte[] destination, int width, int height, Point destPoint, int bytesPerPixel)
        {
            Pin(source, destination);
            fixed (byte* s = source)
            fixed (byte* d = destination)
                Xg.Swizzle((IntPtr)s, (uint)pitch, ref sourceRect, (IntPtr)d, width, height, ref destPoint, bytesPerPixel);
        }

        public static unsafe void Unswizzle(byte[] source, byte[] destination, int width, int height, int bytesPerPixel)
        {
            Pin(source, destination);
            fixed (byte* s = source)
            fixed (byte* d = destination)
                Xg.Unswizzle((IntPtr)s, width, height, IntPtr.Zero, (IntPtr)d, 0, IntPtr.Zero, bytesPerPixel);
        }

        public static unsafe void Unswizzle(byte[] source, int width, int height, Rect sourceRect, byte[] destination, int pitch, Point destPoint, int bytesPerPixel)
        {
            Pin(source, destination);
            fixed (byte* s = source)
            fixed (byte* d = destination)
                Xg.Unswizzle((IntPtr)s, width, height, ref sourceRect, (IntPtr)d, (uint)pitch, ref destPoint, bytesPerPixel);
        }

        public static unsafe void SwizzleBox(byte[] source, int rowPitch, int slicePitch, Box box, byte[] destination, int width, int height, int depth, Point3 point, int bytesPerPixel)
        {
            Pin(source, destination);
            fixed (byte* s = source)
            fixed (byte* d = destination)
                Xg.SwizzleBox((IntPtr)s, (uint)rowPitch, (uint)slicePitch, ref box, (IntPtr)d, width, height, depth, ref point, bytesPerPixel);
        }

        public static unsafe void UnswizzleBox(byte[] source, int width, int height, int depth, Box box, byte[] destination, int rowPitch, int slicePitch, Point3 point, int bytesPerPixel)
        {
            Pin(source, destination);
            fixed (byte* s = source)
            fixed (byte* d = destination)
                Xg.UnswizzleBox((IntPtr)s, width, height, depth, ref box, (IntPtr)d, (uint)rowPitch, (uint)slicePitch, ref point, bytesPerPixel);
        }

        public static unsafe int CompileIndexedDraw(byte[] pushBuffer, PrimitiveType type, int vertexCount, byte[] indices)
        {
            if (pushBuffer == null)
                throw new ArgumentNullException("pushBuffer");
            if (indices == null)
                throw new ArgumentNullException("indices");
            int size = pushBuffer.Length;
            fixed (byte* b = pushBuffer)
            fixed (byte* ix = indices)
            {
                int hr = Xg.CompileIndexed((IntPtr)b, ref size, (int)type, vertexCount, (IntPtr)ix);
                if (hr < 0)
                    throw new InvalidOperationException("pushbuffer");
                return size;
            }
        }

        public static unsafe int Compress(byte[] source, SurfaceFormat sourceFormat, int sourcePitch, byte[] destination, SurfaceFormat destinationFormat, int destinationPitch, int width, int height, float alphaRef, CompressFlags flags)
        {
            Pin(source, destination);
            fixed (byte* s = source)
            fixed (byte* d = destination)
                return Xg.Compress((IntPtr)d, (uint)destinationFormat, (uint)destinationPitch, (uint)width, (uint)height, (IntPtr)s, (uint)sourceFormat, (uint)sourcePitch, alphaRef, (uint)flags);
        }

        public static void SetSurfaceHeader(IntPtr surface, int width, int height, SurfaceFormat format, int dataOffset, int pitch)
        {
            Xg.SurfaceHeader((uint)width, (uint)height, (uint)format, surface, (uint)dataOffset, (uint)pitch);
        }

        public static void SetTextureHeader(IntPtr texture, int width, int height, int levels, int usage, SurfaceFormat format, int pool, int dataOffset, int pitch)
        {
            Xg.TextureHeader((uint)width, (uint)height, (uint)levels, (uint)usage, (uint)format, (uint)pool, texture, (uint)dataOffset, (uint)pitch);
        }

        public static void SetCubeHeader(IntPtr texture, int edge, int levels, int usage, SurfaceFormat format, int pool, int dataOffset, int pitch)
        {
            Xg.CubeHeader((uint)edge, (uint)levels, (uint)usage, (uint)format, (uint)pool, texture, (uint)dataOffset, (uint)pitch);
        }

        public static void SetVolumeHeader(IntPtr texture, int width, int height, int depth, int levels, int usage, SurfaceFormat format, int pool, int dataOffset, int pitch)
        {
            Xg.VolumeHeader((uint)width, (uint)height, (uint)depth, (uint)levels, (uint)usage, (uint)format, (uint)pool, texture, (uint)dataOffset, (uint)pitch);
        }

        public static void SetVertexBufferHeader(IntPtr buffer, int length, int usage, VertexFormat format, int pool, int dataOffset)
        {
            Xg.VertexHeader((uint)length, (uint)usage, (uint)format, (uint)pool, buffer, (uint)dataOffset);
        }

        public static void SetIndexBufferHeader(IntPtr buffer, int length, int usage, SurfaceFormat format, int pool, int dataOffset)
        {
            Xg.IndexHeader((uint)length, (uint)usage, (uint)format, (uint)pool, buffer, (uint)dataOffset);
        }

        public static void SetPaletteHeader(IntPtr palette, int size, int dataOffset)
        {
            Xg.PaletteHeader((uint)size, palette, (uint)dataOffset);
        }

        public static void SetPushBufferHeader(IntPtr buffer, int size, bool cpuCopy, int dataOffset)
        {
            Xg.PushHeader((uint)size, cpuCopy ? 1 : 0, buffer, (uint)dataOffset);
        }

        public static void SetFixupHeader(IntPtr fixup, int size, int dataOffset)
        {
            Xg.FixupHeader((uint)size, fixup, (uint)dataOffset);
        }

        public static unsafe int SaveBitmap(Texture texture, string path)
        {
            return SaveBitmap(texture, 0, path);
        }

        public static unsafe int SaveBitmap(Texture texture, int level, string path)
        {
            if (texture == null)
                throw new ArgumentNullException("texture");
            if (path == null)
                throw new ArgumentNullException("path");
            byte[] ansi = ToAnsi(path);
            fixed (byte* p = ansi)
                return Xg.SaveBmp(texture.Handle, level, (IntPtr)p);
        }

        public static unsafe int SaveXpr(Texture texture, string path)
        {
            if (texture == null)
                throw new ArgumentNullException("texture");
            return SaveXpr(texture.Handle, path, false);
        }

        public static unsafe int SaveXpr(IntPtr resource, string path, bool surfaceAsTexture)
        {
            if (path == null)
                throw new ArgumentNullException("path");
            byte[] ansi = ToAnsi(path);
            fixed (byte* p = ansi)
                return Xg.SaveXpr(resource, (IntPtr)p, surfaceAsTexture ? 1 : 0);
        }

        public static unsafe ShaderCompile CompileShader(byte[] source, string entry, string target)
        {
            if (source == null)
                throw new ArgumentNullException("source");
            if (entry == null)
                throw new ArgumentNullException("entry");
            if (target == null)
                throw new ArgumentNullException("target");
            byte[] entryAnsi = ToAnsi(entry);
            byte[] targetAnsi = ToAnsi(target);
            IntPtr constants, shader, errors, listing, machine;
            int shaderType;
            int hr;
            fixed (byte* src = source)
            fixed (byte* en = entryAnsi)
            fixed (byte* tg = targetAnsi)
                hr = Xg.CompileShader(IntPtr.Zero, (IntPtr)src, source.Length, 0, (IntPtr)en, (IntPtr)tg,
                    out constants, out shader, out errors, out listing, out machine, out shaderType);
            ShaderCompile result = new ShaderCompile();
            result.Status = hr;
            result.ShaderType = shaderType;
            result.Shader = CopyBuffer(shader);
            result.Constants = CopyBuffer(constants);
            result.Errors = CopyText(errors);
            result.Listing = CopyText(listing);
            result.MachineListing = CopyText(machine);
            Xg.BufferRelease(constants);
            Xg.BufferRelease(shader);
            Xg.BufferRelease(errors);
            Xg.BufferRelease(listing);
            Xg.BufferRelease(machine);
            return result;
        }

        public static unsafe ShaderCompile AssembleShader(byte[] source)
        {
            if (source == null)
                throw new ArgumentNullException("source");
            IntPtr constants, shader, errors, listing;
            int shaderType;
            int hr;
            fixed (byte* src = source)
                hr = Xg.AssembleShader((IntPtr)src, source.Length, 0,
                    out constants, out shader, out errors, out listing, out shaderType);
            ShaderCompile result = new ShaderCompile();
            result.Status = hr;
            result.ShaderType = shaderType;
            result.Shader = CopyBuffer(shader);
            result.Constants = CopyBuffer(constants);
            result.Errors = CopyText(errors);
            result.Listing = CopyText(listing);
            Xg.BufferRelease(constants);
            Xg.BufferRelease(shader);
            Xg.BufferRelease(errors);
            Xg.BufferRelease(listing);
            return result;
        }

        public static unsafe int SpliceShaders(byte[] destination, byte[][] shaders, bool optimize, out int instructions)
        {
            if (destination == null)
                throw new ArgumentNullException("destination");
            if (shaders == null || shaders.Length == 0)
                throw new ArgumentNullException("shaders");
            GCHandle[] pins = new GCHandle[shaders.Length];
            IntPtr[] ptrs = new IntPtr[shaders.Length];
            try
            {
                for (int i = 0; i < shaders.Length; i++)
                {
                    if (shaders[i] == null)
                        throw new ArgumentNullException("shaders");
                    pins[i] = GCHandle.Alloc(shaders[i], GCHandleType.Pinned);
                    ptrs[i] = pins[i].AddrOfPinnedObject();
                }
                int size = destination.Length;
                int count = 0;
                int hr;
                fixed (byte* dest = destination)
                fixed (IntPtr* list = ptrs)
                    hr = Xg.Splice((IntPtr)dest, ref size, out count, (IntPtr)list, shaders.Length, optimize ? 1 : 0);
                instructions = count;
                if (hr < 0)
                    throw new InvalidOperationException("shader");
                return size;
            }
            finally
            {
                for (int i = 0; i < pins.Length; i++)
                    if (pins[i].IsAllocated)
                        pins[i].Free();
            }
        }

        public static int ShaderType(byte[] microcode)
        {
            return (int)WithCode(microcode, true);
        }

        public static int ShaderLength(byte[] microcode)
        {
            return (int)WithCode(microcode, false);
        }

        public static unsafe int CompareShaders(byte[] a, byte[] b, out string errors)
        {
            if (a == null)
                throw new ArgumentNullException("a");
            if (b == null)
                throw new ArgumentNullException("b");
            IntPtr log;
            int hr;
            fixed (byte* pa = a)
            fixed (byte* pb = b)
                hr = Xg.Compare((IntPtr)pa, (IntPtr)pb, out log);
            errors = CopyText(log);
            Xg.BufferRelease(log);
            return hr;
        }

        static unsafe uint WithCode(byte[] microcode, bool type)
        {
            if (microcode == null)
                throw new ArgumentNullException("microcode");
            fixed (byte* p = microcode)
                return type ? Xg.ShaderType((IntPtr)p) : Xg.ShaderLength((IntPtr)p);
        }

        static void Pin(byte[] source, byte[] destination)
        {
            if (source == null)
                throw new ArgumentNullException("source");
            if (destination == null)
                throw new ArgumentNullException("destination");
        }

        static unsafe byte[] CopyBuffer(IntPtr buffer)
        {
            if (buffer == IntPtr.Zero)
                return null;
            int n = (int)Xg.BufferSize(buffer);
            IntPtr p = Xg.BufferPointer(buffer);
            if (p == IntPtr.Zero || n <= 0)
                return null;
            byte[] data = new byte[n];
            byte* src = (byte*)p;
            for (int i = 0; i < n; i++)
                data[i] = src[i];
            return data;
        }

        static unsafe string CopyText(IntPtr buffer)
        {
            byte[] data = CopyBuffer(buffer);
            if (data == null)
                return null;
            int n = data.Length;
            while (n > 0 && data[n - 1] == 0)
                n--;
            char[] chars = new char[n];
            for (int i = 0; i < n; i++)
                chars[i] = (char)data[i];
            return new string(chars);
        }

        static byte[] ToAnsi(string text)
        {
            byte[] bytes = new byte[text.Length + 1];
            for (int i = 0; i < text.Length; i++)
                bytes[i] = text[i] < 128 ? (byte)text[i] : (byte)'?';
            return bytes;
        }

        static class Xg
        {
            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_xg_swizzled")]
            public static extern int Swizzled(uint format);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_xg_bytes_per_pixel")]
            public static extern uint BytesPerPixel(uint format);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_xg_swizzle")]
            public static extern void Swizzle(IntPtr source, uint pitch, IntPtr rect, IntPtr dest, int width, int height, IntPtr point, int bytesPerPixel);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_xg_swizzle")]
            public static extern void Swizzle(IntPtr source, uint pitch, ref Rect rect, IntPtr dest, int width, int height, ref Point point, int bytesPerPixel);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_xg_unswizzle")]
            public static extern void Unswizzle(IntPtr source, int width, int height, IntPtr rect, IntPtr dest, uint pitch, IntPtr point, int bytesPerPixel);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_xg_unswizzle")]
            public static extern void Unswizzle(IntPtr source, int width, int height, ref Rect rect, IntPtr dest, uint pitch, ref Point point, int bytesPerPixel);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_xg_swizzle_box")]
            public static extern void SwizzleBox(IntPtr source, uint rowPitch, uint slicePitch, ref Box box, IntPtr dest, int width, int height, int depth, ref Point3 point, int bytesPerPixel);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_xg_unswizzle_box")]
            public static extern void UnswizzleBox(IntPtr source, int width, int height, int depth, ref Box box, IntPtr dest, uint rowPitch, uint slicePitch, ref Point3 point, int bytesPerPixel);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_xg_compile_indexed")]
            public static extern int CompileIndexed(IntPtr buffer, ref int size, int type, int count, IntPtr indices);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_xg_compress")]
            public static extern int Compress(IntPtr dest, uint destFormat, uint destPitch, uint width, uint height, IntPtr source, uint sourceFormat, uint sourcePitch, float alphaRef, uint flags);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_xg_surface_header")]
            public static extern void SurfaceHeader(uint width, uint height, uint format, IntPtr surface, uint data, uint pitch);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_xg_texture_header")]
            public static extern void TextureHeader(uint width, uint height, uint levels, uint usage, uint format, uint pool, IntPtr texture, uint data, uint pitch);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_xg_cube_header")]
            public static extern void CubeHeader(uint edge, uint levels, uint usage, uint format, uint pool, IntPtr texture, uint data, uint pitch);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_xg_volume_header")]
            public static extern void VolumeHeader(uint width, uint height, uint depth, uint levels, uint usage, uint format, uint pool, IntPtr texture, uint data, uint pitch);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_xg_vertex_header")]
            public static extern void VertexHeader(uint length, uint usage, uint fvf, uint pool, IntPtr buffer, uint data);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_xg_index_header")]
            public static extern void IndexHeader(uint length, uint usage, uint format, uint pool, IntPtr buffer, uint data);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_xg_palette_header")]
            public static extern void PaletteHeader(uint size, IntPtr palette, uint data);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_xg_push_header")]
            public static extern void PushHeader(uint size, int cpuCopy, IntPtr buffer, uint data);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_xg_fixup_header")]
            public static extern void FixupHeader(uint size, IntPtr fixup, uint data);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_xg_save_bmp")]
            public static extern int SaveBmp(IntPtr texture, int level, IntPtr path);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_xg_save_xpr")]
            public static extern int SaveXpr(IntPtr resource, IntPtr path, int surfaceAsTexture);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_xg_compile_shader")]
            public static extern int CompileShader(IntPtr file, IntPtr source, int sourceLen, uint flags, IntPtr entry, IntPtr target, out IntPtr constants, out IntPtr shader, out IntPtr errors, out IntPtr listing, out IntPtr machine, out int shaderType);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_xg_assemble_shader")]
            public static extern int AssembleShader(IntPtr source, int sourceLen, uint flags, out IntPtr constants, out IntPtr shader, out IntPtr errors, out IntPtr listing, out int shaderType);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_xg_splice")]
            public static extern int Splice(IntPtr shader, ref int size, out int instructions, IntPtr shaders, int count, int optimize);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_xg_shader_type")]
            public static extern uint ShaderType(IntPtr microcode);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_xg_shader_length")]
            public static extern uint ShaderLength(IntPtr microcode);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_xg_compare_shaders")]
            public static extern int Compare(IntPtr a, IntPtr b, out IntPtr errorLog);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_xg_buffer_pointer")]
            public static extern IntPtr BufferPointer(IntPtr buffer);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_xg_buffer_size")]
            public static extern uint BufferSize(IntPtr buffer);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_xg_buffer_release")]
            public static extern void BufferRelease(IntPtr buffer);
        }
    }
}
