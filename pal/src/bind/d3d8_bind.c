/* Rxdk.Graphics native bind. This is the only translation unit that takes the
 * address of Direct3D exports. build-host pulls it with -Wl,-u,rxdk_bind_d3d8_register
 * when Rxdk.Graphics.dll is part of the title, and the sidecar adds libd3d8.
 * Do not register these symbols from rxdk_dl_symbol.
 */
#include <xboxkrnl/xboxkrnl.h>
#ifndef NT_INCLUDED
#define NT_INCLUDED
#endif
#include <stdarg.h>
#include <windef.h>
#include <winbase.h>
#include <xbox.h>
#include <stdlib.h>
#include <string.h>

void *sse1_memcpy_nt(void *dst, const void *src, size_t n); /* pal/src/sse1_memcpy.c */

typedef struct {
    unsigned int BackBufferWidth;
    unsigned int BackBufferHeight;
    unsigned int BackBufferFormat;
    unsigned int BackBufferCount;
    unsigned int MultiSampleType;
    unsigned int SwapEffect;
    void *hDeviceWindow;
    int Windowed;
    int EnableAutoDepthStencil;
    unsigned int AutoDepthStencilFormat;
    unsigned int Flags;
    unsigned int FullScreen_RefreshRateInHz;
    unsigned int FullScreen_PresentationInterval;
    void *BufferSurfaces[3];
    void *DepthStencilSurface;
} RxdkPresent;

typedef struct {
    int pitch;
    void *bits;
} RxdkLockedRect;

extern void *__attribute__((stdcall)) Direct3DCreate8(unsigned int sdk);
extern int __attribute__((stdcall)) Direct3D_CreateDevice(
    unsigned int adapter, int type, void *unused, unsigned int flags,
    RxdkPresent *pp, void **device);
extern void __attribute__((stdcall)) D3DDevice_Clear(
    unsigned int count, const void *rects, unsigned int flags,
    unsigned int color, float z, unsigned int stencil);
extern void __attribute__((stdcall)) D3DDevice_SetTransform(int state, const void *matrix);
extern void __attribute__((stdcall)) D3DDevice_GetTransform(int state, void *matrix);
extern void __attribute__((stdcall)) D3DDevice_SetViewport(const void *viewport);
extern void __attribute__((stdcall)) D3DDevice_GetViewport(void *viewport);
extern unsigned int D3D__RenderState[];
extern void __attribute__((stdcall)) D3DDevice_SetRenderStateNotInline(int state, unsigned int value);
/* On Xbox the sampler states are texture stage states: D3DTSS_MINFILTER and friends share the
 * array with the fixed-function blend ops. D3DTSS_MAXSTAGES is 4 and D3DTSS_MAX is 32. */
extern unsigned int D3D__TextureState[4][32];
extern void __attribute__((stdcall)) D3DDevice_SetTextureStageStateNotInline(
    unsigned int stage, int type, unsigned int value);
/* D3DDevice_CreateRenderTarget and CreateDepthStencilSurface are inline in d3d8.h; both reach
 * the same export with a different usage flag. D3DUSAGE_RENDERTARGET is 1, DEPTHSTENCIL is 2. */
extern void *__attribute__((stdcall)) D3DDevice_CreateSurface2(
    unsigned int width, unsigned int height, unsigned int usage, unsigned int format);
extern void *__attribute__((stdcall)) D3DDevice_GetRenderTarget2(void);
extern void *__attribute__((stdcall)) D3DDevice_GetDepthStencilSurface2(void);
extern void __attribute__((stdcall)) D3DDevice_SetRenderTarget(void *color, void *depth);
extern void __attribute__((stdcall)) D3DDevice_BlockUntilIdle(void);
extern void *__attribute__((stdcall)) D3DDevice_CreateVertexBuffer2(unsigned int length);
extern unsigned char *__attribute__((stdcall)) D3DVertexBuffer_Lock2(void *buffer, unsigned int flags);
extern void __attribute__((stdcall)) D3DTexture_GetLevelDesc(void *texture, unsigned int level, void *desc);
extern int __attribute__((stdcall)) D3DXCreateTextureFromFileInMemoryEx(
    void *device, const void *data, unsigned int size,
    unsigned int width, unsigned int height, unsigned int levels,
    unsigned int usage, unsigned int format, unsigned int pool,
    unsigned int filter, unsigned int mip_filter, unsigned int color_key,
    void *info, void *palette, void **texture);
extern int __attribute__((stdcall)) D3DXCreateTextureFromFileExA(
    void *device, const char *path,
    unsigned int width, unsigned int height, unsigned int levels,
    unsigned int usage, unsigned int format, unsigned int pool,
    unsigned int filter, unsigned int mip_filter, unsigned int color_key,
    void *info, void *palette, void **texture);
extern void *__attribute__((stdcall)) D3DDevice_CreateIndexBuffer2(unsigned int length);
extern void *__attribute__((stdcall)) D3DDevice_CreateTexture2(
    unsigned int width, unsigned int height, unsigned int depth, unsigned int levels,
    unsigned int usage, unsigned int format, unsigned int type);
extern void __attribute__((stdcall)) D3DTexture_LockRect(
    void *texture, unsigned int level, RxdkLockedRect *locked, const void *rect, unsigned int flags);
extern unsigned int __attribute__((stdcall)) D3DResource_Release(void *resource);
extern void *__attribute__((stdcall)) D3DTexture_GetSurfaceLevel2(void *texture, unsigned int level);
extern int __attribute__((stdcall)) D3DXCreateCubeTextureFromFileA(void *device, const char *path, void **texture);
extern int __attribute__((stdcall)) D3DXCreateCubeTextureFromFileInMemory(void *device, const void *data, unsigned int size, void **texture);
extern int __attribute__((stdcall)) D3DXCreateVolumeTexture(
    void *device, unsigned int width, unsigned int height, unsigned int depth, unsigned int levels,
    unsigned int usage, unsigned int format, unsigned int pool, void **texture);
extern int __attribute__((stdcall)) XGIsSwizzledFormat(unsigned int format);
extern unsigned int __attribute__((stdcall)) XGBytesPerPixelFromFormat(unsigned int format);
extern void __attribute__((stdcall)) XGUnswizzleRect(
    const void *source, unsigned int width, unsigned int height, const void *rect, void *dest,
    unsigned int pitch, const void *point, unsigned int bytes_per_pixel);
extern void __attribute__((stdcall)) XGSwizzleBox(
    const void *source, unsigned int row_pitch, unsigned int slice_pitch, const void *box, void *dest,
    unsigned int width, unsigned int height, unsigned int depth, const void *point, unsigned int bytes_per_pixel);
