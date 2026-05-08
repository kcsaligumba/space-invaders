// input.c

#include <string.h>

#include "input.h"

#include "lw_usb/GenericMacros.h"
#include "lw_usb/GenericTypeDefs.h"
#include "lw_usb/MAX3421E.h"
#include "lw_usb/USB.h"
#include "lw_usb/HID.h"

// Live state -- updated every input_poll() call when a new HID report arrives.
static uint8_t key_live[256];

// Frame snapshots -- promoted by input_snapshot() once per frame.
static uint8_t key_now[256];
static uint8_t key_was[256];

static BOOT_KBD_REPORT kbdbuf;

void input_init(void)
{
    memset(key_live, 0, sizeof(key_live));
    memset(key_now,  0, sizeof(key_now));
    memset(key_was,  0, sizeof(key_was));
    memset(&kbdbuf,  0, sizeof(kbdbuf));
}

void input_poll(void)
{
    MAX3421E_Task();
    USB_Task();

    if (GetUsbTaskState() != USB_STATE_RUNNING) {
        return;
    }

    BYTE rcode = kbdPoll(&kbdbuf);
    if (rcode != 0) {
        // hrNAK (no new data) or any other error: leave key_live alone.
        return;
    }

    // Rebuild key_live from this report's keycode array (boot protocol allows
    // up to 6 simultaneous keys).
    uint8_t fresh[256];
    memset(fresh, 0, sizeof(fresh));
    for (int i = 0; i < 6; i++) {
        uint8_t k = kbdbuf.keycode[i];
        if (k) fresh[k] = 1;
    }
    memcpy(key_live, fresh, sizeof(key_live));
}

void input_snapshot(void)
{
    // Move this-frame -> previous-frame, then capture current live state.
    memcpy(key_was, key_now,  sizeof(key_was));
    memcpy(key_now, key_live, sizeof(key_now));
}

int input_held(uint8_t hid_code)
{
    return key_now[hid_code];
}

int input_pressed(uint8_t hid_code)
{
    return key_now[hid_code] && !key_was[hid_code];
}
