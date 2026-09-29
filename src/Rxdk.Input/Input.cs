// Managed Xbox input over libxapi. The DllImports stay internal. Callers own a device
// and dispose it. The native handle never leaves this assembly. Game pads, wheels,
// light guns, and the other pad-shaped controllers share one report. Mouse and the
// IR remote are their own devices.
using System;
using System.Runtime.InteropServices;

namespace Rxdk
{
    [Flags]
    public enum GamePadButton : ushort
    {
        DPadUp = 0x0001,
        DPadDown = 0x0002,
        DPadLeft = 0x0004,
        DPadRight = 0x0008,
        Start = 0x0010,
        Back = 0x0020,
        LeftThumb = 0x0040,
        RightThumb = 0x0080,
        LightGunOnScreen = 0x2000,
        LightGunFrameDoubler = 0x4000,
        LightGunLineDoubler = 0x8000
    }

    public enum GamePadKind : byte
    {
        Unknown = 0,
        GamePad = 0x01,
        GamePadAlt = 0x02,
        Wheel = 0x10,
        ArcadeStick = 0x20,
        DigitalArcadeStick = 0x21,
        FlightStick = 0x30,
        Snowboard = 0x40,
        LightGun = 0x50,
        RadioFlightControl = 0x60,
        FishingRod = 0x70,
        DancePad = 0x80
    }

    public struct GamePadState
    {
        public bool IsConnected;
        public uint PacketNumber;
        public GamePadButton Buttons;
        public byte A, B, X, Y, Black, White, LeftTrigger, RightTrigger;
        public short ThumbLeftX, ThumbLeftY, ThumbRightX, ThumbRightY;

        public bool IsDown(GamePadButton button) { return (Buttons & button) == button; }
    }

    public struct Keystroke
    {
        public byte VirtualKey;
        public byte Ascii;
        public byte Flags;

        public bool IsKeyUp { get { return (Flags & 0x40) != 0; } }
        public bool IsRepeat { get { return (Flags & 0x80) != 0; } }
        public bool Control { get { return (Flags & 0x01) != 0; } }
        public bool Shift { get { return (Flags & 0x02) != 0; } }
        public bool Alt { get { return (Flags & 0x04) != 0; } }
    }

    public sealed class GamePad : IDisposable
    {
        IntPtr handle;
        readonly int port;
        bool disposed;

        GamePad(int port, IntPtr handle)
        {
            this.port = port;
            this.handle = handle;
        }

        public int Port { get { return port; } }
        public bool IsConnected { get { return handle != IntPtr.Zero; } }

        public static uint ConnectedPorts
        {
            get { InputNative.Init(); return InputNative.ConnectedMask(); }
        }

        public static GamePad Open(int port)
        {
            if (port < 0 || port > 3)
                throw new ArgumentOutOfRangeException("port");
            return new GamePad(port, InputNative.OpenGamepad((uint)port));
        }

        public GamePadState GetState()
        {
            if (disposed)
                throw new ObjectDisposedException("GamePad");
            GamePadState state = new GamePadState();
            if (handle == IntPtr.Zero)
                return state;
            InputNative.PadState raw = new InputNative.PadState();
            uint rc = InputNative.GetState(handle, ref raw);
            if (rc != 0)
                return state;
            state.IsConnected = true;
            state.PacketNumber = raw.Packet;
            state.Buttons = (GamePadButton)raw.Buttons;
            state.A = raw.A;
            state.B = raw.B;
            state.X = raw.X;
            state.Y = raw.Y;
            state.Black = raw.Black;
            state.White = raw.White;
            state.LeftTrigger = raw.LeftTrigger;
            state.RightTrigger = raw.RightTrigger;
            state.ThumbLeftX = raw.Lx;
            state.ThumbLeftY = raw.Ly;
            state.ThumbRightX = raw.Rx;
            state.ThumbRightY = raw.Ry;
            return state;
        }

        public GamePadKind Kind
        {
            get
            {
                if (disposed || handle == IntPtr.Zero)
                    return GamePadKind.Unknown;
                return (GamePadKind)InputNative.Subtype(handle);
            }
        }

        public void SetLightgunCalibration(short centerX, short centerY, short upperLeftX, short upperLeftY)
        {
            if (disposed)
                throw new ObjectDisposedException("GamePad");
            if (handle == IntPtr.Zero)
                return;
            InputNative.SetLightgunCalibration(handle, (ushort)centerX, (ushort)centerY, (ushort)upperLeftX, (ushort)upperLeftY);
        }

        public void SetVibration(ushort leftMotor, ushort rightMotor)
        {
            if (disposed)
                throw new ObjectDisposedException("GamePad");
            if (handle == IntPtr.Zero)
                return;
            InputNative.SetVibration(handle, leftMotor, rightMotor);
        }