extern void __attribute__((stdcall)) XGUnswizzleBox(
    const void *source, unsigned int width, unsigned int height, unsigned int depth, const void *box, void *dest,
    unsigned int row_pitch, unsigned int slice_pitch, const void *point, unsigned int bytes_per_pixel);
extern int __attribute__((stdcall)) XGCompileDrawIndexedVertices(void *buffer, unsigned int *size, int type, unsigned int count, const void *indices);
extern int __attribute__((stdcall)) XGCompressRect(
    void *dest, unsigned int dest_format, unsigned int dest_pitch, unsigned int width, unsigned int height,
    void *source, unsigned int source_format, unsigned int source_pitch, float alpha_ref, unsigned int flags);
extern void __attribute__((stdcall)) XGSetSurfaceHeader(unsigned int w, unsigned int h, unsigned int format, void *surface, unsigned int data, unsigned int pitch);
extern void __attribute__((stdcall)) XGSetTextureHeader(
    unsigned int w, unsigned int h, unsigned int levels, unsigned int usage, unsigned int format, unsigned int pool,
    void *texture, unsigned int data, unsigned int pitch);
extern void __attribute__((stdcall)) XGSetCubeTextureHeader(
    unsigned int edge, unsigned int levels, unsigned int usage, unsigned int format, unsigned int pool,
    void *texture, unsigned int data, unsigned int pitch);
extern void __attribute__((stdcall)) XGSetVolumeTextureHeader(
    unsigned int w, unsigned int h, unsigned int depth, unsigned int levels, unsigned int usage, unsigned int format,
    unsigned int pool, void *texture, unsigned int data, unsigned int pitch);
extern void __attribute__((stdcall)) XGSetVertexBufferHeader(
    unsigned int length, unsigned int usage, unsigned int fvf, unsigned int pool, void *buffer, unsigned int data);
extern void __attribute__((stdcall)) XGSetIndexBufferHeader(
    unsigned int length, unsigned int usage, unsigned int format, unsigned int pool, void *buffer, unsigned int data);
extern void __attribute__((stdcall)) XGSetPaletteHeader(unsigned int size, void *palette, unsigned int data);
extern void __attribute__((stdcall)) XGSetPushBufferHeader(unsigned int size, int cpu_copy, void *buffer, unsigned int data);
extern void __attribute__((stdcall)) XGSetFixupHeader(unsigned int size, void *fixup, unsigned int data);
extern int __attribute__((stdcall)) XGWriteSurfaceToFile(void *surface, const char *path);
extern int __attribute__((stdcall)) XGWriteSurfaceOrTextureToXPR(void *resource, const char *path, int as_texture);
extern int __attribute__((stdcall)) XGCompileShader(
    const char *file, const void *source, unsigned int source_len, unsigned int flags,
    const char *entry, const char *target, void **constants, void **shader, void **errors,
    void **listing, void **machine, void *resolver, void *resolver_data, unsigned int *shader_type);
extern int __attribute__((stdcall)) XGSpliceVertexShaders(
    void *shader, unsigned int *size, unsigned int *instructions, const void **shaders, unsigned int count, int optimize);
extern unsigned int __attribute__((stdcall)) XGSUCode_GetVertexShaderType(const void *microcode);
extern unsigned int __attribute__((stdcall)) XGSUCode_GetVertexShaderLength(const void *microcode);
extern int __attribute__((stdcall)) XGSUCode_CompareVertexShaders(const void *a, const void *b, void **error_log);
extern int __attribute__((stdcall)) XGBufferCreate(unsigned int bytes, void **buffer);
extern unsigned int __attribute__((stdcall)) XGBuffer_Release(void *buffer);
extern void *__attribute__((stdcall)) XGBuffer_GetBufferPointer(void *buffer);
extern unsigned int __attribute__((stdcall)) XGBuffer_GetBufferSize(void *buffer);
extern void __attribute__((stdcall)) D3DDevice_SetStreamSource(unsigned int stream, void *buffer, unsigned int stride);
extern void __attribute__((stdcall)) D3DDevice_SetIndices(void *buffer, unsigned int base_vertex);
extern void __attribute__((stdcall)) D3DDevice_SetVertexShader(unsigned int handle);
extern int __attribute__((stdcall)) D3DDevice_CreateVertexShader(const void *decl, const void *function, unsigned int *handle, unsigned int usage);
extern void __attribute__((stdcall)) D3DDevice_GetVertexShader(unsigned int *handle);
extern void __attribute__((stdcall)) D3DDevice_DeleteVertexShader(unsigned int handle);
/* D3DFASTCALL. Only ever called from C; the managed side sees the cdecl wrapper. */
extern void __attribute__((fastcall)) D3DDevice_SetVertexShaderConstantNotInline(int reg, const void *data, unsigned int count);
extern void __attribute__((stdcall)) D3DDevice_GetVertexShaderConstant(int reg, void *data, unsigned int count);
extern void __attribute__((stdcall)) D3DDevice_LoadVertexShader(unsigned int handle, unsigned int address);
extern void __attribute__((stdcall)) D3DDevice_LoadVertexShaderProgram(const void *function, unsigned int address);
extern void __attribute__((stdcall)) D3DDevice_SelectVertexShader(unsigned int handle, unsigned int address);
extern void __attribute__((stdcall)) D3DDevice_CreatePixelShader(const void *def, unsigned int *handle);
extern void __attribute__((stdcall)) D3DDevice_SetPixelShader(unsigned int handle);
extern void __attribute__((stdcall)) D3DDevice_GetPixelShader(unsigned int *handle);
extern void __attribute__((stdcall)) D3DDevice_DeletePixelShader(unsigned int handle);
extern void __attribute__((stdcall)) D3DDevice_SetPixelShaderConstant(unsigned int reg, const void *data, unsigned int count);
extern void __attribute__((stdcall)) D3DDevice_GetPixelShaderConstant(unsigned int reg, void *data, unsigned int count);
extern void __attribute__((stdcall)) D3DDevice_SetPixelShaderProgram(const void *def);
extern int __attribute__((stdcall)) XGAssembleShader(
    const char *file, const void *source, unsigned int source_len, unsigned int flags,
    void **constants, void **shader, void **errors, void **listing,
    void *resolver, void *resolver_data, unsigned int *shader_type);
extern void __attribute__((stdcall)) D3DDevice_SetTexture(unsigned int stage, void *texture);
extern void __attribute__((stdcall)) D3DDevice_DrawVertices(int type, unsigned int start, unsigned int count);
extern void __attribute__((stdcall)) D3DDevice_DrawIndexedVertices(int type, unsigned int count, const unsigned short *indices);
extern unsigned int __attribute__((stdcall)) D3DDevice_Swap(unsigned int flags);
extern unsigned int __attribute__((stdcall)) D3DDevice_Release(void);
extern unsigned short *D3D__IndexData;

