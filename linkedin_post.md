I built a traffic signal light detector using a $15 ESP32-S3 board with an onboard camera -- no cloud, no GPU, no ML model. Just color math running on a microcontroller.

The system captures camera frames, converts pixels from RGB to HSV color space, and counts how many fall within the red and green hue ranges. If enough bright, saturated red pixels are present, it lights the onboard LED red. Same for green. Simple, fast, and runs at ~3-5 detections per second.

What makes this interesting:

- HSV color space separates hue from brightness, making detection robust across different lighting conditions. You don't need a neural network to tell red from green.
- The detection runs entirely on-device. No cloud API calls, no inference server. A 240 MHz dual-core MCU handles it comfortably.
- It serves a live web dashboard over WiFi -- open the board's IP in any browser to see the camera feed, a virtual signal light indicator, and real-time pixel count stats.
- I built it in both C++ (Arduino/PlatformIO) and MicroPython to compare approaches on the same hardware. Same algorithm, same result, different languages.

The detection algorithm:
1. Capture a 320x240 frame in RGB565 format
2. Sample every 2nd or 3rd pixel (skip the rest for speed)
3. Convert RGB565 to HSV
4. Red: hue 0-12 or 168-179, saturation > 80, value > 80
5. Green: hue 35-85, same saturation and value thresholds
6. Whichever color exceeds 0.5% of sampled pixels wins

Hardware: Freenove ESP32-S3 WROOM with OV2640 camera and WS2812 RGB LED.

Not everything needs deep learning. Sometimes the right color space and a threshold are all you need.

Source code: https://github.com/parupati/TraficLightDetectorIOTEsp32

#IoT #ESP32 #EmbeddedSystems #ComputerVision #MicroPython #Arduino #EdgeComputing #TrafficLight #ColorDetection #PlatformIO #Microcontroller #RealTime #SoftwareEngineering﻿