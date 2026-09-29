/* Rxdk.Input native bind. This is the only translation unit that takes the address of
 * XInput exports. build-host pulls it with -Wl,-u,rxdk_bind_xapi_register when
 * Rxdk.Input.dll is part of the title. libxapi is already on the base link, so the
 * sidecar lists no extra SDK library. Do not register these symbols from rxdk_dl_symbol.
 */
#include <xboxkrnl/xboxkrnl.h>
#ifndef NT_INCLUDED
#define NT_INCLUDED
#endif
#include <stdarg.h>
#include <windef.h>
#include <winbase.h>
#include <xbox.h>
#include <xkbd.h>
#include <string.h>

typedef void *(*RxdkDlLoad)(const char *name, int flags, char **err, void *ud);
typedef void *(*RxdkDlSymbol)(void *handle, const char *name, char **err, void *ud);
typedef void *(*RxdkDlClose)(void *handle, void *ud);
extern void *mono_dl_fallback_register(RxdkDlLoad, RxdkDlSymbol, RxdkDlClose, void *);

typedef struct {
    unsigned int packet;
    unsigned short buttons;
    unsigned char a, b, x, y, black, white, lt, rt;
    short lx, ly, rx, ry;
} __attribute__((packed)) RxdkPadState;

typedef char rxdk_pad_state_size[(sizeof(RxdkPadState) == 22) ? 1 : -1];

typedef struct {
    unsigned char virtual_key;
    char ascii;
    unsigned char flags;
} __attribute__((packed)) RxdkKeystroke;

typedef char rxdk_keystroke_size[(sizeof(RxdkKeystroke) == 3) ? 1 : -1];

typedef struct {
    unsigned int packet;
    unsigned char buttons;
    signed char x, y, wheel;
} __attribute__((packed)) RxdkMouseState;

typedef char rxdk_mouse_state_size[(sizeof(RxdkMouseState) == 8) ? 1 : -1];

typedef struct {
    unsigned int packet;
    unsigned short key;
    unsigned short dt;
} __attribute__((packed)) RxdkIrState;

typedef char rxdk_ir_state_size[(sizeof(RxdkIrState) == 8) ? 1 : -1];

static int devices_ready;
static int keyboard_ready;

/* Same polling the input samples pass to XInputOpen: auto-poll and interrupt-out.
 * NULL would use the type default, which sends rumble as a control SET_REPORT. */
static XINPUT_POLLING_PARAMETERS pad_poll = { 1, 1, 0, 8, 8, 0 };

/* XInputSetState is asynchronous and keeps this buffer until the transfer
 * completes. The samples store it on the gamepad for the life of the open. */
static void *pad_open[4];
static XINPUT_FEEDBACK pad_feedback[4];

static int pad_slot(void *handle)
{
    int i;
    for (i = 0; i < 4; i++)
        if (pad_open[i] == handle)
            return i;
    return -1;
}

static void rxdk_input_init(void)
{
    XDEVICE_PREALLOC_TYPE types[4];
    if (devices_ready)
        return;
    types[0].DeviceType = XDEVICE_TYPE_GAMEPAD;
    types[0].dwPreallocCount = 4;
    types[1].DeviceType = XDEVICE_TYPE_DEBUG_KEYBOARD;
    types[1].dwPreallocCount = 1;
    types[2].DeviceType = XDEVICE_TYPE_DEBUG_MOUSE;
    types[2].dwPreallocCount = 4;
    types[3].DeviceType = XDEVICE_TYPE_IR_REMOTE;
    types[3].dwPreallocCount = 1;
    XInitDevices(4, types);
    /* Enumeration is asynchronous. A pad already attached (xemu port 1 is Xbox
     * port 0) is invisible to XGetDevices until this goes idle. */
    {
        DWORD start = GetTickCount();
        DWORD insertions = 0, removals = 0;
        for (;;) {
            DWORD now = GetTickCount();
            if (XGetDeviceEnumerationStatus() == XDEVICE_ENUMERATION_IDLE) {
                XGetDeviceChanges(XDEVICE_TYPE_GAMEPAD, &insertions, &removals);
                if (XGetDevices(XDEVICE_TYPE_GAMEPAD) != 0)
                    break;
            }
            if ((DWORD)(now - start) >= 2000)
                break;
            Sleep(10);
        }
    }
    devices_ready = 1;
}

static unsigned int rxdk_input_connected_mask(void)
{
    rxdk_input_init();
    return (unsigned int)XGetDevices(XDEVICE_TYPE_GAMEPAD);
}

static void *rxdk_input_open_gamepad(unsigned int port)
{
    void *handle;
    rxdk_input_init();
    if (port > 3)
        return NULL;
    memset(&pad_feedback[port], 0, sizeof(pad_feedback[port]));
    handle = XInputOpen(XDEVICE_TYPE_GAMEPAD, port, XDEVICE_NO_SLOT, &pad_poll);
    pad_open[port] = handle;
    return handle;
}

static void rxdk_input_close(void *handle)
{
    int slot;
    if (!handle)
        return;
    slot = pad_slot(handle);
    if (slot >= 0)
        pad_open[slot] = NULL;
    XInputClose(handle);
}

