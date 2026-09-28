/*
 * RXDK-DotNet — Win32 WIDE file-API thunks for w32file-win32.c.
 *
 * The RXDK SDK is not UNICODE-aware: it declares only the ANSI file APIs, and its WIDE variants are
 * broken on D:\ paths. Mono's w32file-win32.c (a UNICODE build) calls the W APIs with gunichar2
 * (UTF-16) paths. Since the SDK declares no W variants, w32file-win32.c references them undecorated
 * (implicit cdecl), so we define them here as cdecl thunks that convert UTF-16->ANSI and call the
 * working ANSI variant (a stdcall libxapi export). This file includes NO SDK headers (matching
 * win_cdecl_shims.c) so its definitions get plain cdecl linkage; the A variants and structs are
 * declared by hand. Console handles are still routed to serial by win32_supplement.c's
 * mono_w32file_* overrides, which win the --allow-multiple-definition race.
 */

typedef unsigned long  DWORD;
typedef int            BOOL;
typedef void          *HANDLE;
typedef unsigned short WCHAR;

/* ANSI find-data (SDK layout) and our wide find-data (matches rxdk/win32_supplement.h). */
typedef struct { DWORD lo, hi; } RXDK_FT;
typedef struct {
    DWORD dwFileAttributes;
    RXDK_FT ftCreationTime, ftLastAccessTime, ftLastWriteTime;
    DWORD nFileSizeHigh, nFileSizeLow, dwReserved0, dwReserved1;
    char  cFileName[260];
    char  cAlternateFileName[14];
} RXDK_FIND_A;
typedef struct {
    DWORD dwFileAttributes;
    RXDK_FT ftCreationTime, ftLastAccessTime, ftLastWriteTime;
    DWORD nFileSizeHigh, nFileSizeLow, dwReserved0, dwReserved1;
    WCHAR cFileName[260];
    WCHAR cAlternateFileName[14];
} RXDK_FIND_W;

#define RXDK_INVALID_HANDLE ((HANDLE)(long)-1)

/* stdcall ANSI variants / helpers provided by libxapi. */
extern HANDLE __attribute__((__stdcall__)) CreateFileA(const char*, DWORD, DWORD, void*, DWORD, DWORD, void*);
extern DWORD  __attribute__((__stdcall__)) GetFileAttributesA(const char*);
extern DWORD  __attribute__((__stdcall__)) GetFileSize(HANDLE, DWORD*);
extern BOOL   __attribute__((__stdcall__)) CloseHandle(HANDLE);
extern void   __attribute__((__stdcall__)) SetLastError(DWORD);
extern BOOL   __attribute__((__stdcall__)) CreateDirectoryA(const char*, void*);
extern BOOL   __attribute__((__stdcall__)) RemoveDirectoryA(const char*);
extern BOOL   __attribute__((__stdcall__)) DeleteFileA(const char*);
extern BOOL   __attribute__((__stdcall__)) MoveFileA(const char*, const char*);
extern BOOL   __attribute__((__stdcall__)) CopyFileA(const char*, const char*, BOOL);
extern BOOL   __attribute__((__stdcall__)) SetFileAttributesA(const char*, DWORD);
extern BOOL   __attribute__((__stdcall__)) GetFileAttributesExA(const char*, int, void*);
extern BOOL   __attribute__((__stdcall__)) GetDiskFreeSpaceExA(const char*, void*, void*, void*);
extern BOOL   __attribute__((__stdcall__)) GetVolumeInformationA(const char*, char*, DWORD, DWORD*, DWORD*, DWORD*, char*, DWORD);

