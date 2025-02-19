# Netshlix

![Demo](demo.gif)

Displays an RTP/JPEG stream on a [Smalltv-pro](https://github.com/GeekMagicClock/smalltv-pro) (240x240 px LCD).

The roll-your-own minimal [RTP/JPEG stack](components/rtpjpeg) also works on Linux and is fully tested and fuzzed.

## Build and deploy

This is an [ESP-IDF 5](https://github.com/espressif/esp-idf) project.

```bash
source $IDF_PATH/export.sh
export ESPPORT=/dev/ttyACM0

# Configuration
cp sdkconfig.defaults.ci sdkconfig.defaults
# Now, change default config (e.g. WiFi credentials):
idf.py menuconfig
idf.py save-defconfig

# Build/flash (see below for Pinout info)
idf.py build flash

# Lint/format
./format.sh

# Flash and monitor
idf.py -p $ESPPORT build flash monitor
idf.py -p $ESPPORT monitor

# Send frames (`mtu` here means UDP payload size, not actual MTU).
gst-launch-1.0 filesrc location=components/rtpjpeg/BigBuckBunny_320x180.mp4 ! decodebin \
    ! videoconvert ! videoscale ! video/x-raw,width=240,height=240 \
    ! jpegenc \
    ! rtpjpegpay seqnum-offset=63000 mtu=1400 \
    ! udpsink host=10.0.0.134 port=1234

# With fps capped:
gst-launch-1.0 filesrc location=components/rtpjpeg/BigBuckBunny_320x180.mp4 ! decodebin \
    ! videorate ! "video/x-raw,framerate=10/1" ! videoconvert ! videoscale ! video/x-raw,width=240,height=240 \
    ! jpegenc \
    ! rtpjpegpay seqnum-offset=63000 mtu=1400 \
    ! udpsink host=10.0.0.134 port=1234
```

## C Conventions

- Names: `buf`, `sz`, `out`
- Sizes: `ptrdiff_t`
- Objects: `typedef struct X_t {} X_t`, `init_X(..., X_t *out)`, `X_do(X_t *x, ...)`, `X_destroy(X_t *x)`

## Notes

- There is no WiFi provisioning - the credentials are configured via KConfig (`SMALLTV_WIFI_SSID`, `SMALLTV_WIFI_PASSWORD`) and compiled in.
- After a timeout without frames arriving (FRAME_TIMEOUT_US), a test image will be shown on the screen.
- Next to the two jitterbuffer and the JPEG data buffer to decode from, we do not have enough RAM to keep a display framebuffer.
- Thus, there is a single pixel buffer which can hold only a fraction of the screen pixels.
- Both LVGL and the JPEG decoder use this same buffer, rendering one stripe at a time, which is then sent to the display.
- When frames are arriving, LVGL is deactivated by not calling `lv_timer_handler()`.
- We are not using the esp_jpeg component (or ROM decoder) because its API does not allow to receive decoded data block by block.

## Hardware

Picture: https://github.com/GeekMagicClock/smalltv-pro/blob/main/images/img-smalltv-pro.jpg

ESP is `ESP32-WROOM-32E 8M Byte`

    ESP32-D0WD-V3 chip
    Xtensa® dual-core 32-bit LX6 CPU
    448 KB of ROM
    520 KB of SRAM
    16 KB of RTCSRAM
    8 MB of Flash memory (SPI)

PINs on header (verified with multimeter, WROOM module pin number in parens):
    1 GND square
    2 TX (35)
    3 RX (34)
    4 3V3
    5 GPIO0 (25) - must be held low on reset
    6 RST (3)

To let programmer automatically reset the board: Connect pad 1 (GND) to pad 5 (GPIO0).
