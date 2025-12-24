#pragma once
#include <string>
#include <stdint.h>

// compute width in device units between start_pos (inclusive) and end_pos (exclusive)
int16_t calculate_text_width(const std::string &text, size_t start_pos, size_t end_pos);

// Find break position for given text starting at start_pos. Returns index into text (byte offset).
size_t find_break_position(const std::string &text, size_t start_pos, int16_t max_width, bool vertical, float scale_factor);

// Convenience wrapper that accepts font_size and computes internal scale factor
size_t find_break_position_scaled(const std::string &text, size_t start_pos, int16_t max_width, bool vertical, float font_size);
