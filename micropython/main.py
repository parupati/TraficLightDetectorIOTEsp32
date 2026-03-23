import time
import network
import neopixel
import machine
from camera import Camera, PixelFormat, FrameSize, GrabMode
from color_detect import detect_signal
from web_server import WebServer

SSID = "YOUR_WIFI_SSID"
PASSWORD = "YOUR_WIFI_PASSWORD"

led = neopixel.NeoPixel(machine.Pin(48), 1)


def set_led(r, g, b):
    led[0] = (r, g, b)
    led.write()


def connect_wifi():
    wlan = network.WLAN(network.STA_IF)
    wlan.active(True)
    if wlan.isconnected():
        print("Already connected:", wlan.ifconfig()[0])
        return wlan.ifconfig()[0]
    print("Connecting to", SSID, end="")
    wlan.connect(SSID, PASSWORD)
    for _ in range(40):
        if wlan.isconnected():
            break
        print(".", end="")
        time.sleep(0.5)
    if not wlan.isconnected():
        print("\nWiFi FAILED")
        set_led(255, 80, 0)
        return None
    ip = wlan.ifconfig()[0]
    print("\nConnected! IP:", ip)
    return ip


def main():
    print("\n=== Signal Light Detector (MicroPython) ===\n")
    set_led(0, 0, 50)

    cam = Camera(
        pixel_format=PixelFormat.RGB565,
        frame_size=FrameSize.QVGA,
        fb_count=1,
        grab_mode=GrabMode.WHEN_EMPTY,
    )
    print("Camera initialized")

    ip = connect_wifi()
    if not ip:
        return

    detect_state = {"detected": 0, "red": 0, "green": 0, "total": 0}
    server = WebServer(cam, detect_state)
    server.start()
    set_led(0, 50, 0)

    print("==========================================")
    print("  Open: http://{}".format(ip))
    print("  Signal light detection active")
    print("==========================================")

    while True:
        server.handle_once()

        cam.reconfigure(pixel_format=PixelFormat.RGB565, frame_size=FrameSize.QVGA, fb_count=1)
        frame = cam.capture()
        if frame:
            detected, red, green, total = detect_signal(bytes(frame), 320, 240, step=3)
            detect_state["detected"] = detected
            detect_state["red"] = red
            detect_state["green"] = green
            detect_state["total"] = total

            if detected == 1:
                set_led(255, 0, 0)
                print("RED | r={} g={} t={}".format(red, green, total))
            elif detected == 2:
                set_led(0, 255, 0)
                print("GREEN | r={} g={} t={}".format(red, green, total))
            else:
                set_led(0, 0, 30)

        cam.reconfigure(pixel_format=PixelFormat.JPEG, frame_size=FrameSize.SVGA, fb_count=2)
        time.sleep_ms(200)


main()