static void w2a(const WCHAR *w, char *a, int n) { int i = 0; if (w) for (; i < n - 1 && w[i]; ++i) a[i] = (char)w[i]; a[i] = 0; }
static void a2w(const char *a, WCHAR *w, int n) { int i = 0; if (a) for (; i < n - 1 && a[i]; ++i) w[i] = (unsigned char)a[i]; w[i] = 0; }
static void copy_find(const RXDK_FIND_A *s, RXDK_FIND_W *d)
{
    d->dwFileAttributes = s->dwFileAttributes;
    d->ftCreationTime = s->ftCreationTime; d->ftLastAccessTime = s->ftLastAccessTime; d->ftLastWriteTime = s->ftLastWriteTime;
    d->nFileSizeHigh = s->nFileSizeHigh; d->nFileSizeLow = s->nFileSizeLow;
    d->dwReserved0 = s->dwReserved0; d->dwReserved1 = s->dwReserved1;
    a2w(s->cFileName, d->cFileName, 260);
    a2w(s->cAlternateFileName, d->cAlternateFileName, 14);
}

HANDLE CreateFileW(const WCHAR *name, DWORD access, DWORD share, void *sa, DWORD disp, DWORD flags, void *tmpl)
{ char a[520]; w2a(name, a, 520); return CreateFileA(a, access, share, sa, disp, flags, tmpl); }

/* Directory enumeration: libxapi's FindFirstFile(A) is broken on the Xbox (it faults) — the kernel
 * enumerates via NtQueryDirectoryFile, which corefx also targets but we haven't wired. Fail cleanly
 * (ERROR_FILE_NOT_FOUND, no results) instead of crashing. Directory.GetFiles/EnumerateFiles is a
 * known follow-up (needs NtQueryDirectoryFile). */
#define RXDK_ERROR_NO_MORE_FILES 18
#define RXDK_ERROR_FILE_NOT_FOUND 2
HANDLE FindFirstFileW(const WCHAR *pattern, RXDK_FIND_W *wfd)
{ (void)pattern; (void)wfd; SetLastError(RXDK_ERROR_FILE_NOT_FOUND); return RXDK_INVALID_HANDLE; }

BOOL FindNextFileW(HANDLE h, RXDK_FIND_W *wfd)
{ (void)h; (void)wfd; SetLastError(RXDK_ERROR_NO_MORE_FILES); return 0; }

BOOL CreateDirectoryW(const WCHAR *name, void *sa)   { char a[520]; w2a(name, a, 520); return CreateDirectoryA(a, sa); }
BOOL RemoveDirectoryW(const WCHAR *name)             { char a[520]; w2a(name, a, 520); return RemoveDirectoryA(a); }
BOOL DeleteFileW(const WCHAR *name)                  { char a[520]; w2a(name, a, 520); return DeleteFileA(a); }
BOOL MoveFileW(const WCHAR *src, const WCHAR *dst)   { char a[520], b[520]; w2a(src, a, 520); w2a(dst, b, 520); return MoveFileA(a, b); }
BOOL CopyFileW(const WCHAR *src, const WCHAR *dst, BOOL failIfExists)
{ char a[520], b[520]; w2a(src, a, 520); w2a(dst, b, 520); return CopyFileA(a, b, failIfExists); }
BOOL SetFileAttributesW(const WCHAR *name, DWORD attrs) { char a[520]; w2a(name, a, 520); return SetFileAttributesA(a, attrs); }
/* libxapi's GetFileAttributesExA faults on the Xbox; GetFileAttributesA works. Build the
 * WIN32_FILE_ATTRIBUTE_DATA ([0]=attrs, [1..6]=3 FILETIMEs, [7]=sizeHigh, [8]=sizeLow) from attrs,
 * opening the file for its size when it isn't a directory. Times are left zero (rarely used). */
