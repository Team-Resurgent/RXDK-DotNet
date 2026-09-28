// Minimal System.dll so Mono's socket icalls can find System.Net.SocketAddress
// (w32socket.c loads "System.dll" by name). Public surface is UDP and TCP
// (construct, bind 0.0.0.0, send, receive, listen, accept, connect) plus Dns.
// SocketException.ErrorCode is the WSA error. 127.0.0.1 is not bindable on Xbox.
using System;
using System.Runtime.CompilerServices;

namespace System.Net {
    public class SocketAddress {
        internal int m_Size;
        internal byte[] m_Buffer;

        public SocketAddress(int family, int size) {
            m_Buffer = new byte[size];
            m_Size = size;
            m_Buffer[0] = (byte)(family & 0xff);
            m_Buffer[1] = (byte)((family >> 8) & 0xff);
        }

        public int Size { get { return m_Size; } }

        public int Port {
            get { return m_Buffer == null || m_Size < 4 ? 0 : (m_Buffer[2] << 8) | m_Buffer[3]; }
            set {
                m_Buffer[2] = (byte)((value >> 8) & 0xff);
                m_Buffer[3] = (byte)(value & 0xff);
            }
        }

        public void SetIPv4(byte a, byte b, byte c, byte d) {
            m_Buffer[4] = a;
            m_Buffer[5] = b;
            m_Buffer[6] = c;
            m_Buffer[7] = d;
        }
    }

    public class Dns {
        [MethodImpl(MethodImplOptions.InternalCall)]
        static extern bool GetHostByName_icall(string host, out string h_name, out string[] h_aliases, out string[] h_addr_list, int hint);
        [MethodImpl(MethodImplOptions.InternalCall)]
        static extern bool GetHostName_icall(out string h_name);

        public static string GetHostName() {
            string name;
            if (!GetHostName_icall(out name) || name == null)
                throw new System.Net.Sockets.SocketException(11001);
            return name;
        }

        public static string[] GetHostAddresses(string host) {
            string h_name;
            string[] aliases, addrs;
            if (!GetHostByName_icall(host, out h_name, out aliases, out addrs, 0) || addrs == null)
                throw new System.Net.Sockets.SocketException(11001);
            return addrs;
        }

        public static bool HostName() {
            try { return GetHostName() == "xbox"; }
            catch (System.Net.Sockets.SocketException e) {
                Console.WriteLine("net fail dns " + e.ErrorCode);
                return false;
            }
        }

        public static bool ResolveNumeric() {
            try {
                string text = System.Net.Sockets.Socket.FormatIPv4(System.Net.Sockets.Socket.TitleIPv4());
                string[] addrs = GetHostAddresses(text);
                return addrs.Length > 0 && addrs[0] == text;
            } catch (System.Net.Sockets.SocketException e) {
                Console.WriteLine("net fail dns " + e.ErrorCode);
                return false;
            }
        }

        public static bool ResolveName() {
            try {
                string[] addrs = GetHostAddresses("one.one.one.one");
                for (int i = 0; i < addrs.Length; i++)
                    if (addrs[i] == "1.1.1.1" || addrs[i] == "1.0.0.1") return true;
                Console.WriteLine("net dns " + (addrs.Length == 0 ? "none" : addrs[0]));
                return false;
            } catch (System.Net.Sockets.SocketException e) {
                Console.WriteLine("net fail dns " + e.ErrorCode);
                return false;
            }
        }
    }
}

namespace System.Net.Sockets {
    public class SocketException : Exception {
        public int ErrorCode { get; }
        public SocketException(int error) : base("socket error " + error) { ErrorCode = error; }
    }

    public class LingerOption {
        public bool enabled;
        public int lingerTime;
        public LingerOption(bool enable, int seconds) { enabled = enable; lingerTime = seconds; }
    }

    public class Socket : IDisposable {
        public const int InterNetwork = 2;
        public const int Stream = 1;
        public const int Dgram = 2;
        public const int Tcp = 6;
        public const int Udp = 17;
        public const int AddressNotAvailable = 10049;

        IntPtr handle;
        int family;
        public int LastError { get; private set; }

