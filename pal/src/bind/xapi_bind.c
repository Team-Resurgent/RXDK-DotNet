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

static int devices_ready;
static int keyboard_ready;

static void rxdk_input_init(void)
{
    XDEVICE_PREALLOC_TYPE types[2];
    if (devices_ready)
        return;
    types[0].DeviceType = XDEVICE_TYPE_GAMEPAD;
    types[0].dwPreallocCount = 4;
    types[1].DeviceType = XDEVICE_TYPE_DEBUG_KEYBOARD;
    types[1].dwPreallocCount = 1;
    XInitDevices(2, types);
    devices_ready = 1;
}

static unsigned int rxdk_input_connected_mask(void)
{
    rxdk_input_init();
    return (unsigned int)XGetDevices(XDEVICE_TYPE_GAMEPAD);
}

static void *rxdk_input_open_gamepad(unsigned int port)
{
    rxdk_input_init();
    return XInputOpen(XDEVICE_TYPE_GAMEPAD, port, XDEVICE_NO_SLOT, NULL);
}

static void rxdk_input_close(void *handle)
{
    if (handle)
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
    XINPUT_FEEDBACK fb;
    if (!handle)
        return 1167;
    memset(&fb, 0, sizeof(fb));
    fb.Rumble.wLeftMotorSpeed = left;
    fb.Rumble.wRightMotorSpeed = right;
    return (unsigned int)XInputSetState(handle, &fb);
}

static unsigned int rxdk_input_init_keyboard(void)
{
    XINPUT_DEBUG_KEYQUEUE_PARAMETERS p;
    unsigned int rc;
    rxdk_input_init();
    if (keyboard_ready)
        return 0;
    memset(&p, 0, sizeof(p));
    p.dwFlags = XINPUT_DEBUG_KEYQUEUE_FLAG_KEYDOWN | XINPUT_DEBUG_KEYQUEUE_FLAG_KEYREPEAT
        | XINPUT_DEBUG_KEYQUEUE_FLAG_KEYUP | XINPUT_DEBUG_KEYQUEUE_FLAG_ONE_QUEUE;
    p.dwQueueSize = 32;
    p.dwRepeatDelay = 400;
    p.dwRepeatInterval = 150;
    rc = (unsigned int)XInputDebugInitKeyboardQueue(&p);
    if (rc == 0)
        keyboard_ready = 1;
    return rc;
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
