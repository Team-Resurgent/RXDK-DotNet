// Managed Xbox graphics over libd3d8 and libxgraphics. The device is the one
// static NV2A device. Vertex buffers, index buffers, and textures are objects.
// Open() walks Display.Defaults and takes the first mode the console supports.
// Delete a line from that array to leave a mode out. The last line is the fallback.
using System;
using System.Runtime.InteropServices;

namespace Rxdk
{
    public struct DisplayMode
    {
        public int Width;
        public int Height;
        public bool Progressive;
        public bool Widescreen;
        public int RefreshHz;

        public DisplayMode(int width, int height, bool progressive, bool widescreen, int refreshHz)
        {
            Width = width;
            Height = height;
            Progressive = progressive;
            Widescreen = widescreen;
            RefreshHz = refreshHz;
        }
    }

    public static class Display
    {
        // First supported entry wins. The last entry is used when nothing earlier
        // matches. Delete a line to drop that mode (1920x1080 included).
        public static DisplayMode[] Defaults = new DisplayMode[] {
            new DisplayMode(1920, 1080, false, true, 60),
            new DisplayMode(1280, 720, true, true, 60),
            new DisplayMode(720, 480, true, true, 60),
            new DisplayMode(640, 480, true, true, 60),
            new DisplayMode(720, 480, true, false, 60),
            new DisplayMode(640, 480, true, false, 60),
            new DisplayMode(720, 480, false, true, 50),
            new DisplayMode(640, 480, false, true, 50),
            new DisplayMode(720, 480, false, false, 50),
            new DisplayMode(640, 480, false, false, 50),
            new DisplayMode(720, 480, false, true, 60),
            new DisplayMode(640, 480, false, true, 60),
            new DisplayMode(720, 480, false, false, 60),
            new DisplayMode(640, 480, false, false, 60)
        };

        const int StandardPalI = 3;
        const int FlagWidescreen = 0x01;
        const int Flag720p = 0x02;
        const int Flag1080i = 0x04;
        const int Flag480p = 0x08;
        const int FlagPal60 = 0x40;

        internal static bool Supports(DisplayMode mode, int standard, int flags)
        {
            if (mode.RefreshHz == 60 && (flags & FlagPal60) == 0 && standard == StandardPalI)
                return false;
            if (mode.RefreshHz == 50 && standard != StandardPalI)
                return false;
            if (mode.Height == 480 && mode.Widescreen && (flags & FlagWidescreen) == 0)
                return false;
            if (mode.Height == 480 && mode.Progressive && (flags & Flag480p) == 0)
                return false;
            if (mode.Height == 720 && (flags & Flag720p) == 0)
                return false;
            if (mode.Height == 1080 && (flags & Flag1080i) == 0)
                return false;
            return true;
        }
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Matrix4
    {
        public float M11, M12, M13, M14;
        public float M21, M22, M23, M24;
        public float M31, M32, M33, M34;
        public float M41, M42, M43, M44;
    }

    [Flags]
    public enum VertexFormat : uint
    {
        Position = 0x002,
        Rhw = 0x004,
        Blend1 = 0x006,
        Blend2 = 0x008,
        Blend3 = 0x00a,
        Blend4 = 0x00c,
        Blend5 = 0x00e,
        Normal = 0x010,
        Diffuse = 0x040,
        Specular = 0x080,
        Tex1 = 0x100,
        Tex2 = 0x200,
        Tex3 = 0x300,
        Tex4 = 0x400
    }

    [Flags]
    public enum ClearFlags : uint
    {
        Depth = 0x00000001,
        Stencil = 0x00000002,
        TargetRed = 0x00000010,
        TargetGreen = 0x00000020,
        TargetBlue = 0x00000040,
        TargetAlpha = 0x00000080,
        Target = 0x000000f0
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Viewport
    {
        public uint X, Y, Width, Height;
        public float MinZ, MaxZ;
    }

    public enum PrimitiveType
    {
        PointList = 1,
        LineList = 2,
        LineLoop = 3,
        LineStrip = 4,
        TriangleList = 5,
        TriangleStrip = 6,
        TriangleFan = 7
    }

