def rgb565_to_hsv(pixel):
    """Convert RGB565 pixel (big-endian from camera) to HSV."""
    # Swap bytes (camera sends big-endian)
    pixel = ((pixel >> 8) & 0xFF) | ((pixel & 0xFF) << 8)
    r = ((pixel >> 11) & 0x1F) << 3
    g = ((pixel >> 5) & 0x3F) << 2
    b = (pixel & 0x1F) << 3

    max_c = max(r, g, b)
    min_c = min(r, g, b)
    delta = max_c - min_c

    v = max_c
    s = 0 if max_c == 0 else (delta * 255) // max_c

    if delta == 0:
        h = 0
    elif max_c == r:
        h = 30 * (g - b) // delta
    elif max_c == g:
        h = 60 + 30 * (b - r) // delta
    else:
        h = 120 + 30 * (r - g) // delta

    if h < 0:
        h += 180

    return h, s, v


def detect_signal(buf, width, height, step=3):
    """Scan RGB565 frame buffer for red and green signal lights.

    Returns: (detected, red_count, green_count, total)
      detected: 0=none, 1=red, 2=green
    """
    red = 0
    green = 0
    total = 0

    for y in range(0, height, step):
        row_offset = y * width * 2
        for x in range(0, width, step):
            offset = row_offset + x * 2
            pixel = (buf[offset] << 8) | buf[offset + 1]
            h, s, v = rgb565_to_hsv(pixel)
            total += 1

            if s < 80 or v < 80:
                continue

            if h <= 12 or h >= 168:
                red += 1
            elif 35 <= h <= 85:
                green += 1

    threshold = max(total // 200, 5)

    if red > threshold and red > green:
        return 1, red, green, total
    elif green > threshold and green > red:
        return 2, red, green, total
    return 0, red, green, total
