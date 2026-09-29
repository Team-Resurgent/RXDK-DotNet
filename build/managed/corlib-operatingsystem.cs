// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
// See the LICENSE file in the project root for more information.

// This stands in for corefx's System.Runtime.Extensions/src/System/OperatingSystem.cs, which
// scripts/build-corlib.sh drops from the source list in favour of this file. Everything above the
// platform helpers is that file unchanged.
//
// The reason for the swap is the block at the bottom. .NET 5 added the OperatingSystem.Is*
// platform checks and they are now the ordinary way to ask the question, so a library written any
// time in the last few years calls them and fails to compile against a corlib that predates them.
// The alternative would be making corefx's class partial, but it is a submodule of a submodule.
//
// The answers here are constants because a title only ever runs on the console. IsWindows is false
// even though the kernel is a cut-down Windows one and PlatformID reports Win32NT: code that asks
// is looking for the desktop APIs, registry, and drive letters, none of which are here.

using System.Diagnostics;
using System.Runtime.Serialization;

namespace System
{
    [Serializable]
    public sealed class OperatingSystem : ISerializable, ICloneable
    {
        private readonly Version _version;
        private readonly PlatformID _platform;
        private readonly string _servicePack;
        private string _versionString;

        public OperatingSystem(PlatformID platform, Version version) : this(platform, version, null)
        {
        }

        internal OperatingSystem(PlatformID platform, Version version, string servicePack)
        {
            if (platform < PlatformID.Win32S || platform > PlatformID.MacOSX)
            {
                throw new ArgumentOutOfRangeException(nameof(platform), platform, SR.Format(SR.Arg_EnumIllegalVal, platform));
            }

            if (version == null)
            {
                throw new ArgumentNullException(nameof(version));
            }

            _platform = platform;
            _version = version;
            _servicePack = servicePack;
        }

        public void GetObjectData(SerializationInfo info, StreamingContext context)
        {
            throw new PlatformNotSupportedException();
        }

        public PlatformID Platform => _platform;

        public string ServicePack => _servicePack ?? string.Empty;

        public Version Version => _version;

        public object Clone() => new OperatingSystem(_platform, _version, _servicePack);

        public override string ToString() => VersionString;

        public string VersionString
        {
            get
            {
                if (_versionString == null)
                {
                    string os;
                    switch (_platform)
                    {
                        case PlatformID.Win32S: os = "Microsoft Win32S "; break;
                        case PlatformID.Win32Windows: os = (_version.Major > 4 || (_version.Major == 4 && _version.Minor > 0)) ? "Microsoft Windows 98 " : "Microsoft Windows 95 "; break;
                        case PlatformID.Win32NT: os = "Microsoft Windows NT "; break;
                        case PlatformID.WinCE: os = "Microsoft Windows CE "; break;
                        case PlatformID.Unix: os = "Unix "; break;
                        case PlatformID.Xbox: os = "Xbox "; break;
                        case PlatformID.MacOSX: os = "Mac OS X "; break;
                        default:
                            Debug.Fail($"Unknown platform {_platform}");
                            os = "<unknown> "; break;
                    }

                    _versionString = string.IsNullOrEmpty(_servicePack) ?
                        os + _version.ToString() :
                        os + _version.ToString(3) + " " + _servicePack;
                }

                return _versionString;
            }
        }

        // ------------------------------------------------------------------ platform checks

        /// <summary>
        /// Whether the code is running on the named platform. The name is compared the way .NET
        /// does it, case-insensitively and ignoring a trailing version.
        /// </summary>
        public static bool IsOSPlatform(string platform)
        {
            if (platform == null)
                throw new ArgumentNullException(nameof(platform));

            return string.Equals(platform, "XBOX", StringComparison.OrdinalIgnoreCase);
        }

        public static bool IsOSPlatformVersionAtLeast(string platform, int major, int minor = 0, int build = 0, int revision = 0)
        {
            return IsOSPlatform(platform) && IsVersionAtLeast(major, minor, build, revision);
        }

        public static bool IsAndroid() => false;
        public static bool IsAndroidVersionAtLeast(int major, int minor = 0, int build = 0, int revision = 0) => false;
        public static bool IsBrowser() => false;
        public static bool IsFreeBSD() => false;
        public static bool IsFreeBSDVersionAtLeast(int major, int minor = 0, int build = 0, int revision = 0) => false;
        public static bool IsIOS() => false;
        public static bool IsIOSVersionAtLeast(int major, int minor = 0, int build = 0) => false;
        public static bool IsLinux() => false;
        public static bool IsMacCatalyst() => false;
        public static bool IsMacCatalystVersionAtLeast(int major, int minor = 0, int build = 0) => false;
        public static bool IsMacOS() => false;
        public static bool IsMacOSVersionAtLeast(int major, int minor = 0, int build = 0) => false;
        public static bool IsTvOS() => false;
        public static bool IsTvOSVersionAtLeast(int major, int minor = 0, int build = 0) => false;
        public static bool IsWasi() => false;
        public static bool IsWatchOS() => false;
        public static bool IsWatchOSVersionAtLeast(int major, int minor = 0, int build = 0) => false;

        /// <summary>
        /// False. The console runs a cut-down Windows kernel and PlatformID reports Win32NT, but
        /// code asking this question wants the desktop Windows surface, which is not here.
        /// </summary>
        public static bool IsWindows() => false;

        public static bool IsWindowsVersionAtLeast(int major, int minor = 0, int build = 0, int revision = 0) => false;

        private static bool IsVersionAtLeast(int major, int minor, int build, int revision)
        {
            Version current = Environment.OSVersion.Version;
            if (current.Major != major)
                return current.Major > major;
            if (current.Minor != minor)
                return current.Minor > minor;
            if (current.Build != build)
                return current.Build > build;
            return current.Revision >= revision;
        }
    }
}