    public enum RenderState
    {
        ZFunc = 57,
        AlphaFunc = 58,
        AlphaBlendEnable = 59,
        AlphaTestEnable = 60,
        AlphaRef = 61,
        SrcBlend = 62,
        DestBlend = 63,
        ZWriteEnable = 64,
        FogEnable = 92,
        Lighting = 102,
        ColorVertex = 105,
        Ambient = 115,
        FillMode = 139,
        ZEnable = 143,
        StencilEnable = 144,
        CullMode = 147
    }

    public enum Cull
    {
        None = 0,
        Clockwise = 0x900,
        CounterClockwise = 0x901
    }

    public enum Fill
    {
        Point = 0x1b00,
        Wireframe = 0x1b01,
        Solid = 0x1b02
    }

    public enum SurfaceFormat
    {
        Unknown = 0,
        A8R8G8B8 = 6,
        X8R8G8B8 = 7,
        LinearA8R8G8B8 = 0x12,
        LinearX8R8G8B8 = 0x1E
    }

    public static class GraphicsDevice
    {
        static DisplayMode mode;
        static bool open;

        public static int LastStatus;

        public static int Width { get { return mode.Width; } }
        public static int Height { get { return mode.Height; } }
        public static bool Progressive { get { return mode.Progressive; } }
        public static bool Widescreen { get { return mode.Widescreen; } }
        public static int RefreshHz { get { return mode.RefreshHz; } }

        public static bool Open()
        {
            if (open)
                return true;
            DisplayMode[] modes = Display.Defaults;
            if (modes == null || modes.Length == 0)
            {
                LastStatus = -1;
                return false;
            }
            int standard = (int)GfxNative.VideoStandard();
            int flags = (int)GfxNative.VideoFlags();
            int chosen = -1;
            int last = modes.Length - 1;
            for (int i = 0; i < last; i++)
            {
                if (Display.Supports(modes[i], standard, flags))
                {
                    chosen = i;
                    break;
                }
            }
            if (chosen < 0)
                chosen = last;
            DisplayMode picked = modes[chosen];
            int hr = GfxNative.Open(picked.Width, picked.Height, picked.Progressive ? 1 : 0,
                picked.Widescreen ? 1 : 0, picked.RefreshHz);
            LastStatus = hr;
            if (hr != 0)
                return false;
            mode = picked;
            open = true;
            return true;
        }

        public static void Clear(ClearFlags flags, byte r, byte g, byte b, float depth, int stencil)
        {
            GfxNative.Clear((uint)flags, (uint)(0xFF000000 | (r << 16) | (g << 8) | b), depth, (uint)stencil);
        }

        public static void Clear(byte r, byte g, byte b)
        {
            Clear(ClearFlags.Target | ClearFlags.Depth, r, g, b, 1f, 0);
        }

        public static Matrix4 Model
        {
            get { return GetMatrix(6); }
            set { GfxNative.Model(ref value); }
        }

        public static Matrix4 View
        {
            get { return GetMatrix(0); }
            set { GfxNative.View(ref value); }
        }

        public static Matrix4 Projection
        {
            get { return GetMatrix(1); }
            set { GfxNative.Projection(ref value); }
        }

        public static Viewport Viewport
        {
            get
            {
                Viewport viewport = new Viewport();
                GfxNative.GetViewport(ref viewport);
                return viewport;
            }
            set { GfxNative.SetViewport(ref value); }
        }

        public static void SetRenderState(RenderState state, int value)
        {
            GfxNative.RenderState((int)state, (uint)value);
        }

        public static int GetRenderState(RenderState state)
        {
            return (int)GfxNative.GetRenderState((int)state);
        }

        static Matrix4 GetMatrix(int state)
        {
            Matrix4 matrix = new Matrix4();
            GfxNative.GetMatrix(state, ref matrix);
            return matrix;
        }

        public static void SetVertexFormat(VertexFormat format)
        {
            GfxNative.SetFvf((uint)format);
        }

        public static int VertexShader
        {
            get { return (int)GfxNative.GetVertexShader(); }
            set { GfxNative.SetFvf((uint)value); }
        }

        public static int CreateVertexShader(byte[] declaration, byte[] function)
        {
            return CreateVertexShader(declaration, function, 0);
        }

