// Network checks for the on-device suite, written against System.dll's own Socket and Dns.
// 127.0.0.1 is not bindable on Xbox, so traffic to "this machine" goes to the title's address.
using System;
using System.Net;
using System.Net.Sockets;
using System.Runtime.InteropServices;

static class RxdkNet
{
    [DllImport("xnet", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
    static extern uint rxdk_title_ipv4();

    static IPAddress TitleAddress()
    {
        uint ip = rxdk_title_ipv4();
        if (ip == 0)
            throw new SocketException((int)SocketError.AddressNotAvailable);
        return new IPAddress(ip);
    }

    static bool Fail(string what, SocketException e)
    {
        Console.WriteLine("net fail " + what + " " + e.ErrorCode);
        return false;
    }

    static bool SameBytes(byte[] a, byte[] b, int count)
    {
        if (count != a.Length) return false;
        for (int i = 0; i < count; i++) if (a[i] != b[i]) return false;
        return true;
    }

    static readonly byte[] Message = { (byte)'r', (byte)'x', (byte)'d', (byte)'k' };

    public static bool UdpLoopback()
    {
        try {
            using (var sock = new Socket(AddressFamily.InterNetwork, SocketType.Dgram, ProtocolType.Udp)) {
                sock.Bind(new IPEndPoint(IPAddress.Any, 0));
                var target = new IPEndPoint(TitleAddress(), ((IPEndPoint)sock.LocalEndPoint).Port);
                Console.WriteLine("net " + target);
                if (sock.SendTo(Message, target) != Message.Length) return false;
                if (!sock.Poll(2000 * 1000, SelectMode.SelectRead)) return false;
                var buf = new byte[8];
                EndPoint from = new IPEndPoint(IPAddress.Any, 0);
                return SameBytes(Message, buf, sock.ReceiveFrom(buf, ref from));
            }
        } catch (SocketException e) {
            return Fail("udp", e);
        }
    }

    public static bool RejectsLoopback()
    {
        try {
            using (var sock = new Socket(AddressFamily.InterNetwork, SocketType.Dgram, ProtocolType.Udp)) {
                sock.Bind(new IPEndPoint(IPAddress.Loopback, 0));
                return false;
            }
        } catch (SocketException e) {
            return e.SocketErrorCode == SocketError.AddressNotAvailable;
        }
    }

    public static bool TcpEcho()
    {
        try {
            using (var server = new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp)) {
                server.Bind(new IPEndPoint(IPAddress.Any, 0));
                server.Listen(1);
                int port = ((IPEndPoint)server.LocalEndPoint).Port;
                using (var client = new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp)) {
                    client.Connect(new IPEndPoint(TitleAddress(), port));
                    using (Socket accepted = server.Accept()) {
                        if (client.Send(Message) != Message.Length) return false;
                        var buf = new byte[8];
                        return SameBytes(Message, buf, accepted.Receive(buf));
                    }
                }
            }
        } catch (SocketException e) {
            return Fail("tcp", e);
        }
    }

    public static bool HostName()
    {
        try {
            return Dns.GetHostName() == "xbox";
        } catch (SocketException e) {
            return Fail("dns", e);
        }
    }

    public static bool ResolveNumeric()
    {
        try {
            IPAddress title = TitleAddress();
            IPAddress[] addrs = Dns.GetHostAddresses(title.ToString());
            return addrs.Length > 0 && addrs[0].Equals(title);
        } catch (SocketException e) {
            return Fail("dns", e);
        }
    }

    public static bool ResolveName()
    {
        try {
            IPAddress[] addrs = Dns.GetHostAddresses("one.one.one.one");
            foreach (IPAddress a in addrs) {
                string text = a.ToString();
                if (text == "1.1.1.1" || text == "1.0.0.1") return true;
            }
            Console.WriteLine("net dns " + (addrs.Length == 0 ? "none" : addrs[0].ToString()));
            return false;
        } catch (SocketException e) {
            return Fail("dns", e);
        }
    }
}