        public void Dispose()
        {
            if (disposed)
                return;
            disposed = true;
            if (handle != IntPtr.Zero)
            {
                InputNative.Close(handle);
                handle = IntPtr.Zero;
            }
            GC.SuppressFinalize(this);
        }

        ~GamePad() { Dispose(); }
    }

    public sealed class Keyboard : IDisposable
    {
        bool disposed;

        Keyboard() { }

        public static Keyboard Open()
        {
            uint rc = InputNative.InitKeyboard();
            if (rc != 0)
                throw new InvalidOperationException("keyboard queue failed: " + rc.ToString());
            return new Keyboard();
        }

        public bool TryGetKeystroke(out Keystroke key)
        {
            if (disposed)
                throw new ObjectDisposedException("Keyboard");
            InputNative.KeyRaw raw = new InputNative.KeyRaw();
            uint rc = InputNative.GetKeystroke(ref raw);
            key = new Keystroke();
            if (rc != 0)
                return false;
            key.VirtualKey = raw.VirtualKey;
            key.Ascii = raw.Ascii;
            key.Flags = raw.Flags;
            return true;
        }

        public void Dispose()
        {
            if (disposed)
                return;
            disposed = true;
            GC.SuppressFinalize(this);
        }

        ~Keyboard() { Dispose(); }
    }

    [Flags]
    public enum MouseButton : byte
    {
        Left = 0x01,
        Right = 0x02,
        Middle = 0x04,
        X1 = 0x08,
        X2 = 0x10
    }

    public struct MouseState
    {
        public bool IsConnected;
        public uint PacketNumber;
        public MouseButton Buttons;
        public sbyte X, Y, Wheel;

        public bool IsDown(MouseButton button) { return (Buttons & button) == button; }
    }

    public sealed class Mouse : IDisposable
    {
        IntPtr handle;
        readonly int port;
        bool disposed;

        Mouse(int port, IntPtr handle)
        {
            this.port = port;
            this.handle = handle;
        }

        public int Port { get { return port; } }
        public bool IsConnected { get { return handle != IntPtr.Zero; } }

        public static uint ConnectedPorts
        {
            get { InputNative.Init(); return InputNative.MouseMask(); }
        }

        public static Mouse Open(int port)
        {
            if (port < 0 || port > 3)
                throw new ArgumentOutOfRangeException("port");
            return new Mouse(port, InputNative.OpenMouse((uint)port));
        }

        public MouseState GetState()
        {
            if (disposed)
                throw new ObjectDisposedException("Mouse");
            MouseState state = new MouseState();
            if (handle == IntPtr.Zero)
                return state;
            InputNative.MouseRaw raw = new InputNative.MouseRaw();
            uint rc = InputNative.GetMouse(handle, ref raw);
            if (rc != 0)
                return state;
            state.IsConnected = true;
            state.PacketNumber = raw.Packet;
            state.Buttons = (MouseButton)raw.Buttons;
            state.X = raw.X;
            state.Y = raw.Y;
            state.Wheel = raw.Wheel;
            return state;
        }

        public void Dispose()
        {
            if (disposed)
                return;
            disposed = true;
            if (handle != IntPtr.Zero)
            {
                InputNative.Close(handle);
                handle = IntPtr.Zero;
            }
            GC.SuppressFinalize(this);
        }

        ~Mouse() { Dispose(); }
    }

    public struct IrRemoteState
    {
        public bool IsConnected;
        public uint PacketNumber;
        public ushort KeyCode;
        public ushort TimeDelta;
    }

    public sealed class IrRemote : IDisposable
    {
        IntPtr handle;
        readonly int port;
        bool disposed;

        IrRemote(int port, IntPtr handle)
        {
            this.port = port;
            this.handle = handle;
        }

        public int Port { get { return port; } }
        public bool IsConnected { get { return handle != IntPtr.Zero; } }

        public static uint ConnectedPorts
        {
            get { InputNative.Init(); return InputNative.IrMask(); }
        }

        public static IrRemote Open(int port)
        {
            if (port < 0 || port > 3)
                throw new ArgumentOutOfRangeException("port");
            return new IrRemote(port, InputNative.OpenIr((uint)port));
        }

        public IrRemoteState GetState()
        {
            if (disposed)
                throw new ObjectDisposedException("IrRemote");
            IrRemoteState state = new IrRemoteState();
            if (handle == IntPtr.Zero)
                return state;
            InputNative.IrRaw raw = new InputNative.IrRaw();
            uint rc = InputNative.GetIr(handle, ref raw);
            if (rc != 0)
                return state;
            state.IsConnected = true;
            state.PacketNumber = raw.Packet;
            state.KeyCode = raw.Key;
            state.TimeDelta = raw.Dt;
            return state;
        }

