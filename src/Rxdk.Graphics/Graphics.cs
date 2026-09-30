// Managed Xbox graphics over libd3d8 and libxgraphics. The device is the one
// static NV2A device. Vertex buffers, index buffers, and textures are objects.
// Open() walks Display.Defaults and takes the first mode the console supports
// that fits within Display.MaxWidth x Display.MaxHeight. The last line is the fallback.
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
        // Set before the device opens to keep larger modes out, e.g. MaxHeight = 720
        // for a title whose fill rate or memory cannot afford 1080i.
        public static int MaxWidth = int.MaxValue;
        public static int MaxHeight = int.MaxValue;

        // First supported entry within the maximum wins. The last entry is used when
        // nothing earlier matches.
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
        DitherEnable = 65,
        ShadeMode = 66,
        ColorWriteEnable = 67,
        StencilZFail = 68,
        StencilPass = 69,
        StencilFunc = 70,
        StencilRef = 71,
        StencilMask = 72,
        StencilWriteMask = 73,
        BlendOp = 74,
        BlendColor = 75,
        /// <summary>Depth bias, as a slope-scaled factor and a constant offset.</summary>
        PolygonOffsetZSlopeScale = 77,
        PolygonOffsetZOffset = 78,
        SolidOffsetEnable = 81,
        FogEnable = 92,
        Lighting = 102,
        ColorVertex = 105,
        Ambient = 115,
        FillMode = 139,
        ZEnable = 143,
        StencilEnable = 144,
        StencilFail = 145,
        CullMode = 147,
        TextureFactor = 148,
        MultiSampleAntiAlias = 152
    }

    /// <summary>
    /// Which channels a draw is allowed to write, for RenderState.ColorWriteEnable. The console
    /// spends a whole byte per channel rather than one bit, unlike desktop Direct3D 8.
    /// </summary>
    [Flags]
    public enum ColorWriteEnable
    {
        Blue = 1 << 0,
        Green = 1 << 8,
        Red = 1 << 16,
        Alpha = 1 << 24,
        All = Blue | Green | Red | Alpha
    }

    /// <summary>
    /// Per-stage texture state. Direct3D 8 has no separate sampler object: filtering and
    /// addressing are stage states alongside the fixed-function blend controls.
    /// </summary>
    public enum TextureStageState
    {
        AddressU = 0,
        AddressV = 1,
        AddressW = 2,
        MagFilter = 3,
        MinFilter = 4,
        MipFilter = 5,
        MipMapLodBias = 6,
        MaxMipLevel = 7,
        MaxAnisotropy = 8,
        ColorOp = 12,
        ColorArg0 = 13,
        ColorArg1 = 14,
        ColorArg2 = 15,
        AlphaOp = 16,
        AlphaArg0 = 17,
        AlphaArg1 = 18,
        AlphaArg2 = 19,
        ResultArg = 20,
        TextureTransformFlags = 21,
        TexCoordIndex = 28,
        BorderColor = 29
    }

    public enum TextureFilter
    {
        /// <summary>Valid for <see cref="TextureStageState.MipFilter"/> only.</summary>
        None = 0,
        Point = 1,
        Linear = 2,
        Anisotropic = 3
    }

    public enum TextureAddress
    {
        Wrap = 1,
        Mirror = 2,
        Clamp = 3,
        Border = 4,
        ClampToEdge = 5
    }

    public enum TextureOp
    {
        Disable = 1,
        SelectArg1 = 2,
        SelectArg2 = 3,
        Modulate = 4,
        Add = 7
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
        /// <summary>
        /// The DXT block formats. The GPU samples these directly, so compressed content stays
        /// compressed in video memory. DXT2 and DXT4 share their values with DXT3 and DXT5; the
        /// difference is only whether the colour is premultiplied, which the sampler does not care
        /// about.
        /// </summary>
        Dxt1 = 0x0C,
        Dxt3 = 0x0E,
        Dxt5 = 0x0F,
        LinearA8R8G8B8 = 0x12,
        LinearX8R8G8B8 = 0x1E,
        /// <summary>Depth formats, for a depth stencil surface rather than a texture.</summary>
        Depth24Stencil8 = 0x2A,
        Depth16 = 0x2C,
        LinearDepth24Stencil8 = 0x2E,
        LinearDepth16 = 0x30
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
                if (modes[i].Width <= Display.MaxWidth && modes[i].Height <= Display.MaxHeight &&
                    Display.Supports(modes[i], standard, flags))
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

        /// <summary>Passing null unbinds the stream.</summary>
        public static void SetVertexBuffer(VertexBuffer buffer, int stride)
        {
            GfxNative.SetStream(buffer == null ? IntPtr.Zero : buffer.Handle, stride);
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

        public static void DrawIndexedPrimitive(PrimitiveType type, int baseVertex, int startIndex, int primitiveCount)
        {
            GfxNative.DrawIndexed((int)type, baseVertex, startIndex, primitiveCount);
        }

        public static void SetTextureStageState(int stage, TextureStageState state, int value)
        {
            GfxNative.TextureStageState(stage, (int)state, (uint)value);
        }

        public static int GetTextureStageState(int stage, TextureStageState state)
        {
            return (int)GfxNative.GetTextureStageState(stage, (int)state);
        }

        /// <summary>
        /// Direct3D 8 on Xbox has no scene brackets; BeginScene and EndScene are empty inline
        /// functions in the SDK. These exist so callers written against the desktop API work.
        /// </summary>
        public static void BeginScene() { }

        public static void EndScene() { }

        /// <summary>
        /// Waits for the GPU to drain the push buffer. Needed before reading a surface the GPU
        /// was drawing into, since the GPU runs a whole push buffer behind the CPU.
        /// </summary>
        public static void BlockUntilIdle()
        {
            GfxNative.BlockUntilIdle();
        }

        /// <summary>
        /// Binds a colour and depth surface. Passing null for either restores nothing: the
        /// caller keeps the previous surfaces from <see cref="GetRenderTarget"/> and rebinds them.
        /// </summary>
        public static void SetRenderTarget(Surface color, Surface depthStencil)
        {
            GfxNative.SetRenderTarget(
                color == null ? IntPtr.Zero : color.Handle,
                depthStencil == null ? IntPtr.Zero : depthStencil.Handle);
        }

        public static Surface GetRenderTarget()
        {
            return Surface.Adopt(GfxNative.GetRenderTarget());
        }

        public static Surface GetDepthStencil()
        {
            return Surface.Adopt(GfxNative.GetDepthStencil());
        }

        public static void Present()
        {
            GfxNative.Present();
        }
    }

    /// <summary>
    /// A render target, depth buffer, or one mip level of a texture. The back buffer and a
    /// texture's own levels are owned elsewhere, so <see cref="Dispose"/> only releases
    /// surfaces this class created.
    /// </summary>
    public sealed class Surface : IDisposable
    {
        readonly IntPtr handle;
        readonly bool owned;
        bool disposed;

        Surface(IntPtr handle, bool owned)
        {
            this.handle = handle;
            this.owned = owned;
        }

        internal IntPtr Handle { get { return handle; } }

        internal static Surface Adopt(IntPtr handle)
        {
            return handle == IntPtr.Zero ? null : new Surface(handle, false);
        }

        public static Surface CreateRenderTarget(int width, int height, SurfaceFormat format)
        {
            return Create(GfxNative.CreateRenderTarget(width, height, (int)format));
        }

        public static Surface CreateDepthStencil(int width, int height, SurfaceFormat format)
        {
            return Create(GfxNative.CreateDepthStencil(width, height, (int)format));
        }

        /// <summary>One mip level of a texture, for drawing into it as a render target.</summary>
        public static Surface FromTexture(Texture texture, int level)
        {
            if (texture == null)
                throw new ArgumentNullException("texture");
            return Adopt(GfxNative.TextureSurface(texture.Handle, level));
        }

        static Surface Create(IntPtr handle)
        {
            if (handle == IntPtr.Zero)
                throw new OutOfMemoryException();
            return new Surface(handle, true);
        }

        public void Dispose()
        {
            if (disposed)
                return;
            disposed = true;
            if (owned)
                GfxNative.Release(handle);
            GC.SuppressFinalize(this);
        }

        ~Surface() { Dispose(); }
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

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_gfx_texture_stage_state")]
            public static extern void TextureStageState(int stage, int state, uint value);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_gfx_get_texture_stage_state")]
            public static extern uint GetTextureStageState(int stage, int state);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_gfx_create_render_target")]
            public static extern IntPtr CreateRenderTarget(int width, int height, int format);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_gfx_create_depth_stencil")]
            public static extern IntPtr CreateDepthStencil(int width, int height, int format);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_gfx_get_render_target")]
            public static extern IntPtr GetRenderTarget();

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_gfx_get_depth_stencil")]
            public static extern IntPtr GetDepthStencil();

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_gfx_set_render_target")]
            public static extern void SetRenderTarget(IntPtr color, IntPtr depthStencil);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_gfx_texture_surface")]
            public static extern IntPtr TextureSurface(IntPtr texture, int level);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_gfx_block_until_idle")]
            public static extern void BlockUntilIdle();

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
            public static extern void DrawIndexed(int type, int baseVertex, int startIndex, int primitiveCount);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_gfx_create_vertex_buffer")]
            public static extern IntPtr CreateVertexBuffer(int size);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_gfx_vertex_write")]
            public static extern void VertexWrite(IntPtr buffer, IntPtr src, int offset, int size);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_gfx_vertex_write_ex")]
            public static extern void VertexWriteEx(IntPtr buffer, IntPtr src, int offset, int size, uint flags);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_gfx_create_index_buffer")]
            public static extern IntPtr CreateIndexBuffer(int size);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_gfx_index_write")]
            public static extern void IndexWrite(IntPtr buffer, IntPtr src, int offset, int size);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_gfx_create_texture")]
            public static extern IntPtr CreateTexture(int width, int height, int format);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_gfx_create_texture_ex")]
            public static extern IntPtr CreateTextureEx(int width, int height, int levels, int format);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_gfx_texture_write_level")]
            public static extern void TextureWriteLevel(IntPtr texture, int level, IntPtr src, int width, int height, int bytesPerPixel, int linear);

            [DllImport("d3d8", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "rxdk_gfx_texture_read_level")]
            public static extern void TextureReadLevel(IntPtr texture, int level, IntPtr dest, int width, int height, int bytesPerPixel, int linear);

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
            if (data == null)
                throw new ArgumentNullException("data");
            SetData(data, 0, 0, data.Length);
        }

        /// <summary>Writes part of the buffer, which is what a dynamic buffer wants.</summary>
        public unsafe void SetData(byte[] data, int offsetInBytes, int startIndex, int count)
        {
            if (disposed)
                throw new ObjectDisposedException("VertexBuffer");
            if (data == null)
                throw new ArgumentNullException("data");
            if (offsetInBytes < 0 || startIndex < 0 || count < 0 || startIndex + count > data.Length)
                throw new ArgumentOutOfRangeException("count");
            fixed (byte* p = data)
                GfxNative.VertexWrite(handle, (IntPtr)(p + startIndex), offsetInBytes, count);
        }

        public void SetData(IntPtr data, int offsetInBytes, int count)
        {
            SetData(data, offsetInBytes, count, false);
        }

        /// <summary>
        /// noOverwrite skips waiting for the GPU to finish with the buffer. Only pass it when the
        /// range written is not used by any draw already submitted, as when appending to a ring.
        /// </summary>
        public void SetData(IntPtr data, int offsetInBytes, int count, bool noOverwrite)
        {
            if (disposed)
                throw new ObjectDisposedException("VertexBuffer");
            GfxNative.VertexWriteEx(handle, data, offsetInBytes, count, noOverwrite ? 0x20u : 0u);
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
            if (data == null)
                throw new ArgumentNullException("data");
            SetData(data, 0, 0, data.Length);
        }

        public unsafe void SetData(byte[] data, int offsetInBytes, int startIndex, int count)
        {
            if (disposed)
                throw new ObjectDisposedException("IndexBuffer");
            if (data == null)
                throw new ArgumentNullException("data");
            if (offsetInBytes < 0 || startIndex < 0 || count < 0 || startIndex + count > data.Length)
                throw new ArgumentOutOfRangeException("count");
            fixed (byte* p = data)
                GfxNative.IndexWrite(handle, (IntPtr)(p + startIndex), offsetInBytes, count);
        }

        public void SetData(IntPtr data, int offsetInBytes, int count)
        {
            if (disposed)
                throw new ObjectDisposedException("IndexBuffer");
            GfxNative.IndexWrite(handle, data, offsetInBytes, count);
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
        readonly int width, height, bytesPerPixel, linear, blockBytes;
        bool disposed;

        Texture(IntPtr handle, int width, int height, int bytesPerPixel, int linear, int blockBytes)
        {
            this.handle = handle;
            this.width = width;
            this.height = height;
            this.bytesPerPixel = bytesPerPixel;
            this.linear = linear;
            this.blockBytes = blockBytes;
        }

        /// <summary>
        /// Whether the texels are laid out in rows rather than swizzled. This decides how the
        /// sampler reads texture coordinates: a linear texture is addressed in texels, a swizzled
        /// one from 0 to 1. Compressed textures are swizzled for this purpose even though their
        /// blocks are written row by row.
        /// </summary>
        public bool IsLinear { get { return linear != 0 && blockBytes == 0; } }

        /// <summary>
        /// Bytes per 4x4 block, or zero for a format that is not block compressed.
        /// </summary>
        public static int BlockBytes(SurfaceFormat format)
        {
            switch (format)
            {
                case SurfaceFormat.Dxt1: return 8;
                case SurfaceFormat.Dxt3:
                case SurfaceFormat.Dxt5: return 16;
                default: return 0;
            }
        }

        internal IntPtr Handle { get { return handle; } }
        public int Width { get { return width; } }
        public int Height { get { return height; } }

        public static Texture Create(int width, int height, SurfaceFormat format)
        {
            return Create(width, height, 1, format);
        }

        public static Texture Create(int width, int height, int mipLevels, SurfaceFormat format)
        {
            if (width <= 0 || height <= 0)
                throw new ArgumentOutOfRangeException("width");
            IntPtr handle = GfxNative.CreateTextureEx(width, height, mipLevels, (int)format);
            if (handle == IntPtr.Zero)
                throw new OutOfMemoryException();
            int linear = format == SurfaceFormat.LinearA8R8G8B8 || format == SurfaceFormat.LinearX8R8G8B8 ? 1 : 0;
            int block = BlockBytes(format);
            if (block != 0)
            {
                // A compressed level is a grid of 4x4 blocks and cannot be swizzled, so it is
                // written row of blocks at a time. Describing it as a linear image one byte wide
                // lets the same write path carry it.
                return new Texture(handle, width, height, 1, 1, block);
            }
            return new Texture(handle, width, height, 4, linear, 0);
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
            int block = BlockBytes((SurfaceFormat)format);
            if (block != 0)
                return new Texture(handle, width, height, 1, 1, block);
            int bpp = format == (int)SurfaceFormat.A8R8G8B8 || format == (int)SurfaceFormat.X8R8G8B8 || linear != 0 ? 4 : 0;
            return new Texture(handle, width, height, bpp, linear, 0);
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
            SetData(0, pixels, 0, width, height);
        }

        /// <summary>
        /// Writes one mip level. The caller passes that level's dimensions, since only it knows
        /// how the chain was built.
        /// </summary>
        public unsafe void SetData(int level, byte[] pixels, int startIndex, int levelWidth, int levelHeight)
        {
            if (disposed)
                throw new ObjectDisposedException("Texture");
            if (pixels == null)
                throw new ArgumentNullException("pixels");
            if (bytesPerPixel == 0)
                throw new InvalidOperationException("texture");
            Shape(ref levelWidth, ref levelHeight);
            fixed (byte* p = pixels)
                GfxNative.TextureWriteLevel(handle, level, (IntPtr)(p + startIndex),
                    levelWidth, levelHeight, bytesPerPixel, linear);
        }

        /// <summary>
        /// Restates a compressed level's size as the byte grid the write and read paths copy: one
        /// row per row of 4x4 blocks, as wide as that row is long. A level narrower than a block
        /// still occupies a whole one.
        /// </summary>
        void Shape(ref int levelWidth, ref int levelHeight)
        {
            if (blockBytes == 0)
                return;
            levelWidth = (levelWidth + 3) / 4 * blockBytes;
            levelHeight = (levelHeight + 3) / 4;
        }

        public unsafe void GetData(int level, byte[] pixels, int startIndex, int levelWidth, int levelHeight)
        {
            if (disposed)
                throw new ObjectDisposedException("Texture");
            if (pixels == null)
                throw new ArgumentNullException("pixels");
            if (bytesPerPixel == 0)
                throw new InvalidOperationException("texture");
            Shape(ref levelWidth, ref levelHeight);
            fixed (byte* p = pixels)
                GfxNative.TextureReadLevel(handle, level, (IntPtr)(p + startIndex),
                    levelWidth, levelHeight, bytesPerPixel, linear);
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