        [MethodImpl(MethodImplOptions.InternalCall)]
        static extern IntPtr Socket_icall(int family, int type, int proto, out int error);
        [MethodImpl(MethodImplOptions.InternalCall)]
        static extern void Bind_icall(IntPtr sock, SocketAddress sa, out int error);
        [MethodImpl(MethodImplOptions.InternalCall)]
        static extern SocketAddress LocalEndPoint_icall(IntPtr sock, int family, out int error);
        [MethodImpl(MethodImplOptions.InternalCall)]
        static extern unsafe int SendTo_icall(IntPtr sock, byte* buffer, int count, int flags, SocketAddress sa, out int error, bool blocking);
        [MethodImpl(MethodImplOptions.InternalCall)]
        static extern unsafe int ReceiveFrom_icall(IntPtr sock, byte* buffer, int count, int flags, ref SocketAddress sa, out int error, bool blocking);
        [MethodImpl(MethodImplOptions.InternalCall)]
        static extern bool Poll_icall(IntPtr sock, int mode, int timeout, out int error);
        [MethodImpl(MethodImplOptions.InternalCall)]
        static extern void Close_icall(IntPtr sock, out int error);
        [MethodImpl(MethodImplOptions.InternalCall)]
        static extern void Listen_icall(IntPtr sock, int backlog, out int error);
        [MethodImpl(MethodImplOptions.InternalCall)]
        static extern IntPtr Accept_icall(IntPtr sock, out int error, bool blocking);
        [MethodImpl(MethodImplOptions.InternalCall)]
        static extern void Connect_icall(IntPtr sock, SocketAddress sa, out int error, bool blocking);
        [MethodImpl(MethodImplOptions.InternalCall)]
        static extern unsafe int Send_icall(IntPtr sock, byte* buffer, int count, int flags, out int error, bool blocking);
        [MethodImpl(MethodImplOptions.InternalCall)]
        static extern unsafe int Receive_icall(IntPtr sock, byte* buffer, int count, int flags, out int error, bool blocking);

        Socket(IntPtr accepted, int addressFamily) {
            handle = accepted;
            family = addressFamily;
        }
        [System.Runtime.InteropServices.DllImport("xnet", CallingConvention=System.Runtime.InteropServices.CallingConvention.Cdecl, ExactSpelling=true)]
        static extern uint rxdk_title_ipv4();

        public Socket(int addressFamily, int socketType, int protocol) {
            family = addressFamily;
            int error;
            handle = Socket_icall(addressFamily, socketType, protocol, out error);
            LastError = error;
            if (handle == IntPtr.Zero || error != 0)
                throw new SocketException(error);
        }

        public void Listen(int backlog) {
            int error;
            Listen_icall(handle, backlog, out error);
            LastError = error;
            if (error != 0) throw new SocketException(error);
        }

        public Socket Accept() {
            int error;
            IntPtr accepted = Accept_icall(handle, out error, true);
            LastError = error;
            if (accepted == IntPtr.Zero || error != 0) throw new SocketException(error);
            return new Socket(accepted, family);
        }

        public void Connect(SocketAddress address) {
            int error;
            Connect_icall(handle, address, out error, true);
            LastError = error;
            if (error != 0) throw new SocketException(error);
        }

        public unsafe int Send(byte[] buffer) {
            int error, n;
            fixed (byte* p = buffer)
                n = Send_icall(handle, p, buffer.Length, 0, out error, true);
            LastError = error;
            if (error != 0) throw new SocketException(error);
            return n;
        }

        public unsafe int Receive(byte[] buffer) {
            int error, n;
            fixed (byte* p = buffer)
                n = Receive_icall(handle, p, buffer.Length, 0, out error, true);
            LastError = error;
            if (error != 0) throw new SocketException(error);
            return n;
        }

        public void Bind(SocketAddress address) {
            int error;
            Bind_icall(handle, address, out error);
            LastError = error;
            if (error != 0) throw new SocketException(error);
        }

        // 0.0.0.0. Port 0 lets the stack pick one. 127.0.0.1 throws AddressNotAvailable.
        public void BindAny(int port) {
            var any = new SocketAddress(family, 16);
            any.Port = port;
            Bind(any);
        }

        public SocketAddress LocalEndPoint {
            get {
                int error;
                SocketAddress local = LocalEndPoint_icall(handle, family, out error);
                LastError = error;
                if (error != 0 || local == null) throw new SocketException(error);
                return local;
            }
        }

        public unsafe int SendTo(byte[] buffer, SocketAddress to) {
            int error, n;
            fixed (byte* p = buffer)
                n = SendTo_icall(handle, p, buffer.Length, 0, to, out error, true);
            LastError = error;
            if (error != 0) throw new SocketException(error);
            return n;
        }