typedef void *(*RxdkDlLoad)(const char *name, int flags, char **err, void *ud);
typedef void *(*RxdkDlSymbol)(void *handle, const char *name, char **err, void *ud);
typedef void *(*RxdkDlClose)(void *handle, void *ud);
extern void *mono_dl_fallback_register(RxdkDlLoad, RxdkDlSymbol, RxdkDlClose, void *);

static int device_open;
static void *d3d_device;

extern void __attribute__((stdcall)) XGSwizzleRect(
    const void *source, unsigned int pitch, const void *rect, void *dest,
    unsigned int width, unsigned int height, const void *point, unsigned int bytes_per_pixel);

typedef struct {
    unsigned int format;
    unsigned int type;
    unsigned int usage;
    unsigned int size;
    unsigned int samples;
    unsigned int width;
    unsigned int height;
} RxdkLevelDesc;

static unsigned int rxdk_gfx_video_flags(void)
{
    return (unsigned int)XGetVideoFlags();
}

static unsigned int rxdk_gfx_video_standard(void)
{
    return (unsigned int)XGetVideoStandard();
}

static unsigned int vertex_count(int type, int primitives)
{
    switch (type) {
    case 1: return (unsigned int)primitives;
    case 2: return (unsigned int)primitives * 2;
    case 3: case 4: return (unsigned int)primitives + 1;
    case 5: return (unsigned int)primitives * 3;
    case 6: case 7: return (unsigned int)primitives + 2;
    case 8: return (unsigned int)primitives * 4;
    case 9: return (unsigned int)primitives * 2 + 2;
    default: return 0;
    }
}

static int rxdk_gfx_open(int width, int height, int progressive, int widescreen, int refresh_hz)
{
    RxdkPresent pp;
    void *device;
    int hr;
    if (device_open)
        return 0;
    memset(&pp, 0, sizeof(pp));
    pp.BackBufferWidth = (unsigned int)width;
    pp.BackBufferHeight = (unsigned int)height;
    pp.BackBufferFormat = 7; /* D3DFMT_X8R8G8B8 */
    pp.BackBufferCount = 1;
    pp.SwapEffect = 1; /* D3DSWAPEFFECT_DISCARD */
    pp.EnableAutoDepthStencil = 1;
    pp.AutoDepthStencilFormat = 0x2A; /* D3DFMT_D24S8 */
    pp.FullScreen_RefreshRateInHz = (unsigned int)refresh_hz;
    pp.Flags = progressive ? 0x40 : 0x20;
    if (widescreen)
        pp.Flags |= 0x10;
    Direct3DCreate8(0);
    device = NULL;
    hr = Direct3D_CreateDevice(0, 1, NULL, 0x40, &pp, &device);
    if (hr < 0)
        return (int)hr;
    d3d_device = device;
    device_open = 1;
    /* The front and back buffers start as whatever was in memory, and the front one is
       scanned out until the title's first frame, so blank both. */
    for (int i = 0; i < 2; i++) {
        D3DDevice_Clear(0, NULL, 0x7 /* target | zbuffer | stencil */, 0xFF000000, 1.0f, 0);
        D3DDevice_Swap(0);
    }
    return 0;
}

typedef struct {
    float m[16];
} __attribute__((aligned(16))) RxdkMatrix;

static void apply_matrix(int state, const float *m)
{
    RxdkMatrix tmp;
    if (!m)
        return;
    memcpy(&tmp, m, sizeof(tmp));
    D3DDevice_SetTransform(state, &tmp);
}

static void describe_texture(void *texture, int *width, int *height, int *format)
{
    RxdkLevelDesc desc;
    memset(&desc, 0, sizeof(desc));
    D3DTexture_GetLevelDesc(texture, 0, &desc);
    if (width)
        *width = (int)desc.width;
    if (height)
        *height = (int)desc.height;
    if (format)
        *format = (int)desc.format;
}

static void *rxdk_gfx_texture_from_memory_ex(
    const void *data, int size, int width, int height, int levels, int format,
    int *out_width, int *out_height, int *out_format)
{
    void *texture = NULL;
    int hr;
    if (!d3d_device || !data || size <= 0)
        return NULL;
    hr = D3DXCreateTextureFromFileInMemoryEx(
        d3d_device, data, (unsigned int)size,
        (unsigned int)width, (unsigned int)height, (unsigned int)levels,
        0, (unsigned int)format, 0, 0xFFFFFFFFu, 0xFFFFFFFFu, 0, NULL, NULL, &texture);
    if (hr < 0 || !texture)
        return NULL;
    describe_texture(texture, out_width, out_height, out_format);
    return texture;
}

static void *rxdk_gfx_cube_from_memory(const void *data, int size)
{
    void *texture = NULL;
    int hr;
    if (!d3d_device || !data || size <= 0)
        return NULL;
    hr = D3DXCreateCubeTextureFromFileInMemory(d3d_device, data, (unsigned int)size, &texture);
    return hr < 0 ? NULL : texture;
}

static void *rxdk_gfx_cube_from_file(const char *path)
{
    void *texture = NULL;
    int hr;
    if (!d3d_device || !path)
        return NULL;
    hr = D3DXCreateCubeTextureFromFileA(d3d_device, path, &texture);
    return hr < 0 ? NULL : texture;
}

static void *rxdk_gfx_volume_create(int width, int height, int depth, int levels, int format)
{
    void *texture = NULL;
    int hr;
    if (!d3d_device || width <= 0 || height <= 0 || depth <= 0)
        return NULL;
    hr = D3DXCreateVolumeTexture(
        d3d_device, (unsigned int)width, (unsigned int)height, (unsigned int)depth,
        (unsigned int)levels, 0, (unsigned int)format, 0, &texture);
    return hr < 0 ? NULL : texture;
}

static void *rxdk_gfx_texture_from_file_ex(
    const char *path, int width, int height, int levels, int format,
    int *out_width, int *out_height, int *out_format)
{
    void *texture = NULL;
    int hr;
    if (!d3d_device || !path)
        return NULL;
    hr = D3DXCreateTextureFromFileExA(
        d3d_device, path,
        (unsigned int)width, (unsigned int)height, (unsigned int)levels,
        0, (unsigned int)format, 0, 0xFFFFFFFFu, 0xFFFFFFFFu, 0, NULL, NULL, &texture);
    if (hr < 0 || !texture)
        return NULL;
    describe_texture(texture, out_width, out_height, out_format);
    return texture;
}

