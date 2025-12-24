#include <string>
#include "powermgt.h"
#include <M5Unified.h>
#include "readpaper.h"
#include "../text/bin_font_print.h"
#include "device/ui_display.h"
#include "text/text_handle.h"
#include "text/book_handle.h"
#include "ui/ui_canvas_image.h"
#include "test/per_file_debug.h"
#include "ui/ui_lock_screen.h"
#include <SPIFFS.h>
#include <SD.h>

#include "current_book.h"

void show_shutdown_and_sleep(bool inIssue)
{
    // CRITICAL: Save config and bookmark before shutdown to prevent loss on power off
    // This is essential when user presses power button directly without any prior save triggers.
    extern bool config_save();
#if DBG_POWERMGT
    bool config_ok = config_save();
    Serial.printf("[POWER] 配置保存结果: %s, 等待SD卡写入完成...\n", config_ok ? "成功" : "失败");
#else
    (void)config_save();
#endif
    
    auto current_sp = std::atomic_load(&__g_current_book_shared);
    if (current_sp)
    {
        current_sp->saveBookmark();
    }
    
    // CRITICAL FIX: Ensure SD card controller's internal buffer is flushed to physical media.
    // Standard delay(100) is NOT enough for SD card writes due to:
    // 1. SD card internal cache (typically 512 bytes or more)
    // 2. SPI transaction latency
    // 3. Wear leveling and block erase delays on the SD card itself
    // We need at least 300-500ms to ensure all pending writes are committed.
    // Additionally, we should attempt to flush the SD filesystem.
    
#if DBG_POWERMGT
    Serial.printf("[POWER] 配置保存结果: %s, 等待SD卡写入完成...\n", config_ok ? "成功" : "失败");
#endif
    
    // Try to flush SD filesystem if available
    // Note: SDW::SD may not expose a direct flush API, but we can attempt SD.end() + SD.begin()
    // or rely on sufficient delay. For safety, use a longer delay.
    delay(500);  // Increased from 100ms to 500ms to ensure SD card internal buffers flush
    
    // Optional: attempt to remount SD to force flush (risky, may fail)
    // SDW::SD.end();
    // delay(100);
    // SDW::SD.begin();

    if (!inIssue)
        show_lockscreen(PAPER_S3_WIDTH, PAPER_S3_HEIGHT, 30, "电源键开机", true);
    else
        show_lockscreen(PAPER_S3_WIDTH, PAPER_S3_HEIGHT, 30, nullptr, true);
    // 关机太快会无法切屏幕 , delay in show_lockscreen
    delay(2000);
    M5.Display.waitDisplay();
    delay(2000);
    M5.Power.powerOff();
}
void show_shutdown_low(const char* iconname, const char* info, int8_t imgwidth, int8_t imgheight)
{
    M5.Display.powerSaveOff();
    M5.Display.clear();

    // 显示指定的图标
    String iconPath = "/spiffs/";
    iconPath += iconname;
    ui_push_image_to_display_direct(iconPath.c_str(), (PAPER_S3_WIDTH-imgwidth)/2, (PAPER_S3_HEIGHT-imgheight)/2 );


    // 在屏幕中心显示信息文本
    M5.Display.setTextColor(TFT_BLACK);
    M5.Display.setTextSize(2);
    M5.Display.setTextDatum(MC_DATUM); // 居中对齐
    M5.Display.drawString(info, PAPER_S3_WIDTH / 2, PAPER_S3_HEIGHT / 2 + imgheight/2 + 20);

    M5.Display.waitDisplay();
    M5.Display.powerSaveOn();
    M5.Power.powerOff();
}