        public static unsafe int CreateVertexShader(byte[] declaration, byte[] function, int usage)
        {
            if (function == null)
                throw new ArgumentNullException("function");
            uint handle;
            int status;
            fixed (byte* program = function)
            {
                if (declaration == null || declaration.Length == 0)
                    status = GfxNative.CreateVertexShader(IntPtr.Zero, (IntPtr)program, (uint)usage, out handle);
                else
                {
                    fixed (byte* decl = declaration)
                        status = GfxNative.CreateVertexShader((IntPtr)decl, (IntPtr)program, (uint)usage, out handle);
                }
            }
            if (status < 0)
                throw new InvalidOperationException("CreateVertexShader");
            return (int)handle;
        }

        public static void DeleteVertexShader(int handle)
        {
            GfxNative.DeleteVertexShader((uint)handle);
        }

        public static void LoadVertexShader(int handle, int address)
        {
            GfxNative.LoadVertexShader((uint)handle, (uint)address);
        }

        public static unsafe void LoadVertexShaderProgram(byte[] function, int address)
        {
            if (function == null)
                throw new ArgumentNullException("function");
            fixed (byte* program = function)
                GfxNative.LoadVertexShaderProgram((IntPtr)program, (uint)address);
        }

        public static void SelectVertexShader(int handle, int address)
        {
            GfxNative.SelectVertexShader((uint)handle, (uint)address);
        }

        public static unsafe void SetVertexShaderConstant(int register, float[] values)
        {
            if (values == null)
                throw new ArgumentNullException("values");
            if ((values.Length & 3) != 0)
                throw new ArgumentException("values");
            fixed (float* data = values)
                GfxNative.SetVertexShaderConstant(register, (IntPtr)data, values.Length / 4);
        }

        public static unsafe void GetVertexShaderConstant(int register, float[] values)
        {
            if (values == null)
                throw new ArgumentNullException("values");
            if ((values.Length & 3) != 0)
                throw new ArgumentException("values");
            fixed (float* data = values)
                GfxNative.GetVertexShaderConstant(register, (IntPtr)data, values.Length / 4);
        }

        public static int PixelShader
        {
            get { return (int)GfxNative.GetPixelShader(); }
            set { GfxNative.SetPixelShader((uint)value); }
        }

        public static unsafe int CreatePixelShader(byte[] shader)
        {
            if (shader == null)
                throw new ArgumentNullException("shader");
            uint handle;
            fixed (byte* program = shader)
                handle = GfxNative.CreatePixelShader((IntPtr)program);
            if (handle == 0)
                throw new InvalidOperationException("CreatePixelShader");
            return (int)handle;
        }

        public static void DeletePixelShader(int handle)
        {
            GfxNative.DeletePixelShader((uint)handle);
        }

        public static unsafe void SetPixelShaderProgram(byte[] shader)
        {
            if (shader == null)
                throw new ArgumentNullException("shader");
            fixed (byte* program = shader)
                GfxNative.SetPixelShaderProgram((IntPtr)program);
        }

        public static unsafe void SetPixelShaderConstant(int register, float[] values)
        {
            if (values == null)
                throw new ArgumentNullException("values");
            if ((values.Length & 3) != 0)
                throw new ArgumentException("values");
            fixed (float* data = values)
                GfxNative.SetPixelShaderConstant((uint)register, (IntPtr)data, values.Length / 4);
        }

        public static unsafe void GetPixelShaderConstant(int register, float[] values)
        {
            if (values == null)
                throw new ArgumentNullException("values");
            if ((values.Length & 3) != 0)
                throw new ArgumentException("values");
            fixed (float* data = values)
                GfxNative.GetPixelShaderConstant((uint)register, (IntPtr)data, values.Length / 4);
        }

        public static void SetVertexBuffer(VertexBuffer buffer, int stride)
        {
            if (buffer == null)
                throw new ArgumentNullException("buffer");
            GfxNative.SetStream(buffer.Handle, stride);
        }

        public static void SetIndexBuffer(IndexBuffer buffer)
        {
            GfxNative.SetIndices(buffer == null ? IntPtr.Zero : buffer.Handle);
        }

