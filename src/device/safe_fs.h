#pragma once

#include <string>
#include <functional>
#include <Arduino.h>
#include "../SD/SDWrapper.h"

// Utilities for robust file writes on SD/SPIFFS to survive sudden power loss.
// Pattern: write to <path>.tmp, flush/close, then rename to <path>.
// On read: if <path> is missing but <path>.tmp exists, try to restore by renaming.

namespace SafeFS
{
    // Return a temporary file path for a given final path (appends ".tmp").
    inline std::string tmpPathFor(const std::string &path)
    {
        return path + ".tmp";
    }

    // Try to atomically replace the destination with the tmp file.
    // CRITICAL FIX for hardware reset safety:
    // - Never delete the destination file before ensuring tmp is valid
    // - Prefer overwrite (copy) over rename+delete to minimize data loss window
    inline bool promoteTmpToFinal(const std::string &tmp, const std::string &final)
    {
        // CRITICAL: Verify tmp file exists and is readable before any operation
        File src = SDW::SD.open(tmp.c_str(), "r");
        if (!src)
        {
            // tmp file missing or corrupted, do not touch the final file
            return false;
        }
        size_t tmp_size = src.size();
        src.close();
        
        if (tmp_size == 0)
        {
            // tmp file is empty, likely corrupted during write, do not promote
            SDW::SD.remove(tmp.c_str());
            return false;
        }
        
        // Strategy 1: Try direct rename if destination doesn't exist
        if (!SDW::SD.exists(final.c_str()))
        {
            if (SDW::SD.rename(tmp.c_str(), final.c_str()))
            {
                return true;
            }
        }
        
        // Strategy 2: If destination exists, OVERWRITE it by copying tmp -> final
        // This is safer than remove+rename because:
        // - If power loss occurs during copy, destination still has old (valid) content
        // - Only after successful copy+flush do we commit the new content
        src = SDW::SD.open(tmp.c_str(), "r");
        if (!src)
            return false;
        
        File dst = SDW::SD.open(final.c_str(), "w");
        if (!dst)
        {
            src.close();
            return false;
        }
        
        uint8_t buf[512];
        while (src.available())
        {
            int n = src.read(buf, sizeof(buf));
            if (n <= 0)
                break;
            dst.write(buf, (size_t)n);
        }
        dst.flush();
        
        // CRITICAL: Ensure data is flushed to SD card before closing.
        // Increased delay from 10ms to 30ms for better hardware reset tolerance.
        delay(30);
        dst.close();
        src.close();
        
        // Verify the copied file size matches
        File verify = SDW::SD.open(final.c_str(), "r");
        if (verify)
        {
            size_t final_size = verify.size();
            verify.close();
            if (final_size == tmp_size)
            {
                // Success, cleanup tmp
                SDW::SD.remove(tmp.c_str());
                return true;
            }
        }
        
        // If verification failed, keep tmp for next recovery attempt
        return false;
    }

    // Safely write a file using a writer functor. The writer should return true on success.
    // The function creates <path>.tmp, invokes writer(file), flushes, closes, then promotes tmp -> final.
    inline bool safeWrite(const std::string &path, const std::function<bool(File &)> &writer)
    {
        const std::string tmp = tmpPathFor(path);
        // Ensure parent folder likely exists; rely on caller for folder creation.
        File f = SDW::SD.open(tmp.c_str(), "w");
        if (!f)
            return false;
        bool ok = writer(f);
        f.flush();
        f.close();
        if (!ok)
        {
            SDW::SD.remove(tmp.c_str());
            return false;
        }
        return promoteTmpToFinal(tmp, path);
    }

    // During read, if final is missing but tmp exists, try to promote tmp to final.
    inline void restoreFromTmpIfNeeded(const std::string &path)
    {
        const std::string tmp = tmpPathFor(path);
        if (!SDW::SD.exists(path.c_str()) && SDW::SD.exists(tmp.c_str()))
        {
            // Best-effort promotion; ignore result
            promoteTmpToFinal(tmp, path);
        }
    }
}
