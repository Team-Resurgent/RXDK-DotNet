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

/* Directory enumeration: libxapi's FindFirstFile(A) faults on the Xbox. corefx Directory.GetFiles
 * does not use it — it opens the directory with CreateFile (FILE_FLAG_BACKUP_SEMANTICS) and pages
 * entries with ntdll!NtQueryDirectoryFile. These FindFirst/FindNext thunks stay a clean failure for
 * the few Mono w32file callers that still hit them. */
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
 * WIN32_FILE_ATTRIBUTE_DATA ([0]=attrs, [1..6]=3 FILETIMEs, [7]=sizeHigh, [8]=sizeLow) from attrs.
 * Non-directories get their size from GetFileSize on a normal read open (the same open FileStream
 * already uses). Times stay zero. */
BOOL GetFileAttributesExW(const WCHAR *name, int level, void *info)
{
    char a[520]; DWORD attrs; DWORD *d = (DWORD *)info; (void)level;
    w2a(name, a, 520);
    attrs = GetFileAttributesA(a);
    if (attrs == 0xFFFFFFFFu) { SetLastError(2 /*ERROR_FILE_NOT_FOUND*/); return 0; }
    for (int i = 0; i < 9; ++i) d[i] = 0;
    d[0] = attrs;
    if ((attrs & 0x10u) == 0) { /* not a directory — FileInfo.Length reads sizeHigh/sizeLow */
        HANDLE h = CreateFileA(a, 0x80000000u /*GENERIC_READ*/, 1 /*FILE_SHARE_READ*/, 0,
                               3 /*OPEN_EXISTING*/, 0x80 /*FILE_ATTRIBUTE_NORMAL*/, 0);
        if (h != RXDK_INVALID_HANDLE) {
            DWORD hi = 0, lo = GetFileSize(h, &hi);
            if (lo != 0xFFFFFFFFu) { d[7] = hi; d[8] = lo; }
            CloseHandle(h);
        }
    }
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
 * thunks / real libxapi handle-ops above. ntdll NtQueryDirectoryFile / NtCreateFile have no
 * SetLastError, so their wrappers below are stdcall (Winapi), matching GetTimeZoneInformation. */
extern BOOL   __attribute__((__stdcall__)) MoveFileExA(const char*, const char*, DWORD);
extern BOOL   __attribute__((__stdcall__)) CopyFileA(const char*, const char*, BOOL);
extern BOOL   __attribute__((__stdcall__)) SetEndOfFile(HANDLE);
extern BOOL   __attribute__((__stdcall__)) FlushFileBuffers(HANDLE);
extern BOOL   __attribute__((__stdcall__)) ReadFile(HANDLE, void*, DWORD, DWORD*, void*);
extern BOOL   __attribute__((__stdcall__)) WriteFile(HANDLE, const void*, DWORD, DWORD*, void*);
extern DWORD  __attribute__((__stdcall__)) SetFilePointer(HANDLE, long, long*, DWORD);

DWORD  sc_GetFileAttributesExW(const WCHAR *n, int lvl, void *info) { return GetFileAttributesExW(n, lvl, info); }

/* -------------------------------------------------------------------------------------------------
 * ntdll directory enumeration.
 *
 * corefx FileSystemEnumerator opens a directory with CreateFileW(FILE_FLAG_BACKUP_SEMANTICS), then
 * pages FILE_FULL_DIR_INFORMATION via NtQueryDirectoryFile. The Xbox kernel exports that call, but
 * its signature drops Windows' ReturnSingleEntry argument and takes an ANSI OBJECT_STRING filter.
 * NtCreateFile likewise uses the Xbox OBJECT_ATTRIBUTES (Root, ANSI name, Attributes) and has no
 * EaBuffer/EaLength. These wrappers speak the Windows signatures corefx P/Invokes and call the
 * kernel with the Xbox ones. They are stdcall: the DllImports do not set SetLastError.
 */
typedef struct { unsigned long Status; unsigned long Information; } RXDK_IOSB;
typedef struct { unsigned long Length; void *RootDirectory; void *ObjectName; unsigned long Attributes; void *Sd; void *Qos; } RXDK_WIN_OA;
typedef struct { unsigned short Length; unsigned short MaximumLength; WCHAR *Buffer; } RXDK_WIN_USTR;
typedef struct { unsigned short Length; unsigned short MaximumLength; char *Buffer; } RXDK_ANSI;
typedef struct { void *RootDirectory; RXDK_ANSI *ObjectName; unsigned long Attributes; } RXDK_XBOX_OA;

extern long __attribute__((__stdcall__)) NtQueryDirectoryFile(void*, void*, void*, void*, RXDK_IOSB*, void*, unsigned long, int, void*, int);
extern long __attribute__((__stdcall__)) NtCreateFile(void**, unsigned long, RXDK_XBOX_OA*, RXDK_IOSB*, void*, unsigned long, unsigned long, unsigned long, unsigned long);
extern unsigned long __attribute__((__stdcall__)) RtlNtStatusToDosError(long status);
extern void *malloc(unsigned int);
extern void  free(void *);

/* corefx Marshal.AllocHGlobal -> HeapAlloc(GetProcessHeap()). The import is
 * api-ms-win-core-heap, which the Xbox has no loader for; a NULL HeapAlloc becomes
 * OutOfMemoryException before enumeration ever calls NtQueryDirectoryFile. */
static void *rxdk_process_heap = (void *)0x48454150;
void *__attribute__((__stdcall__)) sc_GetProcessHeap(void) { return rxdk_process_heap; }
void *__attribute__((__stdcall__)) sc_HeapAlloc(void *heap, unsigned long flags, unsigned long bytes)
{
    void *p;
    (void)heap;
    if (bytes == 0) bytes = 1;
    p = malloc(bytes);
    if (p && (flags & 8u)) { unsigned long i; char *c = (char *)p; for (i = 0; i < bytes; ++i) c[i] = 0; }
    return p;
}
int __attribute__((__stdcall__)) sc_HeapFree(void *heap, unsigned long flags, void *mem)
{
    (void)heap; (void)flags;
    free(mem);
    return 1;
}

#define RXDK_FILE_FLAG_BACKUP_SEMANTICS 0x02000000u
#define RXDK_SYNCHRONIZE                0x00100000u
#define RXDK_FILE_OPEN                  1
#define RXDK_FILE_CREATE                2
#define RXDK_FILE_OPEN_IF               3
#define RXDK_FILE_OVERWRITE             4
#define RXDK_FILE_OVERWRITE_IF          5
#define RXDK_FILE_DIRECTORY_FILE        0x00000001u
#define RXDK_FILE_SYNCHRONOUS_IO_NONALERT 0x00000020u
#define RXDK_FILE_OPEN_FOR_BACKUP_INTENT  0x00004000u
#define RXDK_OBJ_CASE_INSENSITIVE       0x00000040u
#define RXDK_STATUS_SUCCESS             0

static int win32_to_nt_disp(DWORD disp)
{
    switch (disp) {
    case 1: return RXDK_FILE_CREATE;       /* CREATE_NEW */
    case 2: return RXDK_FILE_OVERWRITE_IF; /* CREATE_ALWAYS */
    case 4: return RXDK_FILE_OPEN_IF;      /* OPEN_ALWAYS */
    case 5: return RXDK_FILE_OVERWRITE;    /* TRUNCATE_EXISTING */
    default: return RXDK_FILE_OPEN;        /* OPEN_EXISTING */
    }
}

/* D:\foo -> \??\D:\foo. NT paths (\??\ or \Device\) pass through. */
static void to_nt_path(const char *win, char *out, int n)
{
    int i = 0, j;
    if (!win) { if (n > 0) out[0] = 0; return; }
    if (win[0] == '\\') {
        for (; i < n - 1 && win[i]; ++i) out[i] = win[i];
        out[i] = 0;
        return;
    }
    if (n > 4) { out[0] = '\\'; out[1] = '?'; out[2] = '?'; out[3] = '\\'; i = 4; }
    for (j = 0; i < n - 1 && win[j]; ++j, ++i) out[i] = win[j];
    out[i] = 0;
}

/* Open a directory the kernel way. CreateFileA does not accept FILE_FLAG_BACKUP_SEMANTICS. */
static HANDLE rxdk_open_directory(const WCHAR *name, DWORD access, DWORD share, DWORD disp)
{
    char win[520], nt[520];
    RXDK_ANSI as;
    RXDK_XBOX_OA oa;
    RXDK_IOSB iosb;
    void *h = 0;
    long st;
    int i;
    w2a(name, win, 520);
    to_nt_path(win, nt, 520);
    for (i = 0; nt[i]; ++i) {}
    as.Length = (unsigned short)i;
    as.MaximumLength = (unsigned short)(i + 1);
    as.Buffer = nt;
    oa.RootDirectory = 0;
    oa.ObjectName = &as;
    oa.Attributes = RXDK_OBJ_CASE_INSENSITIVE;
    iosb.Status = 0; iosb.Information = 0;
    st = NtCreateFile(&h, access | RXDK_SYNCHRONIZE, &oa, &iosb, 0, 0, share,
                      (unsigned long)win32_to_nt_disp(disp),
                      RXDK_FILE_DIRECTORY_FILE | RXDK_FILE_SYNCHRONOUS_IO_NONALERT | RXDK_FILE_OPEN_FOR_BACKUP_INTENT);
    if (st != RXDK_STATUS_SUCCESS || !h) {
        SetLastError(RtlNtStatusToDosError(st));
        return RXDK_INVALID_HANDLE;
    }
    return h;
}

HANDLE sc_CreateFileW(const WCHAR *n, DWORD a, DWORD s, void *sa, DWORD d, DWORD f, void *t)
{
    if (f & RXDK_FILE_FLAG_BACKUP_SEMANTICS)
        return rxdk_open_directory(n, a, s, d);
    return CreateFileW(n, a, s, sa, d, f, t);
}

/* Windows NtQueryDirectoryFile has a ReturnSingleEntry argument the Xbox kernel does not.
 * corefx passes FileName == NULL (it filters in managed code), so the ANSI filter stays NULL. */
#define RXDK_STATUS_NO_MORE_FILES  ((long)0x80000006)
#define RXDK_STATUS_BUFFER_OVERFLOW ((long)0x80000005)
#define RXDK_STATUS_NO_MEMORY       ((long)0xC0000017)
#define RXDK_FILE_DIRECTORY_INFORMATION 1
#define RXDK_FILE_FULL_DIR_INFORMATION  2

/* FileFullDirectoryInformation (class 2) is STATUS_INVALID_INFO_CLASS on the Xbox. The kernel
 * enumerates with FileDirectoryInformation (class 1, ANSI names, no EaSize). corefx only understands
 * the wide FILE_FULL_DIR_INFORMATION layout, so expand class 1 into that buffer. */
static long rxdk_query_directory_full(void *file, void *event, void *apc, void *ctx,
    RXDK_IOSB *iosb, unsigned char *dst, unsigned long len, int restart)
{
    unsigned long klen = len / 3;
    unsigned char *tmp, *src, *out, *end, *last;
    long st;
    unsigned long written = 0;
    if (klen < 256) klen = len;
    tmp = (unsigned char *)malloc(klen ? klen : 1);
    if (!tmp) return RXDK_STATUS_NO_MEMORY;
    st = NtQueryDirectoryFile(file, event, apc, ctx, iosb, tmp, klen, RXDK_FILE_DIRECTORY_INFORMATION, 0, restart);
    if (st == RXDK_STATUS_BUFFER_OVERFLOW && klen < len) {
        free(tmp);
        klen = len;
        tmp = (unsigned char *)malloc(klen);
        if (!tmp) return RXDK_STATUS_NO_MEMORY;
        st = NtQueryDirectoryFile(file, event, apc, ctx, iosb, tmp, klen, RXDK_FILE_DIRECTORY_INFORMATION, 0, restart);
    }
    if (st != RXDK_STATUS_SUCCESS) { free(tmp); return st; }
    src = tmp;
    out = dst;
    end = dst + len;
    last = 0;
    for (;;) {
        unsigned long next, nameLen, need, aligned, i;
        if (src + 64 > tmp + klen) break;
        next = *(unsigned long *)src;
        nameLen = *(unsigned long *)(src + 60);
        if (nameLen > 260 || src + 64 + nameLen > tmp + klen) break;
        need = 68 + nameLen * 2;
        aligned = (need + 7u) & ~7u;
        if (out + need > end) break;
        for (i = 0; i < 60; ++i) out[i] = src[i];
        *(unsigned long *)(out + 60) = nameLen * 2; /* FileNameLength, wide bytes */
        *(unsigned long *)(out + 64) = 0;           /* EaSize */
        for (i = 0; i < nameLen; ++i) { out[68 + i * 2] = src[64 + i]; out[68 + i * 2 + 1] = 0; }
        for (i = need; i < aligned && out + i < end; ++i) out[i] = 0;
        *(unsigned long *)out = (out + aligned <= end) ? aligned : 0;
        last = out;
        written += (out + aligned <= end) ? aligned : need;
        if (next == 0 || src + next < src || src + next >= tmp + klen) break;
        if (out + aligned > end) break;
        out += aligned;
        src += next;
    }
    if (last) *(unsigned long *)last = 0;
    free(tmp);
    if (!last) return RXDK_STATUS_NO_MORE_FILES;
    if (iosb) { iosb->Status = 0; iosb->Information = written; }
    return RXDK_STATUS_SUCCESS;
}

long __attribute__((__stdcall__)) sc_NtQueryDirectoryFile(
    void *file, void *event, void *apc, void *ctx, RXDK_IOSB *iosb, void *info, unsigned long len,
    int infoClass, int single, void *fileName, int restart)
{
    (void)single; (void)fileName;
    if (infoClass == RXDK_FILE_FULL_DIR_INFORMATION)
        return rxdk_query_directory_full(file, event, apc, ctx, iosb, (unsigned char *)info, len, restart);
    return NtQueryDirectoryFile(file, event, apc, ctx, iosb, info, len, infoClass, 0, restart);
}

/* SetLastError=true, so cdecl. Used when a failed query is turned into an exception message. */
int sc_FormatMessageW(int flags, void *src, unsigned long msgId, int lang, WCHAR *buf, int nSize, void *args)
{
    char tmp[16]; int i = 0, n = 0; unsigned long v = msgId;
    (void)flags; (void)src; (void)lang; (void)args;
    if (!buf || nSize < 2) return 0;
    if (!v) { buf[0] = '0'; buf[1] = 0; return 1; }
    while (v) { tmp[i++] = (char)('0' + v % 10); v /= 10; }
    while (i && n < nSize - 1) buf[n++] = (WCHAR)tmp[--i];
    buf[n] = 0;
    return n;
}

/* Windows NtCreateFile: 11 args and a Windows OBJECT_ATTRIBUTES (UTF-16 name). Used for recursive
 * enumeration (a child directory relative to an already-open parent handle). */
long __attribute__((__stdcall__)) sc_NtCreateFile(
    void **file, unsigned long access, RXDK_WIN_OA *oa, RXDK_IOSB *iosb, void *alloc,
    unsigned long attrs, unsigned long share, unsigned long disp, unsigned long options,
    void *ea, unsigned long eaLen)
{
    RXDK_WIN_USTR *us;
    char abuf[520];
    RXDK_ANSI as;
    RXDK_XBOX_OA xo;
    int n = 0, i;
    (void)ea; (void)eaLen; (void)options;
    if (oa && (us = (RXDK_WIN_USTR *)oa->ObjectName) && us->Buffer) {
        n = (int)us->Length / 2;
        if (n > 519) n = 519;
        for (i = 0; i < n; ++i) abuf[i] = (char)us->Buffer[i];
    }
    abuf[n] = 0;
    as.Length = (unsigned short)n;
    as.MaximumLength = (unsigned short)(n + 1);
    as.Buffer = abuf;
    xo.RootDirectory = oa ? oa->RootDirectory : 0;
    xo.ObjectName = &as;
    xo.Attributes = oa ? oa->Attributes : RXDK_OBJ_CASE_INSENSITIVE;
    return NtCreateFile(file, access, &xo, iosb, alloc, attrs, share, disp,
                        RXDK_FILE_DIRECTORY_FILE | RXDK_FILE_SYNCHRONOUS_IO_NONALERT | RXDK_FILE_OPEN_FOR_BACKUP_INTENT);
}
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
