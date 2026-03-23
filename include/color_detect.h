#ifndef COLOR_DETECT_H
#define COLOR_DETECT_H

#include <stdint.h>

typedef struct {
    uint8_t h; // 0-179
    uint8_t s; // 0-255
    uint8_t v; // 0-255
} hsv_t;

typedef struct {
    int red_count;
    int green_count;
    int total_pixels;
    int detected; // 0=none, 1=red, 2=green
} detect_result_t;

// Convert RGB565 pixel to HSV
static inline hsv_t rgb565_to_hsv(uint16_t pixel) {
    // RGB565: RRRRRGGGGGGBBBBB
    uint8_t r = ((pixel >> 11) & 0x1F) << 3;
    uint8_t g = ((pixel >> 5)  & 0x3F) << 2;
    uint8_t b = (pixel & 0x1F) << 3;

    hsv_t hsv;
    uint8_t max_c = r > g ? (r > b ? r : b) : (g > b ? g : b);
    uint8_t min_c = r < g ? (r < b ? r : b) : (g < b ? g : b);
    uint8_t delta = max_c - min_c;

    hsv.v = max_c;

    if (max_c == 0) {
        hsv.s = 0;
        hsv.h = 0;
        return hsv;
    }

    hsv.s = (uint8_t)(((uint16_t)delta * 255) / max_c);

    if (delta == 0) {
        hsv.h = 0;
        return hsv;
    }

    int hue;
    if (max_c == r) {
        hue = 30 * (int)(g - b) / (int)delta;
    } else if (max_c == g) {
        hue = 60 + 30 * (int)(b - r) / (int)delta;
    } else {
        hue = 120 + 30 * (int)(r - g) / (int)delta;
    }

    if (hue < 0) hue += 180;
    hsv.h = (uint8_t)hue;
    return hsv;
}

// Detect red and green signal lights in an RGB565 frame
// Samples every `step` pixels for speed
static detect_result_t detect_signal_color(uint16_t *pixels, int width, int height, int step) {
    detect_result_t result = {0, 0, 0, 0};

    // Only scan the frame (sample for speed)
    for (int y = 0; y < height; y += step) {
        for (int x = 0; x < width; x += step) {
            // RGB565 is big-endian from camera, swap bytes
            uint16_t raw = pixels[y * width + x];
            uint16_t pixel = (raw >> 8) | (raw << 8);

            hsv_t hsv = rgb565_to_hsv(pixel);
            result.total_pixels++;

            // Only consider bright, saturated pixels (signal lights are bright)
            if (hsv.s < 80 || hsv.v < 80) continue;

            // Red: hue 0-12 or 168-179 (wraps around)
            if (hsv.h <= 12 || hsv.h >= 168) {
                result.red_count++;
            }
            // Green: hue 35-85
            else if (hsv.h >= 35 && hsv.h <= 85) {
                result.green_count++;
            }
        }
    }

    // Require a minimum percentage of colored pixels to trigger
    int threshold = result.total_pixels / 200; // 0.5% of sampled pixels
    if (threshold < 5) threshold = 5;

    if (result.red_count > threshold && result.red_count > result.green_count) {
        result.detected = 1; // RED
    } else if (result.green_count > threshold && result.green_count > result.red_count) {
        result.detected = 2; // GREEN
    }

    return result;
}

#endif