static void rxdk_gfx_model(const float *m) { apply_matrix(6, m); }
static void rxdk_gfx_view(const float *m) { apply_matrix(0, m); }
static void rxdk_gfx_projection(const float *m) { apply_matrix(1, m); }

static void rxdk_gfx_get_matrix(int state, float *m)
{
    __attribute__((aligned(16))) unsigned char tmp[64];
    if (!m)
        return;
    D3DDevice_GetTransform(state, tmp);
    memcpy(m, tmp, 64);
}

static void rxdk_gfx_set_viewport(const void *viewport)
{
    if (viewport)
        D3DDevice_SetViewport(viewport);
}

static void rxdk_gfx_get_viewport(void *viewport)
{
    if (viewport)
        D3DDevice_GetViewport(viewport);
}

static unsigned int rxdk_gfx_get_render_state(int state)
{
    if (state < 0 || state >= 166)
        return 0;
    return D3D__RenderState[state];
}

static void rxdk_gfx_render_state(int state, unsigned int value)
{
    D3DDevice_SetRenderStateNotInline(state, value);
}

static unsigned int rxdk_gfx_get_texture_stage_state(int stage, int state)
{
    if (stage < 0 || stage >= 4 || state < 0 || state >= 32)
        return 0;
    return D3D__TextureState[stage][state];
}

static void rxdk_gfx_texture_stage_state(int stage, int state, unsigned int value)
{
    if (stage < 0 || stage >= 4)
        return;
    D3DDevice_SetTextureStageStateNotInline((unsigned int)stage, state, value);
}

static void *rxdk_gfx_create_vertex_buffer(int size)
{
    return D3DDevice_CreateVertexBuffer2((unsigned int)size);
}

static void rxdk_gfx_sfence(void)
{
    __asm__ __volatile__("sfence" ::: "memory");
}

/* Morton order the NV2A sampler uses. Ordinary stores, so the result can be copied
 * onto write-combined memory and fenced. XGSwizzleRect writes with movntps and never
 * fences, which leaves the GPU sampling a mix of the new texels and the previous
 * contents of the page. */
static void rxdk_gfx_swizzle(const unsigned char *src, unsigned char *dst, int width, int height,
    int bytes_per_pixel)
{
    unsigned int mask_x = 0, mask_y = 0, bit = 1, mask_bit = 1;
    int x, y, off_y;

    while (bit < (unsigned int)width || bit < (unsigned int)height) {
        if (bit < (unsigned int)width) {
            mask_x |= mask_bit;
            mask_bit <<= 1;
        }
        if (bit < (unsigned int)height) {
            mask_y |= mask_bit;
            mask_bit <<= 1;
        }
        bit <<= 1;
    }

    off_y = 0;
    for (y = 0; y < height; y++) {
        int off_x = 0;
        const unsigned char *row = src + (size_t)y * (size_t)width * (size_t)bytes_per_pixel;
        for (x = 0; x < width; x++) {
            memcpy(dst + ((size_t)off_y + (size_t)off_x) * (size_t)bytes_per_pixel,
                row + (size_t)x * (size_t)bytes_per_pixel, (size_t)bytes_per_pixel);
            off_x = (off_x - (int)mask_x) & (int)mask_x;
        }
        off_y = (off_y - (int)mask_y) & (int)mask_y;
    }
}

/* flags 0 waits for the GPU to stop reading the buffer. D3DLOCK_NOOVERWRITE (0x20) does not wait,
 * for a caller that only writes a range no pending draw uses. */
static void rxdk_gfx_vertex_write_ex(void *buffer, const void *src, int offset, int size, unsigned int flags)
{
    unsigned char *dst;
    if (!buffer || !src || size <= 0)
        return;
    dst = D3DVertexBuffer_Lock2(buffer, flags);
    /* The CPU never reads vertex data back, so stream it past the cache. */
    sse1_memcpy_nt(dst + offset, src, (size_t)size);
    rxdk_gfx_sfence();
}

static void rxdk_gfx_vertex_write(void *buffer, const void *src, int offset, int size)
{
    rxdk_gfx_vertex_write_ex(buffer, src, offset, size, 0);
}

static void *rxdk_gfx_create_index_buffer(int size)
{
    return D3DDevice_CreateIndexBuffer2((unsigned int)size);
}

static void rxdk_gfx_index_write(void *buffer, const void *src, int offset, int size)
{
    unsigned int data;
    if (!buffer || !src || size <= 0)
        return;
    data = ((unsigned int *)buffer)[1];
    memcpy((unsigned char *)data + offset, src, (size_t)size);
}

static void *rxdk_gfx_create_texture(int width, int height, int format)
{
    return D3DDevice_CreateTexture2((unsigned int)width, (unsigned int)height, 1, 1, 0,
        (unsigned int)format, 3);
}

/* Mip-capable creation. rxdk_gfx_create_texture stays as the one-level case it always was. */
static void *rxdk_gfx_create_texture_ex(int width, int height, int levels, int format)
{
    return D3DDevice_CreateTexture2((unsigned int)width, (unsigned int)height, 1,
        (unsigned int)(levels <= 0 ? 1 : levels), 0, (unsigned int)format, 3);
}

/* Writes one mip level. A swizzled level goes through XGSwizzleRect; a linear one is copied row by
 * row so the surface's own pitch is honoured. A compressed level uses the linear path, described as
 * its grid of block rows: one byte per element, the row as long as that row of blocks is. */
static void rxdk_gfx_texture_write_level(void *texture, int level, const void *src, int width,
    int height, int bytes_per_pixel, int linear)
{
    RxdkLockedRect locked;
    if (!texture || !src || width <= 0 || height <= 0)
        return;
    D3DTexture_LockRect(texture, (unsigned int)level, &locked, NULL, 0);
    /* LockRect is void on Xbox. A rejected lock comes back as a null pointer rather than a status,
     * and writing through it would not land in the texture. */
    if (!locked.bits || (linear && locked.pitch < width * bytes_per_pixel))
        return;
    if (linear)
    {
        int y, row = width * bytes_per_pixel;
        for (y = 0; y < height; y++)
            memcpy((unsigned char *)locked.bits + y * locked.pitch,
                (const unsigned char *)src + y * row, (size_t)row);
    }
    else
    {
        size_t bytes = (size_t)width * (size_t)height * (size_t)bytes_per_pixel;
        unsigned char *swizzled = (unsigned char *)malloc(bytes);
        if (!swizzled)
            return;
        rxdk_gfx_swizzle(src, swizzled, width, height, bytes_per_pixel);
        memset(locked.bits, 0, bytes);
        memcpy(locked.bits, swizzled, bytes);
        free(swizzled);
    }
    rxdk_gfx_sfence();
}

