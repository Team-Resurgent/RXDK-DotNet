// Managed Xbox input over libxapi. The DllImports stay internal. Callers own a GamePad
// or Keyboard and dispose it. The native handle never leaves this assembly.
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
        RightThumb = 0x0080
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
    }
}