BOOL GetFileAttributesExW(const WCHAR *name, int level, void *info)
{
    char a[520]; DWORD attrs; DWORD *d = (DWORD *)info; (void)level;
    w2a(name, a, 520);
    attrs = GetFileAttributesA(a);
    if (attrs == 0xFFFFFFFFu) { SetLastError(2 /*ERROR_FILE_NOT_FOUND*/); return 0; }
    for (int i = 0; i < 9; ++i) d[i] = 0;
    d[0] = attrs;   /* size/times left 0 for now (File.Exists needs only attrs) */
    return 1;
}
BOOL GetDiskFreeSpaceExW(const WCHAR *dir, void *avail, void *total, void *free_)
{ char a[520]; w2a(dir, a, 520); return GetDiskFreeSpaceExA(dir ? a : (char*)0, avail, total, free_); }
BOOL GetVolumeInformationW(const WCHAR *root, WCHAR *volname, DWORD volsz, DWORD *serial, DWORD *maxcomp, DWORD *flags, WCHAR *fsname, DWORD fssz)
{
    char a[520], fsA[128]; BOOL r;
    w2a(root, a, 520);
    r = GetVolumeInformationA(root ? a : (char*)0, (char*)0, 0, serial, maxcomp, flags, fsname ? fsA : (char*)0, fssz < 128 ? fssz : 128);
    (void)volname; (void)volsz;
    if (r && fsname) a2w(fsA, fsname, (int)fssz);
    return r;
}
/* ReplaceFile: no libxapi equivalent — emulate (delete dest, rename source over it). */
BOOL ReplaceFileW(const WCHAR *dst, const WCHAR *src, const WCHAR *backup, DWORD flags, void *e1, void *e2)
{
    char d[520], s[520]; (void)backup; (void)flags; (void)e1; (void)e2;
    w2a(dst, d, 520); w2a(src, s, 520);
    DeleteFileA(d);
    return MoveFileA(s, d);
}

/* APIs libxapi lacks entirely — reasonable Xbox fallbacks (single fixed drive D:, no pipes/locks). */
HANDLE GetStdHandle(DWORD id) { return id == (DWORD)-10 ? (HANDLE)3 : id == (DWORD)-12 ? (HANDLE)2 : (HANDLE)1; }
DWORD  GetFileType(HANDLE h)  { return (h == (HANDLE)1 || h == (HANDLE)2 || h == (HANDLE)3) ? 2u /*CHAR*/ : 1u /*DISK*/; }
DWORD  GetDriveTypeW(const WCHAR *root) { (void)root; return 3u; /*DRIVE_FIXED*/ }
DWORD  GetCurrentDirectoryW(DWORD len, WCHAR *buf) { if (buf && len >= 4) { buf[0]='D'; buf[1]=':'; buf[2]='\\'; buf[3]=0; } return 3u; }
BOOL   SetCurrentDirectoryW(const WCHAR *path) { (void)path; return 1; }
DWORD  GetLogicalDriveStringsW(DWORD len, WCHAR *buf) { if (buf && len >= 5) { buf[0]='D'; buf[1]=':'; buf[2]='\\'; buf[3]=0; buf[4]=0; } return 4u; }
BOOL   LockFile(HANDLE h, DWORD ol, DWORD oh, DWORD nl, DWORD nh) { (void)h;(void)ol;(void)oh;(void)nl;(void)nh; return 1; }
BOOL   UnlockFile(HANDLE h, DWORD ol, DWORD oh, DWORD nl, DWORD nh) { (void)h;(void)ol;(void)oh;(void)nl;(void)nh; return 1; }
BOOL   CreatePipe(HANDLE *rd, HANDLE *wr, void *sa, DWORD size) { (void)rd;(void)wr;(void)sa;(void)size; return 0; /*unsupported*/ }
BOOL   CancelIoEx(HANDLE h, void *ovl) { (void)h; (void)ovl; return 1; }

/* -------------------------------------------------------------------------------------------------
 * CDECL wrappers for corefx's DIRECT kernel32 P/Invokes (System.IO.FileSystem: File/Directory and
 * the FileStream ctor's existence check), resolved through our mono_dl fallback (registered in
 * win32_supplement.c). These are cdecl on purpose: for its SetLastError=true DllImports the mono
 * pinvoke wrapper on this build cleans the args itself (caller-cleanup), so a stdcall callee double-
 * cleans the stack and corrupts the return (observed as a jump to IP=1). They wrap the cdecl W->A
 * thunks / real libxapi handle-ops above. Directory enumeration additionally uses ntdll
 * NtQueryDirectoryFile, which is not wired yet — Directory.GetFiles is a known follow-up. */