static unsigned int rxdk_input_get_state(void *handle, RxdkPadState *state)
{
    XINPUT_STATE raw;
    unsigned int rc;
    if (!handle || !state)
        return 1167; /* ERROR_DEVICE_NOT_CONNECTED */
    memset(&raw, 0, sizeof(raw));
    rc = (unsigned int)XInputGetState(handle, &raw);
    if (rc != 0)
        return rc;
    state->packet = (unsigned int)raw.dwPacketNumber;
    state->buttons = raw.Gamepad.wButtons;
    state->a = raw.Gamepad.bAnalogButtons[XINPUT_GAMEPAD_A];
    state->b = raw.Gamepad.bAnalogButtons[XINPUT_GAMEPAD_B];
    state->x = raw.Gamepad.bAnalogButtons[XINPUT_GAMEPAD_X];
    state->y = raw.Gamepad.bAnalogButtons[XINPUT_GAMEPAD_Y];
    state->black = raw.Gamepad.bAnalogButtons[XINPUT_GAMEPAD_BLACK];
    state->white = raw.Gamepad.bAnalogButtons[XINPUT_GAMEPAD_WHITE];
    state->lt = raw.Gamepad.bAnalogButtons[XINPUT_GAMEPAD_LEFT_TRIGGER];
    state->rt = raw.Gamepad.bAnalogButtons[XINPUT_GAMEPAD_RIGHT_TRIGGER];
    state->lx = raw.Gamepad.sThumbLX;
    state->ly = raw.Gamepad.sThumbLY;
    state->rx = raw.Gamepad.sThumbRX;
    state->ry = raw.Gamepad.sThumbRY;
    return 0;
}

static unsigned int rxdk_input_set_vibration(void *handle, unsigned short left, unsigned short right)
{
    int slot;
    XINPUT_FEEDBACK *fb;
    if (!handle)
        return 1167;
    slot = pad_slot(handle);
    if (slot < 0)
        return 1167;
    fb = &pad_feedback[slot];
    /* The samples skip a new send while the previous one is still in flight. */
    if (fb->Header.dwStatus == ERROR_IO_PENDING)
        return ERROR_IO_PENDING;
    fb->Rumble.wLeftMotorSpeed = left;
    fb->Rumble.wRightMotorSpeed = right;
    return (unsigned int)XInputSetState(handle, fb);
}

static unsigned int rxdk_input_init_keyboard(void)
{
    XINPUT_DEBUG_KEYQUEUE_PARAMETERS p;
    unsigned int rc;
    rxdk_input_init();
    if (keyboard_ready)
        return 0;
    memset(&p, 0, sizeof(p));
    p.dwFlags = XINPUT_DEBUG_KEYQUEUE_FLAG_KEYDOWN | XINPUT_DEBUG_KEYQUEUE_FLAG_KEYREPEAT;
    p.dwQueueSize = 25;
    p.dwRepeatDelay = 500;
    p.dwRepeatInterval = 50;
    rc = (unsigned int)XInputDebugInitKeyboardQueue(&p);
    if (rc == 0)
        keyboard_ready = 1;
    return rc;
}

static unsigned int rxdk_input_mouse_mask(void)
{
    rxdk_input_init();
    return (unsigned int)XGetDevices(XDEVICE_TYPE_DEBUG_MOUSE);
}

static void *rxdk_input_open_mouse(unsigned int port)
{
    rxdk_input_init();
    if (port > 3)
        return NULL;
    /* NULL polling uses the mouse type default: auto-poll, no output reports. */
    return XInputOpen(XDEVICE_TYPE_DEBUG_MOUSE, port, XDEVICE_NO_SLOT, NULL);
}

static unsigned int rxdk_input_get_mouse(void *handle, RxdkMouseState *state)
{
    XINPUT_STATE raw;
    unsigned int rc;
    if (!handle || !state)
        return 1167;
    memset(&raw, 0, sizeof(raw));
    rc = (unsigned int)XInputGetState(handle, &raw);
    if (rc != 0)
        return rc;
    state->packet = (unsigned int)raw.dwPacketNumber;
    state->buttons = raw.DebugMouse.bButtons;
    state->x = raw.DebugMouse.cMickeysX;
    state->y = raw.DebugMouse.cMickeysY;
    state->wheel = raw.DebugMouse.cWheel;
    return 0;
}

static unsigned int rxdk_input_ir_mask(void)
{
    rxdk_input_init();
    return (unsigned int)XGetDevices(XDEVICE_TYPE_IR_REMOTE);
}

static void *rxdk_input_open_ir(unsigned int port)
{
    rxdk_input_init();
    if (port > 3)
        return NULL;
    return XInputOpen(XDEVICE_TYPE_IR_REMOTE, port, XDEVICE_NO_SLOT, NULL);
}

