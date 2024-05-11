# Netshlix

**Work in Progress**

Goal: display an RTP/JPEG stream on a [Smalltv-pro](https://github.com/GeekMagicClock/smalltv-pro) (240x240 px LCD).

The roll-your-own minimal [RTP/JPEG stack](components/rtpjpeg) incl. jitterbuffer also runs on Linux and is fully tested and fuzzed.

## Build and deploy

This is an [ESP-IDF 5](https://github.com/espressif/esp-idf) project.

```bash
source $IDF_PATH/export.sh
export ESPPORT=/dev/ttyACM0

# Configure:
cp sdkconfig.defaults.ci sdkconfig.defaults
# Now, edit WiFi credentials in sdkconfig.defaults

# Build/flash
idf.py build flash

# Lint/format
./format.sh

# Flash and monitor
idf.py -p PORT flash monitor

# Send frames
gst-launch-1.0 filesrc location=components/rtpjpeg/BigBuckBunny_320x180.mp4 \
    ! decodebin \
    ! jpegenc \
    ! rtpjpegpay seqnum-offset=63000 mtu=1400 \
    ! udpsink host=10.0.0.134 port=1234
```

## C Conventions

- Names: buf, sz, out
- Sizes: ptrdiff_t

## Ideas, TODOs

- [ ] WiFi setup, SoftAP
- [ ] Touch sensor
- [ ] RTP/MJPEG
- [ ] HTTP API, image upload
- [ ] Display image from HTTP(s)
- [ ] SPIFFS/littlefs
- [ ] Flash encryption
- [ ] OTA updates
- [ ] Display log buffer
- [ ] Struct rearrangement for size opt https://justine.lol/sizetricks/#arrange