static void rxdk_gfx_texture_write(void *texture, const void *src, int width, int height,
    int bytes_per_pixel, int linear)
{
    rxdk_gfx_texture_write_level(texture, 0, src, width, height, bytes_per_pixel, linear);
}

/* Reads one mip level back out. Like the write above, a compressed level comes through the linear
 * path framed as its grid of block rows. */
static void rxdk_gfx_texture_read_level(void *texture, int level, void *dest, int width,
    int height, int bytes_per_pixel, int linear)
{
    RxdkLockedRect locked;
    if (!texture || !dest || width <= 0 || height <= 0)
        return;
    D3DTexture_LockRect(texture, (unsigned int)level, &locked, NULL, 0);
    if (linear)
    {
        int y, row = width * bytes_per_pixel;
        for (y = 0; y < height; y++)
            memcpy((unsigned char *)dest + y * row,
                (const unsigned char *)locked.bits + y * locked.pitch, (size_t)row);
    }
    else
        XGUnswizzleRect(locked.bits, (unsigned int)width, (unsigned int)height, NULL, dest,
            (unsigned int)(width * bytes_per_pixel), NULL, (unsigned int)bytes_per_pixel);
}

static void rxdk_gfx_release(void *resource)
{
    if (resource)
        D3DResource_Release(resource);
}

static void rxdk_gfx_set_stream(void *buffer, int stride)
{
    D3DDevice_SetStreamSource(0, buffer, (unsigned int)stride);
}

static void *rxdk_gfx_bound_indices;

static void rxdk_gfx_set_indices(void *buffer)
{
    rxdk_gfx_bound_indices = buffer;
    D3DDevice_SetIndices(buffer, 0);
}

static void rxdk_gfx_set_fvf(unsigned int fvf)
{
    D3DDevice_SetVertexShader(fvf);
}

static void rxdk_gfx_set_texture(int stage, void *texture)
{
    D3DDevice_SetTexture((unsigned int)stage, texture);
}

static void rxdk_gfx_draw(int type, int start, int count)
{
    D3DDevice_DrawVertices(type, (unsigned int)start, vertex_count(type, count));
}

static void rxdk_gfx_draw_indexed(int type, int base_vertex, int start, int count)
{
    /* Sprite batches append into one vertex buffer and pass 0-based indices.
     * DrawIndexedVertices adds m_IndexBase to those indices. Leaving the base at 0
     * makes every batch after the first read the first batch's vertices, so later
     * sprites show up with some other image's texels. */
    D3DDevice_SetIndices(rxdk_gfx_bound_indices, (unsigned int)base_vertex);
    D3DDevice_DrawIndexedVertices(type, vertex_count(type, count), D3D__IndexData + start);
}

static void rxdk_gfx_clear(unsigned int flags, unsigned int color, float depth, unsigned int stencil)
{
    D3DDevice_Clear(0, NULL, flags, color, depth, stencil);
}

/* D3DUSAGE_RENDERTARGET and D3DUSAGE_DEPTHSTENCIL from d3d8types.h. */
static void *rxdk_gfx_create_render_target(int width, int height, int format)
{
    return D3DDevice_CreateSurface2((unsigned int)width, (unsigned int)height, 1, (unsigned int)format);
}

static void *rxdk_gfx_create_depth_stencil(int width, int height, int format)
{
    return D3DDevice_CreateSurface2((unsigned int)width, (unsigned int)height, 2, (unsigned int)format);
}

static void *rxdk_gfx_get_render_target(void)
{
    return D3DDevice_GetRenderTarget2();
}

static void *rxdk_gfx_get_depth_stencil(void)
{
    return D3DDevice_GetDepthStencilSurface2();
}

static void rxdk_gfx_set_render_target(void *color, void *depth)
{
    D3DDevice_SetRenderTarget(color, depth);
}

static void *rxdk_gfx_texture_surface(void *texture, int level)
{
    if (!texture)
        return NULL;
    return D3DTexture_GetSurfaceLevel2(texture, (unsigned int)level);
}

/* The GPU runs behind the CPU by a whole push buffer. Anything that reads a surface the GPU
 * was just drawing into, such as resolving a render target, has to wait for it first. */
static void rxdk_gfx_block_until_idle(void)
{
    D3DDevice_BlockUntilIdle();
}

static void rxdk_gfx_present(void)
{
    D3DDevice_Swap(0);
}

static void rxdk_gfx_close(void)
{
    if (!device_open)
        return;
    D3DDevice_Release();
    d3d_device = NULL;
    device_open = 0;
}

static int rxdk_xg_swizzled(unsigned int format)
{
    return XGIsSwizzledFormat(format);
}

static unsigned int rxdk_xg_bytes_per_pixel(unsigned int format)
{
    return XGBytesPerPixelFromFormat(format);
}

static void rxdk_xg_swizzle(const void *source, unsigned int pitch, const void *rect, void *dest,
    unsigned int width, unsigned int height, const void *point, unsigned int bytes_per_pixel)
{
    XGSwizzleRect(source, pitch, rect, dest, width, height, point, bytes_per_pixel);
}

static void rxdk_xg_unswizzle(const void *source, unsigned int width, unsigned int height, const void *rect,
    void *dest, unsigned int pitch, const void *point, unsigned int bytes_per_pixel)
{
    XGUnswizzleRect(source, width, height, rect, dest, pitch, point, bytes_per_pixel);
}

static void rxdk_xg_swizzle_box(const void *source, unsigned int row_pitch, unsigned int slice_pitch, const void *box,
    void *dest, unsigned int width, unsigned int height, unsigned int depth, const void *point, unsigned int bytes_per_pixel)
{
    XGSwizzleBox(source, row_pitch, slice_pitch, box, dest, width, height, depth, point, bytes_per_pixel);
}

static void rxdk_xg_unswizzle_box(const void *source, unsigned int width, unsigned int height, unsigned int depth,
    const void *box, void *dest, unsigned int row_pitch, unsigned int slice_pitch, const void *point, unsigned int bytes_per_pixel)
{
    XGUnswizzleBox(source, width, height, depth, box, dest, row_pitch, slice_pitch, point, bytes_per_pixel);
}

static int rxdk_xg_compile_indexed(void *buffer, int *size, int type, int count, const void *indices)
{
    unsigned int n;
    int hr;
    if (!size)
        return -1;
    n = (unsigned int)*size;
    hr = XGCompileDrawIndexedVertices(buffer, &n, type, (unsigned int)count, indices);
    *size = (int)n;
    return hr;
}