        public void Dispose()
        {
            if (disposed)
                return;
            disposed = true;
            if (handle != IntPtr.Zero)
            {
                InputNative.Close(handle);
                handle = IntPtr.Zero;
            }
            GC.SuppressFinalize(this);
        }

        ~IrRemote() { Dispose(); }
    }

    static class InputNative
    {
        [StructLayout(LayoutKind.Sequential, Pack = 1)]
        internal struct PadState
        {
            public uint Packet;
            public ushort Buttons;
            public byte A, B, X, Y, Black, White, LeftTrigger, RightTrigger;
            public short Lx, Ly, Rx, Ry;
        }

        [StructLayout(LayoutKind.Sequential, Pack = 1)]
        internal struct KeyRaw
        {
            public byte VirtualKey;
            public byte Ascii;
            public byte Flags;
        }

        [StructLayout(LayoutKind.Sequential, Pack = 1)]
        internal struct MouseRaw
        {
            public uint Packet;
            public byte Buttons;
            public sbyte X, Y, Wheel;
        }

        [StructLayout(LayoutKind.Sequential, Pack = 1)]
        internal struct IrRaw
        {
            public uint Packet;
            public ushort Key;
            public ushort Dt;
        }

        [DllImport("xapi", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        static extern void rxdk_input_init();

        [DllImport("xapi", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        static extern uint rxdk_input_connected_mask();

        [DllImport("xapi", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        static extern IntPtr rxdk_input_open_gamepad(uint port);

        [DllImport("xapi", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        static extern void rxdk_input_close(IntPtr handle);

        [DllImport("xapi", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        static extern uint rxdk_input_get_state(IntPtr handle, ref PadState state);

        [DllImport("xapi", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        static extern uint rxdk_input_set_vibration(IntPtr handle, ushort left, ushort right);

        [DllImport("xapi", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        static extern uint rxdk_input_init_keyboard();

        [DllImport("xapi", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        static extern uint rxdk_input_get_keystroke(ref KeyRaw key);

        internal static void Init() { rxdk_input_init(); }
        internal static uint ConnectedMask() { return rxdk_input_connected_mask(); }
        internal static IntPtr OpenGamepad(uint port) { return rxdk_input_open_gamepad(port); }
        internal static void Close(IntPtr handle) { rxdk_input_close(handle); }
        internal static uint GetState(IntPtr handle, ref PadState state) { return rxdk_input_get_state(handle, ref state); }
        internal static void SetVibration(IntPtr handle, ushort left, ushort right) { rxdk_input_set_vibration(handle, left, right); }
        internal static uint InitKeyboard() { return rxdk_input_init_keyboard(); }
        internal static uint GetKeystroke(ref KeyRaw key) { return rxdk_input_get_keystroke(ref key); }

        [DllImport("xapi", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        static extern uint rxdk_input_mouse_mask();

        [DllImport("xapi", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        static extern IntPtr rxdk_input_open_mouse(uint port);

        [DllImport("xapi", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        static extern uint rxdk_input_get_mouse(IntPtr handle, ref MouseRaw state);

        [DllImport("xapi", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        static extern uint rxdk_input_ir_mask();

        [DllImport("xapi", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        static extern IntPtr rxdk_input_open_ir(uint port);

        [DllImport("xapi", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        static extern uint rxdk_input_get_ir(IntPtr handle, ref IrRaw state);

        [DllImport("xapi", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        static extern uint rxdk_input_subtype(IntPtr handle);

        [DllImport("xapi", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        static extern uint rxdk_input_set_lightgun_calibration(IntPtr handle, ushort centerX, ushort centerY, ushort upperLeftX, ushort upperLeftY);

        internal static uint MouseMask() { return rxdk_input_mouse_mask(); }
        internal static IntPtr OpenMouse(uint port) { return rxdk_input_open_mouse(port); }
        internal static uint GetMouse(IntPtr handle, ref MouseRaw state) { return rxdk_input_get_mouse(handle, ref state); }
        internal static uint IrMask() { return rxdk_input_ir_mask(); }
        internal static IntPtr OpenIr(uint port) { return rxdk_input_open_ir(port); }
        internal static uint GetIr(IntPtr handle, ref IrRaw state) { return rxdk_input_get_ir(handle, ref state); }
        internal static uint Subtype(IntPtr handle) { return rxdk_input_subtype(handle); }
        internal static void SetLightgunCalibration(IntPtr handle, ushort centerX, ushort centerY, ushort upperLeftX, ushort upperLeftY)
        {
            rxdk_input_set_lightgun_calibration(handle, centerX, centerY, upperLeftX, upperLeftY);
        }
    }
}