        public static void SetTexture(int stage, Texture texture)
        {
            GfxNative.SetTexture(stage, texture == null ? IntPtr.Zero : texture.Handle);
        }

        public static void SetTexture(int stage, CubeTexture texture)
        {
            GfxNative.SetTexture(stage, texture == null ? IntPtr.Zero : texture.Handle);
        }

        public static void SetTexture(int stage, VolumeTexture texture)
        {
            GfxNative.SetTexture(stage, texture == null ? IntPtr.Zero : texture.Handle);
        }

        public static void DrawPrimitive(PrimitiveType type, int startVertex, int primitiveCount)
        {
            GfxNative.Draw((int)type, startVertex, primitiveCount);
        }

        public static void DrawIndexedPrimitive(PrimitiveType type, int startIndex, int primitiveCount)
        {
            GfxNative.DrawIndexed((int)type, startIndex, primitiveCount);
        }

        public static void Present()
        {
            GfxNative.Present();
        }
    }

    internal static class GfxNative
        {
            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_gfx_video_flags")]
            public static extern uint VideoFlags();

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_gfx_video_standard")]
            public static extern uint VideoStandard();

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_gfx_open")]
            public static extern int Open(int width, int height, int progressive, int widescreen, int refreshHz);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_gfx_clear")]
            public static extern void Clear(uint flags, uint color, float depth, uint stencil);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_gfx_get_matrix")]
            public static extern void GetMatrix(int state, ref Matrix4 matrix);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_gfx_set_viewport")]
            public static extern void SetViewport(ref Viewport viewport);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_gfx_get_viewport")]
            public static extern void GetViewport(ref Viewport viewport);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_gfx_get_render_state")]
            public static extern uint GetRenderState(int state);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_gfx_model")]
            public static extern void Model(ref Matrix4 matrix);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_gfx_view")]
            public static extern void View(ref Matrix4 matrix);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_gfx_projection")]
            public static extern void Projection(ref Matrix4 matrix);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_gfx_render_state")]
            public static extern void RenderState(int state, uint value);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_gfx_set_stream")]
            public static extern void SetStream(IntPtr buffer, int stride);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_gfx_set_indices")]
            public static extern void SetIndices(IntPtr buffer);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_gfx_set_fvf")]
            public static extern void SetFvf(uint fvf);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_gfx_create_vertex_shader")]
            public static extern int CreateVertexShader(IntPtr declaration, IntPtr function, uint usage, out uint handle);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_gfx_get_vertex_shader")]
            public static extern uint GetVertexShader();

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_gfx_delete_vertex_shader")]
            public static extern void DeleteVertexShader(uint handle);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_gfx_set_vs_constant")]
            public static extern void SetVertexShaderConstant(int register, IntPtr data, int count);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_gfx_get_vs_constant")]
            public static extern void GetVertexShaderConstant(int register, IntPtr data, int count);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_gfx_load_vertex_shader")]
            public static extern void LoadVertexShader(uint handle, uint address);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_gfx_load_vertex_shader_program")]
            public static extern void LoadVertexShaderProgram(IntPtr function, uint address);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_gfx_select_vertex_shader")]
            public static extern void SelectVertexShader(uint handle, uint address);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_gfx_create_pixel_shader")]
            public static extern uint CreatePixelShader(IntPtr shader);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_gfx_set_pixel_shader")]
            public static extern void SetPixelShader(uint handle);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_gfx_get_pixel_shader")]
            public static extern uint GetPixelShader();

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_gfx_delete_pixel_shader")]
            public static extern void DeletePixelShader(uint handle);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_gfx_set_ps_constant")]
            public static extern void SetPixelShaderConstant(uint register, IntPtr data, int count);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_gfx_get_ps_constant")]
            public static extern void GetPixelShaderConstant(uint register, IntPtr data, int count);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_gfx_set_pixel_shader_program")]
            public static extern void SetPixelShaderProgram(IntPtr shader);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_gfx_set_texture")]
            public static extern void SetTexture(int stage, IntPtr texture);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_gfx_draw")]
            public static extern void Draw(int type, int startVertex, int primitiveCount);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_gfx_draw_indexed")]
            public static extern void DrawIndexed(int type, int startIndex, int primitiveCount);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_gfx_create_vertex_buffer")]
            public static extern IntPtr CreateVertexBuffer(int size);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_gfx_vertex_write")]
            public static extern void VertexWrite(IntPtr buffer, IntPtr src, int offset, int size);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_gfx_create_index_buffer")]
            public static extern IntPtr CreateIndexBuffer(int size);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_gfx_index_write")]
            public static extern void IndexWrite(IntPtr buffer, IntPtr src, int offset, int size);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_gfx_create_texture")]
            public static extern IntPtr CreateTexture(int width, int height, int format);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_gfx_texture_from_memory_ex")]
            public static extern IntPtr TextureFromMemoryEx(IntPtr data, int size, int width, int height, int mipLevels, int format, out int outWidth, out int outHeight, out int outFormat);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_gfx_texture_from_file_ex")]
            public static extern IntPtr TextureFromFileEx(IntPtr path, int width, int height, int mipLevels, int format, out int outWidth, out int outHeight, out int outFormat);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_gfx_cube_from_memory")]
            public static extern IntPtr CubeFromMemory(IntPtr data, int size);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_gfx_cube_from_file")]
            public static extern IntPtr CubeFromFile(IntPtr path);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_gfx_volume_create")]
            public static extern IntPtr VolumeCreate(int width, int height, int depth, int levels, int format);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_gfx_texture_write")]
            public static extern void TextureWrite(IntPtr texture, IntPtr src, int width, int height, int bytesPerPixel, int linear);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_gfx_release")]
            public static extern void Release(IntPtr resource);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_gfx_present")]
            public static extern void Present();

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_gfx_close")]
            public static extern void Close();
    }

