#pragma once

#include <stdint.h>
#include <stdbool.h>

// Initialize USB MSC subsystem. Returns true on success.
bool usb_msc_init();

// Start presenting the SD card as USB MSC. Returns true if started.
bool usb_msc_start();

// Stop MSC and restore normal operation.
void usb_msc_stop();

// Poll helper (if needed by main loop)
void usb_msc_poll();

// Returns true if MSC is active
bool usb_msc_is_active();

// When unmounted by host this callback will request a device reboot
// The module will call ESP.restart() when host unmounts (implemented internally).