static int rxdk_xg_compress(void *dest, unsigned int dest_format, unsigned int dest_pitch, unsigned int width,
    unsigned int height, void *source, unsigned int source_format, unsigned int source_pitch, float alpha_ref, unsigned int flags)
{
    return XGCompressRect(dest, dest_format, dest_pitch, width, height, source, source_format, source_pitch, alpha_ref, flags);
}

static void rxdk_xg_surface_header(unsigned int w, unsigned int h, unsigned int format, void *surface, unsigned int data, unsigned int pitch)
{
    XGSetSurfaceHeader(w, h, format, surface, data, pitch);
}

static void rxdk_xg_texture_header(unsigned int w, unsigned int h, unsigned int levels, unsigned int usage,
    unsigned int format, unsigned int pool, void *texture, unsigned int data, unsigned int pitch)
{
    XGSetTextureHeader(w, h, levels, usage, format, pool, texture, data, pitch);
}

static void rxdk_xg_cube_header(unsigned int edge, unsigned int levels, unsigned int usage, unsigned int format,
    unsigned int pool, void *texture, unsigned int data, unsigned int pitch)
{
    XGSetCubeTextureHeader(edge, levels, usage, format, pool, texture, data, pitch);
}

static void rxdk_xg_volume_header(unsigned int w, unsigned int h, unsigned int depth, unsigned int levels,
    unsigned int usage, unsigned int format, unsigned int pool, void *texture, unsigned int data, unsigned int pitch)
{
    XGSetVolumeTextureHeader(w, h, depth, levels, usage, format, pool, texture, data, pitch);
}

static void rxdk_xg_vertex_header(unsigned int length, unsigned int usage, unsigned int fvf, unsigned int pool, void *buffer, unsigned int data)
{
    XGSetVertexBufferHeader(length, usage, fvf, pool, buffer, data);
}

static void rxdk_xg_index_header(unsigned int length, unsigned int usage, unsigned int format, unsigned int pool, void *buffer, unsigned int data)
{
    XGSetIndexBufferHeader(length, usage, format, pool, buffer, data);
}

static void rxdk_xg_palette_header(unsigned int size, void *palette, unsigned int data)
{
    XGSetPaletteHeader(size, palette, data);
}

static void rxdk_xg_push_header(unsigned int size, int cpu_copy, void *buffer, unsigned int data)
{
    XGSetPushBufferHeader(size, cpu_copy, buffer, data);
}

static void rxdk_xg_fixup_header(unsigned int size, void *fixup, unsigned int data)
{
    XGSetFixupHeader(size, fixup, data);
}

static int rxdk_xg_save_bmp(void *texture, int level, const char *path)
{
    void *surface;
    int hr;
    if (!texture || !path)
        return -1;
    surface = D3DTexture_GetSurfaceLevel2(texture, (unsigned int)level);
    if (!surface)
        return -1;
    hr = XGWriteSurfaceToFile(surface, path);
    D3DResource_Release(surface);
    return hr;
}

static int rxdk_xg_save_xpr(void *resource, const char *path, int as_texture)
{
    if (!resource || !path)
        return -1;
    return XGWriteSurfaceOrTextureToXPR(resource, path, as_texture);
}

static int rxdk_xg_assemble_shader(const void *source, int source_len, unsigned int flags,
    void **constants, void **shader, void **errors, void **listing, unsigned int *shader_type)
{
    return XGAssembleShader(NULL, source, (unsigned int)source_len, flags,
        constants, shader, errors, listing, NULL, NULL, shader_type);
}

static int rxdk_gfx_create_vertex_shader(const void *decl, const void *function, unsigned int usage, unsigned int *handle)
{
    unsigned int created = 0;
    int hr;
    if (!handle)
        return -1;
    hr = D3DDevice_CreateVertexShader(decl, function, &created, usage);
    *handle = created;
    return hr;
}

static unsigned int rxdk_gfx_get_vertex_shader(void)
{
    unsigned int handle = 0;
    D3DDevice_GetVertexShader(&handle);
    return handle;
}

static void rxdk_gfx_delete_vertex_shader(unsigned int handle)
{
    D3DDevice_DeleteVertexShader(handle);
}

/* reg is the register the shader names, so c0 in vs.1.1 is reg 0 here. The +96 and the count in
 * DWORDs rather than registers are both what the SDK's own D3DDevice_SetVertexShaderConstant
 * wrapper does around these entry points; the hardware file is addressed from -96. */
static void rxdk_gfx_set_vs_constant(int reg, const void *data, int count)
{
    D3DDevice_SetVertexShaderConstantNotInline(reg + 96, data, (unsigned int)(count * 4));
}

/* The getter is not the setter's mirror: it applies the +96 itself and counts in registers, so the
 * register the shader names goes in untouched. */
static void rxdk_gfx_get_vs_constant(int reg, void *data, int count)
{
    D3DDevice_GetVertexShaderConstant(reg, data, (unsigned int)count);
}

static void rxdk_gfx_load_vertex_shader(unsigned int handle, unsigned int address)
{
    D3DDevice_LoadVertexShader(handle, address);
}

static void rxdk_gfx_load_vertex_shader_program(const void *function, unsigned int address)
{
    D3DDevice_LoadVertexShaderProgram(function, address);
}

static void rxdk_gfx_select_vertex_shader(unsigned int handle, unsigned int address)
{
    D3DDevice_SelectVertexShader(handle, address);
}

static unsigned int rxdk_gfx_create_pixel_shader(const void *def)
{
    unsigned int handle = 0;
    if (!def)
        return 0;
    D3DDevice_CreatePixelShader(def, &handle);
    return handle;
}

static void rxdk_gfx_set_pixel_shader(unsigned int handle)
{
    D3DDevice_SetPixelShader(handle);
}

static unsigned int rxdk_gfx_get_pixel_shader(void)
{
    unsigned int handle = 0;
    D3DDevice_GetPixelShader(&handle);
    return handle;
}

static void rxdk_gfx_delete_pixel_shader(unsigned int handle)
{
    D3DDevice_DeletePixelShader(handle);
}

static void rxdk_gfx_set_ps_constant(unsigned int reg, const void *data, int count)
{
    D3DDevice_SetPixelShaderConstant(reg, data, (unsigned int)count);
}

static void rxdk_gfx_get_ps_constant(unsigned int reg, void *data, int count)
{
    D3DDevice_GetPixelShaderConstant(reg, data, (unsigned int)count);
}

static void rxdk_gfx_set_pixel_shader_program(const void *def)
{
    if (def)
        D3DDevice_SetPixelShaderProgram(def);
}

static int rxdk_xg_compile_shader(const char *file, const void *source, int source_len, unsigned int flags,
    const char *entry, const char *target, void **constants, void **shader, void **errors,
    void **listing, void **machine, unsigned int *shader_type)
{
    return XGCompileShader(file, source, (unsigned int)source_len, flags, entry, target,
        constants, shader, errors, listing, machine, NULL, NULL, shader_type);
}