    public sealed class VertexBuffer : IDisposable
    {
        readonly IntPtr handle;
        bool disposed;

        VertexBuffer(IntPtr handle) { this.handle = handle; }

        internal IntPtr Handle { get { return handle; } }

        public static VertexBuffer Create(int sizeInBytes)
        {
            if (sizeInBytes <= 0)
                throw new ArgumentOutOfRangeException("sizeInBytes");
            IntPtr handle = GfxNative.CreateVertexBuffer(sizeInBytes);
            if (handle == IntPtr.Zero)
                throw new OutOfMemoryException();
            return new VertexBuffer(handle);
        }

        public unsafe void SetData(byte[] data)
        {
            if (disposed)
                throw new ObjectDisposedException("VertexBuffer");
            if (data == null)
                throw new ArgumentNullException("data");
            fixed (byte* p = data)
                GfxNative.VertexWrite(handle, (IntPtr)p, 0, data.Length);
        }

        public void Dispose()
        {
            if (disposed)
                return;
            disposed = true;
            GfxNative.Release(handle);
            GC.SuppressFinalize(this);
        }

        ~VertexBuffer() { Dispose(); }
    }

    public sealed class IndexBuffer : IDisposable
    {
        readonly IntPtr handle;
        bool disposed;

        IndexBuffer(IntPtr handle) { this.handle = handle; }

        internal IntPtr Handle { get { return handle; } }

        public static IndexBuffer Create(int sizeInBytes)
        {
            if (sizeInBytes <= 0)
                throw new ArgumentOutOfRangeException("sizeInBytes");
            IntPtr handle = GfxNative.CreateIndexBuffer(sizeInBytes);
            if (handle == IntPtr.Zero)
                throw new OutOfMemoryException();
            return new IndexBuffer(handle);
        }

        public unsafe void SetData(byte[] data)
        {
            if (disposed)
                throw new ObjectDisposedException("IndexBuffer");
            if (data == null)
                throw new ArgumentNullException("data");
            fixed (byte* p = data)
                GfxNative.IndexWrite(handle, (IntPtr)p, 0, data.Length);
        }

        public void Dispose()
        {
            if (disposed)
                return;
            disposed = true;
            GfxNative.Release(handle);
            GC.SuppressFinalize(this);
        }

        ~IndexBuffer() { Dispose(); }
    }

    public sealed class Texture : IDisposable
    {
        readonly IntPtr handle;
        readonly int width, height, bytesPerPixel, linear;
        bool disposed;