        public unsafe int ReceiveFrom(byte[] buffer, ref SocketAddress from) {
            int error, n;
            fixed (byte* p = buffer)
                n = ReceiveFrom_icall(handle, p, buffer.Length, 0, ref from, out error, true);
            LastError = error;
            if (error != 0) throw new SocketException(error);
            return n;
        }

        // Timeout is microseconds. Mode 0 is SelectRead.
        public bool Poll(int mode, int microseconds) {
            int error;
            bool ready = Poll_icall(handle, mode, microseconds, out error);
            LastError = error;
            if (error != 0) throw new SocketException(error);
            return ready;
        }

        public static uint TitleIPv4() { return rxdk_title_ipv4(); }

        public static string FormatIPv4(uint ip) {
            return (ip & 255) + "." + ((ip >> 8) & 255) + "." + ((ip >> 16) & 255) + "." + ((ip >> 24) & 255);
        }

        public void Close() {
            if (handle == IntPtr.Zero) return;
            int error;
            Close_icall(handle, out error);
            handle = IntPtr.Zero;
            LastError = error;
        }

        public void Dispose() { Close(); }

        public static bool Loopback() {
            try {
                using (var sock = new Socket(InterNetwork, Dgram, Udp)) {
                    sock.BindAny(0);
                    SocketAddress local = sock.LocalEndPoint;
                    uint ip = TitleIPv4();
                    if (ip == 0) {
                        Console.WriteLine("net fail no address");
                        return false;
                    }
                    Console.WriteLine("net " + FormatIPv4(ip) + ":" + local.Port);
                    local.SetIPv4((byte)(ip & 255), (byte)((ip >> 8) & 255), (byte)((ip >> 16) & 255), (byte)((ip >> 24) & 255));
                    byte[] msg = new byte[] { (byte)'r', (byte)'x', (byte)'d', (byte)'k' };
                    if (sock.SendTo(msg, local) != msg.Length) return false;
                    if (!sock.Poll(0, 2000 * 1000)) return false;
                    byte[] buf = new byte[8];
                    SocketAddress from = new SocketAddress(InterNetwork, 16);
                    int got = sock.ReceiveFrom(buf, ref from);
                    if (got != msg.Length) return false;
                    for (int i = 0; i < got; i++) if (buf[i] != msg[i]) return false;
                    return true;
                }
            } catch (SocketException e) {
                Console.WriteLine("net fail " + e.ErrorCode);
                return false;
            }
        }

        public static bool RejectsLoopback() {
            try {
                using (var sock = new Socket(InterNetwork, Dgram, Udp)) {
                    var sa = new SocketAddress(InterNetwork, 16);
                    sa.SetIPv4(127, 0, 0, 1);
                    sock.Bind(sa);
                    return false;
                }
            } catch (SocketException e) {
                return e.ErrorCode == AddressNotAvailable;
            }
        }

        static SocketAddress TitleEndpoint(int port) {
            uint ip = TitleIPv4();
            var sa = new SocketAddress(InterNetwork, 16);
            sa.Port = port;
            sa.SetIPv4((byte)(ip & 255), (byte)((ip >> 8) & 255), (byte)((ip >> 16) & 255), (byte)((ip >> 24) & 255));
            return sa;
        }

        public static bool TcpEcho() {
            try {
                using (var server = new Socket(InterNetwork, Stream, Tcp)) {
                    server.BindAny(0);
                    int port = server.LocalEndPoint.Port;
                    server.Listen(1);
                    using (var client = new Socket(InterNetwork, Stream, Tcp)) {
                        client.Connect(TitleEndpoint(port));
                        using (Socket accepted = server.Accept()) {
                            byte[] msg = new byte[] { (byte)'r', (byte)'x', (byte)'d', (byte)'k' };
                            if (client.Send(msg) != msg.Length) return false;
                            byte[] buf = new byte[8];
                            int got = accepted.Receive(buf);
                            if (got != msg.Length) return false;
                            for (int i = 0; i < got; i++) if (buf[i] != msg[i]) return false;
                            return true;
                        }
                    }
                }
            } catch (SocketException e) {
                Console.WriteLine("net fail tcp " + e.ErrorCode);
                return false;
            }
        }
    }
}