static int rxdk_xg_splice(void *shader, int *size, int *instructions, const void **shaders, int count, int optimize)
{
    unsigned int bytes = size ? (unsigned int)*size : 0;
    unsigned int count_out = 0;
    int hr = XGSpliceVertexShaders(shader, size ? &bytes : NULL, &count_out, shaders, (unsigned int)count, optimize);
    if (size)
        *size = (int)bytes;
    if (instructions)
        *instructions = (int)count_out;
    return hr;
}

static unsigned int rxdk_xg_shader_type(const void *microcode)
{
    return XGSUCode_GetVertexShaderType(microcode);
}

static unsigned int rxdk_xg_shader_length(const void *microcode)
{
    return XGSUCode_GetVertexShaderLength(microcode);
}

static int rxdk_xg_compare_shaders(const void *a, const void *b, void **error_log)
{
    return XGSUCode_CompareVertexShaders(a, b, error_log);
}

static void *rxdk_xg_buffer_pointer(void *buffer)
{
    return buffer ? XGBuffer_GetBufferPointer(buffer) : NULL;
}

static unsigned int rxdk_xg_buffer_size(void *buffer)
{
    return buffer ? XGBuffer_GetBufferSize(buffer) : 0;
}

static void rxdk_xg_buffer_release(void *buffer)
{
    if (buffer)
        XGBuffer_Release(buffer);
}

static void *d3d_load(const char *name, int flags, char **err, void *ud)
{
    (void)flags;
    (void)err;
    (void)ud;
    if (name && (strcmp(name, "d3d8") == 0 || strcmp(name, "d3d8.dll") == 0))
        return (void *)(size_t)0x44334438;
    return NULL;
}