static unsigned int rxdk_input_get_ir(void *handle, RxdkIrState *state)
{
    XINPUT_STATE raw;
    unsigned char *bytes;
    unsigned int rc;
    if (!handle || !state)
        return 1167;
    memset(&raw, 0, sizeof(raw));
    rc = (unsigned int)XInputGetState(handle, &raw);
    if (rc != 0)
        return rc;
    /* The public state union has no IR member. The report is the first four
     * bytes: key code, then time since the previous code. */
    bytes = (unsigned char *)&raw.Gamepad;
    state->packet = (unsigned int)raw.dwPacketNumber;
    state->key = (unsigned short)(bytes[0] | (bytes[1] << 8));
    state->dt = (unsigned short)(bytes[2] | (bytes[3] << 8));
    return 0;
}

static unsigned int rxdk_input_subtype(void *handle)
{
    XINPUT_CAPABILITIES caps;
    if (!handle)
        return 0;
    memset(&caps, 0, sizeof(caps));
    if (XInputGetCapabilities(handle, &caps) != 0)
        return 0;
    return caps.SubType;
}

static unsigned int rxdk_input_set_lightgun_calibration(
    void *handle, unsigned short center_x, unsigned short center_y,
    unsigned short upper_left_x, unsigned short upper_left_y)
{
    XINPUT_LIGHTGUN_CALIBRATION_OFFSETS offsets;
    if (!handle)
        return 1167;
    offsets.wCenterX = center_x;
    offsets.wCenterY = center_y;
    offsets.wUpperLeftX = upper_left_x;
    offsets.wUpperLeftY = upper_left_y;
    return (unsigned int)XInputSetLightgunCalibration(handle, &offsets);
}

static unsigned int rxdk_input_get_keystroke(RxdkKeystroke *key)
{
    XINPUT_DEBUG_KEYSTROKE raw;
    unsigned int rc;
    if (!key)
        return 6; /* ERROR_INVALID_HANDLE */
    memset(&raw, 0, sizeof(raw));
    rc = (unsigned int)XInputDebugGetKeystroke(&raw);
    if (rc != 0)
        return rc;
    key->virtual_key = raw.VirtualKey;
    key->ascii = raw.Ascii;
    key->flags = raw.Flags;
    return 0;
}

static void *xapi_load(const char *name, int flags, char **err, void *ud)
{
    (void)flags;
    (void)err;
    (void)ud;
    if (name && (strcmp(name, "xapi") == 0 || strcmp(name, "xapi.dll") == 0))
        return (void *)(size_t)0x58415049;
    return NULL;
}

static void *xapi_symbol(void *handle, const char *name, char **err, void *ud)
{
    (void)handle;
    (void)err;
    (void)ud;
    if (!name)
        return NULL;
    if (!strcmp(name, "rxdk_input_init")) return (void *)&rxdk_input_init;
    if (!strcmp(name, "rxdk_input_connected_mask")) return (void *)&rxdk_input_connected_mask;
    if (!strcmp(name, "rxdk_input_open_gamepad")) return (void *)&rxdk_input_open_gamepad;
    if (!strcmp(name, "rxdk_input_close")) return (void *)&rxdk_input_close;
    if (!strcmp(name, "rxdk_input_get_state")) return (void *)&rxdk_input_get_state;
    if (!strcmp(name, "rxdk_input_set_vibration")) return (void *)&rxdk_input_set_vibration;
    if (!strcmp(name, "rxdk_input_init_keyboard")) return (void *)&rxdk_input_init_keyboard;
    if (!strcmp(name, "rxdk_input_get_keystroke")) return (void *)&rxdk_input_get_keystroke;
    if (!strcmp(name, "rxdk_input_mouse_mask")) return (void *)&rxdk_input_mouse_mask;
    if (!strcmp(name, "rxdk_input_open_mouse")) return (void *)&rxdk_input_open_mouse;
    if (!strcmp(name, "rxdk_input_get_mouse")) return (void *)&rxdk_input_get_mouse;
    if (!strcmp(name, "rxdk_input_ir_mask")) return (void *)&rxdk_input_ir_mask;
    if (!strcmp(name, "rxdk_input_open_ir")) return (void *)&rxdk_input_open_ir;
    if (!strcmp(name, "rxdk_input_get_ir")) return (void *)&rxdk_input_get_ir;
    if (!strcmp(name, "rxdk_input_subtype")) return (void *)&rxdk_input_subtype;
    if (!strcmp(name, "rxdk_input_set_lightgun_calibration")) return (void *)&rxdk_input_set_lightgun_calibration;
    return NULL;
}

static void *xapi_close(void *handle, void *ud)
{
    (void)handle;
    (void)ud;
    return NULL;
}

void rxdk_bind_xapi_register(void)
{
    mono_dl_fallback_register(xapi_load, xapi_symbol, xapi_close, NULL);
}

/* XInput pulls the USB C++ objects, which pull libunwind. That object takes the
 * address of these section bounds. An empty span means there is nothing to walk. */
__asm__(
    ".section .eh_frame,\"dr\"\n"
    ".globl ___eh_frame_start\n"
    "___eh_frame_start:\n"
    ".globl ___eh_frame_end\n"
    "___eh_frame_end:\n"
    ".text\n"
);
