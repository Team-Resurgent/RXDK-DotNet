// Types DeflateStream.cs needs that live in System.dll on desktop Mono. Compiled into Main.dll
// so the on-device suite can round-trip DeflateStream and GZipStream without building System.dll.
// GZipStream is the Mono wrapper: gzip=true selects windowBits 31 in zlib-helper.c.
using System;
using System.IO;

namespace Mono.Util {
    [AttributeUsage(AttributeTargets.Method)]
    sealed class MonoPInvokeCallbackAttribute : Attribute {
        public MonoPInvokeCallbackAttribute(Type t) {}
    }
}

namespace System.IO.Compression {
    public enum CompressionMode { Decompress = 0, Compress = 1 }
    public enum CompressionLevel { Optimal = 0, Fastest = 1, NoCompression = 2 }

    public class GZipStream : Stream {
        readonly DeflateStream ds;

        public GZipStream(Stream stream, CompressionMode mode) : this(stream, mode, false) {}

        public GZipStream(Stream stream, CompressionMode mode, bool leaveOpen) {
            ds = new DeflateStream(stream, mode, leaveOpen, true);
        }

        protected override void Dispose(bool disposing) {
            if (disposing && ds != null) ds.Dispose();
            base.Dispose(disposing);
        }

        public override bool CanRead { get { return ds.CanRead; } }
        public override bool CanSeek { get { return false; } }
        public override bool CanWrite { get { return ds.CanWrite; } }
        public override long Length { get { throw new NotSupportedException(); } }
        public override long Position {
            get { throw new NotSupportedException(); }
            set { throw new NotSupportedException(); }
        }
        public override void Flush() { ds.Flush(); }
        public override int Read(byte[] buffer, int offset, int count) { return ds.Read(buffer, offset, count); }
        public override long Seek(long offset, SeekOrigin origin) { throw new NotSupportedException(); }
        public override void SetLength(long value) { throw new NotSupportedException(); }
        public override void Write(byte[] buffer, int offset, int count) { ds.Write(buffer, offset, count); }
    }
}