static void *d3d_symbol(void *handle, const char *name, char **err, void *ud)
{
    (void)handle;
    (void)err;
    (void)ud;
    if (!name)
        return NULL;
    if (!strcmp(name, "rxdk_gfx_video_flags")) return (void *)&rxdk_gfx_video_flags;
    if (!strcmp(name, "rxdk_gfx_video_standard")) return (void *)&rxdk_gfx_video_standard;
    if (!strcmp(name, "rxdk_gfx_open")) return (void *)&rxdk_gfx_open;
    if (!strcmp(name, "rxdk_gfx_clear")) return (void *)&rxdk_gfx_clear;
    if (!strcmp(name, "rxdk_gfx_get_matrix")) return (void *)&rxdk_gfx_get_matrix;
    if (!strcmp(name, "rxdk_gfx_set_viewport")) return (void *)&rxdk_gfx_set_viewport;
    if (!strcmp(name, "rxdk_gfx_get_viewport")) return (void *)&rxdk_gfx_get_viewport;
    if (!strcmp(name, "rxdk_gfx_get_render_state")) return (void *)&rxdk_gfx_get_render_state;
    if (!strcmp(name, "rxdk_gfx_model")) return (void *)&rxdk_gfx_model;
    if (!strcmp(name, "rxdk_gfx_view")) return (void *)&rxdk_gfx_view;
    if (!strcmp(name, "rxdk_gfx_projection")) return (void *)&rxdk_gfx_projection;
    if (!strcmp(name, "rxdk_gfx_render_state")) return (void *)&rxdk_gfx_render_state;
    if (!strcmp(name, "rxdk_gfx_get_texture_stage_state")) return (void *)&rxdk_gfx_get_texture_stage_state;
    if (!strcmp(name, "rxdk_gfx_texture_stage_state")) return (void *)&rxdk_gfx_texture_stage_state;
    if (!strcmp(name, "rxdk_gfx_create_render_target")) return (void *)&rxdk_gfx_create_render_target;
    if (!strcmp(name, "rxdk_gfx_create_depth_stencil")) return (void *)&rxdk_gfx_create_depth_stencil;
    if (!strcmp(name, "rxdk_gfx_get_render_target")) return (void *)&rxdk_gfx_get_render_target;
    if (!strcmp(name, "rxdk_gfx_get_depth_stencil")) return (void *)&rxdk_gfx_get_depth_stencil;
    if (!strcmp(name, "rxdk_gfx_set_render_target")) return (void *)&rxdk_gfx_set_render_target;
    if (!strcmp(name, "rxdk_gfx_texture_surface")) return (void *)&rxdk_gfx_texture_surface;
    if (!strcmp(name, "rxdk_gfx_block_until_idle")) return (void *)&rxdk_gfx_block_until_idle;
    if (!strcmp(name, "rxdk_gfx_create_vertex_buffer")) return (void *)&rxdk_gfx_create_vertex_buffer;
    if (!strcmp(name, "rxdk_gfx_vertex_write")) return (void *)&rxdk_gfx_vertex_write;
    if (!strcmp(name, "rxdk_gfx_vertex_write_ex")) return (void *)&rxdk_gfx_vertex_write_ex;
    if (!strcmp(name, "rxdk_gfx_create_index_buffer")) return (void *)&rxdk_gfx_create_index_buffer;
    if (!strcmp(name, "rxdk_gfx_index_write")) return (void *)&rxdk_gfx_index_write;
    if (!strcmp(name, "rxdk_gfx_create_texture")) return (void *)&rxdk_gfx_create_texture;
    if (!strcmp(name, "rxdk_gfx_create_texture_ex")) return (void *)&rxdk_gfx_create_texture_ex;
    if (!strcmp(name, "rxdk_gfx_texture_write_level")) return (void *)&rxdk_gfx_texture_write_level;
    if (!strcmp(name, "rxdk_gfx_texture_read_level")) return (void *)&rxdk_gfx_texture_read_level;
    if (!strcmp(name, "rxdk_gfx_texture_from_memory_ex")) return (void *)&rxdk_gfx_texture_from_memory_ex;
    if (!strcmp(name, "rxdk_gfx_texture_from_file_ex")) return (void *)&rxdk_gfx_texture_from_file_ex;
    if (!strcmp(name, "rxdk_gfx_cube_from_memory")) return (void *)&rxdk_gfx_cube_from_memory;
    if (!strcmp(name, "rxdk_gfx_cube_from_file")) return (void *)&rxdk_gfx_cube_from_file;
    if (!strcmp(name, "rxdk_gfx_volume_create")) return (void *)&rxdk_gfx_volume_create;
    if (!strcmp(name, "rxdk_xg_swizzled")) return (void *)&rxdk_xg_swizzled;
    if (!strcmp(name, "rxdk_xg_bytes_per_pixel")) return (void *)&rxdk_xg_bytes_per_pixel;
    if (!strcmp(name, "rxdk_xg_swizzle")) return (void *)&rxdk_xg_swizzle;
    if (!strcmp(name, "rxdk_xg_unswizzle")) return (void *)&rxdk_xg_unswizzle;
    if (!strcmp(name, "rxdk_xg_swizzle_box")) return (void *)&rxdk_xg_swizzle_box;
    if (!strcmp(name, "rxdk_xg_unswizzle_box")) return (void *)&rxdk_xg_unswizzle_box;
    if (!strcmp(name, "rxdk_xg_compile_indexed")) return (void *)&rxdk_xg_compile_indexed;
    if (!strcmp(name, "rxdk_xg_compress")) return (void *)&rxdk_xg_compress;
    if (!strcmp(name, "rxdk_xg_surface_header")) return (void *)&rxdk_xg_surface_header;
    if (!strcmp(name, "rxdk_xg_texture_header")) return (void *)&rxdk_xg_texture_header;
    if (!strcmp(name, "rxdk_xg_cube_header")) return (void *)&rxdk_xg_cube_header;
    if (!strcmp(name, "rxdk_xg_volume_header")) return (void *)&rxdk_xg_volume_header;
    if (!strcmp(name, "rxdk_xg_vertex_header")) return (void *)&rxdk_xg_vertex_header;
    if (!strcmp(name, "rxdk_xg_index_header")) return (void *)&rxdk_xg_index_header;
    if (!strcmp(name, "rxdk_xg_palette_header")) return (void *)&rxdk_xg_palette_header;
    if (!strcmp(name, "rxdk_xg_push_header")) return (void *)&rxdk_xg_push_header;
    if (!strcmp(name, "rxdk_xg_fixup_header")) return (void *)&rxdk_xg_fixup_header;
    if (!strcmp(name, "rxdk_xg_save_bmp")) return (void *)&rxdk_xg_save_bmp;
    if (!strcmp(name, "rxdk_xg_save_xpr")) return (void *)&rxdk_xg_save_xpr;
    if (!strcmp(name, "rxdk_xg_compile_shader")) return (void *)&rxdk_xg_compile_shader;
    if (!strcmp(name, "rxdk_xg_splice")) return (void *)&rxdk_xg_splice;
    if (!strcmp(name, "rxdk_xg_shader_type")) return (void *)&rxdk_xg_shader_type;
    if (!strcmp(name, "rxdk_xg_shader_length")) return (void *)&rxdk_xg_shader_length;
    if (!strcmp(name, "rxdk_xg_compare_shaders")) return (void *)&rxdk_xg_compare_shaders;
    if (!strcmp(name, "rxdk_xg_buffer_pointer")) return (void *)&rxdk_xg_buffer_pointer;
    if (!strcmp(name, "rxdk_xg_buffer_size")) return (void *)&rxdk_xg_buffer_size;
    if (!strcmp(name, "rxdk_xg_buffer_release")) return (void *)&rxdk_xg_buffer_release;
    if (!strcmp(name, "rxdk_gfx_texture_write")) return (void *)&rxdk_gfx_texture_write;
    if (!strcmp(name, "rxdk_gfx_release")) return (void *)&rxdk_gfx_release;
    if (!strcmp(name, "rxdk_gfx_set_stream")) return (void *)&rxdk_gfx_set_stream;
    if (!strcmp(name, "rxdk_gfx_set_indices")) return (void *)&rxdk_gfx_set_indices;
    if (!strcmp(name, "rxdk_gfx_set_fvf")) return (void *)&rxdk_gfx_set_fvf;
    if (!strcmp(name, "rxdk_gfx_create_vertex_shader")) return (void *)&rxdk_gfx_create_vertex_shader;
    if (!strcmp(name, "rxdk_gfx_get_vertex_shader")) return (void *)&rxdk_gfx_get_vertex_shader;
    if (!strcmp(name, "rxdk_gfx_delete_vertex_shader")) return (void *)&rxdk_gfx_delete_vertex_shader;
    if (!strcmp(name, "rxdk_gfx_set_vs_constant")) return (void *)&rxdk_gfx_set_vs_constant;
    if (!strcmp(name, "rxdk_gfx_get_vs_constant")) return (void *)&rxdk_gfx_get_vs_constant;
    if (!strcmp(name, "rxdk_gfx_load_vertex_shader")) return (void *)&rxdk_gfx_load_vertex_shader;
    if (!strcmp(name, "rxdk_gfx_load_vertex_shader_program")) return (void *)&rxdk_gfx_load_vertex_shader_program;
    if (!strcmp(name, "rxdk_gfx_select_vertex_shader")) return (void *)&rxdk_gfx_select_vertex_shader;
    if (!strcmp(name, "rxdk_gfx_create_pixel_shader")) return (void *)&rxdk_gfx_create_pixel_shader;
    if (!strcmp(name, "rxdk_gfx_set_pixel_shader")) return (void *)&rxdk_gfx_set_pixel_shader;
    if (!strcmp(name, "rxdk_gfx_get_pixel_shader")) return (void *)&rxdk_gfx_get_pixel_shader;
    if (!strcmp(name, "rxdk_gfx_delete_pixel_shader")) return (void *)&rxdk_gfx_delete_pixel_shader;
    if (!strcmp(name, "rxdk_gfx_set_ps_constant")) return (void *)&rxdk_gfx_set_ps_constant;
    if (!strcmp(name, "rxdk_gfx_get_ps_constant")) return (void *)&rxdk_gfx_get_ps_constant;
    if (!strcmp(name, "rxdk_gfx_set_pixel_shader_program")) return (void *)&rxdk_gfx_set_pixel_shader_program;
    if (!strcmp(name, "rxdk_xg_assemble_shader")) return (void *)&rxdk_xg_assemble_shader;
    if (!strcmp(name, "rxdk_gfx_set_texture")) return (void *)&rxdk_gfx_set_texture;
    if (!strcmp(name, "rxdk_gfx_draw")) return (void *)&rxdk_gfx_draw;
    if (!strcmp(name, "rxdk_gfx_draw_indexed")) return (void *)&rxdk_gfx_draw_indexed;
    if (!strcmp(name, "rxdk_gfx_present")) return (void *)&rxdk_gfx_present;
    if (!strcmp(name, "rxdk_gfx_close")) return (void *)&rxdk_gfx_close;
    return NULL;
}

static void *d3d_close(void *handle, void *ud)
{
    (void)handle;
    (void)ud;
    return NULL;
}

void rxdk_bind_d3d8_register(void)
{
    mono_dl_fallback_register(d3d_load, d3d_symbol, d3d_close, NULL);
}