extern BOOL   __attribute__((__stdcall__)) MoveFileExA(const char*, const char*, DWORD);
extern BOOL   __attribute__((__stdcall__)) CopyFileA(const char*, const char*, BOOL);
extern BOOL   __attribute__((__stdcall__)) SetEndOfFile(HANDLE);
extern BOOL   __attribute__((__stdcall__)) FlushFileBuffers(HANDLE);
extern BOOL   __attribute__((__stdcall__)) ReadFile(HANDLE, void*, DWORD, DWORD*, void*);
extern BOOL   __attribute__((__stdcall__)) WriteFile(HANDLE, const void*, DWORD, DWORD*, void*);
extern DWORD  __attribute__((__stdcall__)) SetFilePointer(HANDLE, long, long*, DWORD);

DWORD  sc_GetFileAttributesExW(const WCHAR *n, int lvl, void *info) { return GetFileAttributesExW(n, lvl, info); }
HANDLE sc_CreateFileW(const WCHAR *n, DWORD a, DWORD s, void *sa, DWORD d, DWORD f, void *t) { return CreateFileW(n, a, s, sa, d, f, t); }
BOOL   sc_CreateDirectoryW(const WCHAR *n, void *sa) { return CreateDirectoryW(n, sa); }
BOOL   sc_RemoveDirectoryW(const WCHAR *n) { return RemoveDirectoryW(n); }
BOOL   sc_DeleteFileW(const WCHAR *n) { return DeleteFileW(n); }
BOOL   sc_SetFileAttributesW(const WCHAR *n, DWORD attrs) { return SetFileAttributesW(n, attrs); }
DWORD  sc_GetCurrentDirectoryW(DWORD len, WCHAR *buf) { return GetCurrentDirectoryW(len, buf); }
BOOL   sc_SetCurrentDirectoryW(const WCHAR *p) { return SetCurrentDirectoryW(p); }
BOOL   sc_FindNextFileW(HANDLE h, void *wfd) { return FindNextFileW(h, (RXDK_FIND_W*)wfd); }
BOOL   sc_MoveFileExW(const WCHAR *src, const WCHAR *dst, DWORD flags) { char a[520], b[520]; w2a(src, a, 520); w2a(dst, b, 520); return MoveFileExA(a, b, flags); }
BOOL   sc_CopyFileExW(const WCHAR *src, const WCHAR *dst, void *prog, void *data, void *cancel, DWORD flags)
{ char a[520], b[520]; (void)prog; (void)data; (void)cancel; w2a(src, a, 520); w2a(dst, b, 520); return CopyFileA(a, b, (flags & 0x1) ? 1 : 0); }
BOOL   sc_ReplaceFileW(const WCHAR *dst, const WCHAR *src, const WCHAR *bak, DWORD f, void *e1, void *e2) { return ReplaceFileW(dst, src, bak, f, e1, e2); }
HANDLE sc_FindFirstFileExW(const WCHAR *name, int lvl, void *fd, int op, void *filt, DWORD flags)
{ (void)lvl; (void)op; (void)filt; (void)flags; return FindFirstFileW(name, (RXDK_FIND_W*)fd); }
BOOL   sc_FindClose(HANDLE h) { return CloseHandle(h); }   /* FindClose is a macro -> CloseHandle */
BOOL   sc_SetEndOfFile(HANDLE h) { return SetEndOfFile(h); }
BOOL   sc_FlushFileBuffers(HANDLE h) { return FlushFileBuffers(h); }
BOOL   sc_CloseHandle(HANDLE h) { return CloseHandle(h); }
BOOL   sc_ReadFile(HANDLE h, void *b, DWORD n, DWORD *rd, void *o) { return ReadFile(h, b, n, rd, o); }
BOOL   sc_WriteFile(HANDLE h, const void *b, DWORD n, DWORD *wr, void *o) { return WriteFile(h, b, n, wr, o); }
DWORD  sc_SetFilePointer(HANDLE h, long dist, long *high, DWORD method) { return SetFilePointer(h, dist, high, method); }
