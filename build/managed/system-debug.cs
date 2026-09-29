// System.Diagnostics.Debug for System.dll.
//
// In the net_4_x profile the public Debug lives in System.dll, not corlib. Mono's corlib has its
// own internal copy and renames it to DebugInternal in a post-processing step we do not run, so
// without this file every caller gets CS0122 on the internal one. Mono builds the reference
// source Debug.cs, which drags in the whole TraceListener, TraceSource, and config stack; a
// console title has no use for configurable listeners, so this writes to the console instead,
// which the host forwards to the serial port.

using System.Globalization;

namespace System.Diagnostics
{
    public static class Debug
    {
        static int indentLevel;
        static int indentSize = 4;
        static bool needIndent = true;

        public static int IndentLevel
        {
            get { return indentLevel; }
            set { indentLevel = value < 0 ? 0 : value; }
        }

        public static int IndentSize
        {
            get { return indentSize; }
            set { indentSize = value < 0 ? 0 : value; }
        }

        /// <summary>Present for source compatibility; output is never buffered.</summary>
        public static bool AutoFlush { get { return true; } set { } }

        public static void Indent() { indentLevel++; }

        public static void Unindent() { if (indentLevel > 0) indentLevel--; }

        public static void Flush() { }

        [Conditional("DEBUG")]
        public static void Assert(bool condition)
        {
            if (!condition)
                Fail(null);
        }

        [Conditional("DEBUG")]
        public static void Assert(bool condition, string message)
        {
            if (!condition)
                Fail(message);
        }

        [Conditional("DEBUG")]
        public static void Assert(bool condition, string message, string detailMessage)
        {
            if (!condition)
                Fail(message, detailMessage);
        }

        [Conditional("DEBUG")]
        public static void Assert(bool condition, string message, string detailMessageFormat, params object[] args)
        {
            if (!condition)
                Fail(message, string.Format(CultureInfo.InvariantCulture, detailMessageFormat, args));
        }

        /// <summary>
        /// Reports the failure and returns. A console has nowhere to show the assertion dialog
        /// .NET Framework would put up, and taking the title down would lose the message.
        /// </summary>
        [Conditional("DEBUG")]
        public static void Fail(string message)
        {
            WriteLine(message == null ? "Assertion failed" : "Assertion failed: " + message);
        }

        [Conditional("DEBUG")]
        public static void Fail(string message, string detailMessage)
        {
            Fail(message);
            if (!string.IsNullOrEmpty(detailMessage))
                WriteLine(detailMessage);
        }

        [Conditional("DEBUG")]
        public static void Write(string message)
        {
            WriteCore(message);
        }

        [Conditional("DEBUG")]
        public static void Write(object value)
        {
            Write(value == null ? string.Empty : value.ToString());
        }

        [Conditional("DEBUG")]
        public static void Write(string message, string category)
        {
            Write(category == null ? message : category + ": " + message);
        }

        [Conditional("DEBUG")]
        public static void Write(object value, string category)
        {
            Write(value == null ? string.Empty : value.ToString(), category);
        }

        [Conditional("DEBUG")]
        public static void WriteLine(string message)
        {
            WriteCore(message);
            WriteCore(Environment.NewLine);
            needIndent = true;
        }

        [Conditional("DEBUG")]
        public static void WriteLine(object value)
        {
            WriteLine(value == null ? string.Empty : value.ToString());
        }

        [Conditional("DEBUG")]
        public static void WriteLine(string message, string category)
        {
            WriteLine(category == null ? message : category + ": " + message);
        }

        [Conditional("DEBUG")]
        public static void WriteLine(object value, string category)
        {
            WriteLine(value == null ? string.Empty : value.ToString(), category);
        }

        [Conditional("DEBUG")]
        public static void WriteLine(string format, params object[] args)
        {
            WriteLine(string.Format(CultureInfo.InvariantCulture, format, args));
        }

        [Conditional("DEBUG")]
        public static void WriteIf(bool condition, string message)
        {
            if (condition)
                Write(message);
        }

        [Conditional("DEBUG")]
        public static void WriteIf(bool condition, object value)
        {
            if (condition)
                Write(value);
        }

        [Conditional("DEBUG")]
        public static void WriteLineIf(bool condition, string message)
        {
            if (condition)
                WriteLine(message);
        }

        [Conditional("DEBUG")]
        public static void WriteLineIf(bool condition, object value)
        {
            if (condition)
                WriteLine(value);
        }

        [Conditional("DEBUG")]
        public static void Print(string message)
        {
            WriteLine(message);
        }

        [Conditional("DEBUG")]
        public static void Print(string format, params object[] args)
        {
            WriteLine(string.Format(CultureInfo.InvariantCulture, format, args));
        }

        static void WriteCore(string message)
        {
            if (message == null)
                message = string.Empty;
            if (needIndent && indentLevel > 0 && indentSize > 0)
                message = new string(' ', indentLevel * indentSize) + message;
            needIndent = false;
            Console.Error.Write(message);
        }
    }
}