        Texture(IntPtr handle, int width, int height, int bytesPerPixel, int linear)
        {
            this.handle = handle;
            this.width = width;
            this.height = height;
            this.bytesPerPixel = bytesPerPixel;
            this.linear = linear;
        }

        internal IntPtr Handle { get { return handle; } }
        public int Width { get { return width; } }
        public int Height { get { return height; } }

        public static Texture Create(int width, int height, SurfaceFormat format)
        {
            if (width <= 0 || height <= 0)
                throw new ArgumentOutOfRangeException("width");
            IntPtr handle = GfxNative.CreateTexture(width, height, (int)format);
            if (handle == IntPtr.Zero)
                throw new OutOfMemoryException();
            int linear = format == SurfaceFormat.LinearA8R8G8B8 || format == SurfaceFormat.LinearX8R8G8B8 ? 1 : 0;
            return new Texture(handle, width, height, 4, linear);
        }

        public static Texture FromMemory(byte[] data)
        {
            if (data == null)
                throw new ArgumentNullException("data");
            return FromMemory(data, 0, 0, 0, SurfaceFormat.Unknown);
        }

        public static Texture FromFile(string path)
        {
            if (path == null)
                throw new ArgumentNullException("path");
            return FromFile(path, 0, 0, 0, SurfaceFormat.Unknown);
        }

        public unsafe static Texture FromMemory(byte[] data, int width, int height, int mipLevels, SurfaceFormat format)
        {
            if (data == null)
                throw new ArgumentNullException("data");
            int outWidth, outHeight, outFormat;
            fixed (byte* p = data)
            {
                IntPtr handle = GfxNative.TextureFromMemoryEx(
                    (IntPtr)p, data.Length, DefaultSize(width), DefaultSize(height), DefaultSize(mipLevels),
                    (int)format, out outWidth, out outHeight, out outFormat);
                return Adopt(handle, outWidth, outHeight, outFormat);
            }
        }

        public unsafe static Texture FromFile(string path, int width, int height, int mipLevels, SurfaceFormat format)
        {
            if (path == null)
                throw new ArgumentNullException("path");
            byte[] ansi = ToAnsi(path);
            int outWidth, outHeight, outFormat;
            fixed (byte* p = ansi)
            {
                IntPtr handle = GfxNative.TextureFromFileEx(
                    (IntPtr)p, DefaultSize(width), DefaultSize(height), DefaultSize(mipLevels),
                    (int)format, out outWidth, out outHeight, out outFormat);
                return Adopt(handle, outWidth, outHeight, outFormat);
            }
        }

        static int DefaultSize(int value)
        {
            return value <= 0 ? -1 : value;
        }

        static Texture Adopt(IntPtr handle, int width, int height, int format)
        {
            if (handle == IntPtr.Zero)
                throw new InvalidOperationException("texture");
            int linear = format == (int)SurfaceFormat.LinearA8R8G8B8 || format == (int)SurfaceFormat.LinearX8R8G8B8 ? 1 : 0;
            int bpp = format == (int)SurfaceFormat.A8R8G8B8 || format == (int)SurfaceFormat.X8R8G8B8 || linear != 0 ? 4 : 0;
            return new Texture(handle, width, height, bpp, linear);
        }

        static byte[] ToAnsi(string text)
        {
            byte[] bytes = new byte[text.Length + 1];
            for (int i = 0; i < text.Length; i++)
                bytes[i] = text[i] < 128 ? (byte)text[i] : (byte)'?';
            return bytes;
        }

        public unsafe void SetData(byte[] pixels)
        {
            if (disposed)
                throw new ObjectDisposedException("Texture");
            if (pixels == null)
                throw new ArgumentNullException("pixels");
            if (bytesPerPixel == 0)
                throw new InvalidOperationException("texture");
            fixed (byte* p = pixels)
                GfxNative.TextureWrite(handle, (IntPtr)p, width, height, bytesPerPixel, linear);
        }

        public void Dispose()
        {
            if (disposed)
                return;
            disposed = true;
            GfxNative.Release(handle);
            GC.SuppressFinalize(this);
        }

        ~Texture() { Dispose(); }
    }
}
