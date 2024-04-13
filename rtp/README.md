# RTP + MJPEG

RTP: https://datatracker.ietf.org/doc/html/rfc3550
RTP h264: https://datatracker.ietf.org/doc/html/rfc6184
RTP MJPG: https://datatracker.ietf.org/doc/html/rfc2435

https://github.com/meetecho/janus-gateway/blob/master/src/rtp.h
https://github.com/meetecho/janus-gateway/blob/master/src/rtp.c

## Gst pipeline

https://download.blender.org/peach/bigbuckbunny_movies/BigBuckBunny_320x180.mp4

Stream MJPEG via RTP:

```bash
# Send
gst-launch-1.0 filesrc location=BigBuckBunny_320x180.mp4 ! decodebin ! jpegenc ! rtpjpegpay ! udpsink host=127.0.0.1 port=1234
# Play
gst-launch-1.0 -v udpsrc address=localhost port=1234 ! application/x-rtp,encoding-name=JPEG,payload=26 ! rtpjpegdepay ! jpegdec ! videoconvert ! videoscale ! autovideosink
# Dummy receive
nc -u -l 1234 | pv > /dev/null
```

Same with h264:

```bash
gst-launch-1.0 filesrc location=BigBuckBunny_320x180.mp4 ! decodebin ! videoconvert ! video/x-raw,format=I420 ! x264enc tune=zerolatency bitrate=500 speed-preset=superfast ! rtph264pay ! udpsink host=127.0.0.1 port=1234
gst-launch-1.0 -v udpsrc port=1234 caps="application/x-rtp,media=(string)video,clock-rate=(int)90000,encoding-name=(string)H264,payload=(int)96" ! rtph264depay ! decodebin ! videoconvert ! autovideosink
```

## TODOs

- [ ] Add fuzzing
- [ ] Add menuconfig for some defs.
- [ ] Struct rearrangement for size opt https://justine.lol/sizetricks/#arrange

## Conventions

- Names: buf, sz, out
- Sizes: ptrdiff_t
- Pass structs by value
- All objects are responsible themselves to filter packets by SSRC etc.

## Simulate bad network

```bash
# Create NS
sudo ip netns add s1
sudo ip netns add s2
# Create & attach cable
sudo ip link add veth0 type veth peer name veth1
sudo ip link set veth0 netns s1
sudo ip link set veth1 netns s2
# Set IPs
sudo ip -n s1 addr add 192.168.64.1/24 dev veth0
sudo ip -n s2 addr add 192.168.64.2/24 dev veth1
# Check
sudo ip netns exec s1 ip addr
sudo ip netns exec s2 ip addr
sudo ip netns exec s1 arp
sudo ip netns exec s2 arp
sudo ip netns exec s1 route
sudo ip netns exec s2 route
# Bring up
sudo ip -n s1 link set veth0 up
sudo ip -n s2 link set veth1 up
# Test
sudo ip netns exec s1 ping 192.168.64.2
sudo ip netns exec s2 ping 192.168.64.1
# Test netcat
sudo ip netns exec s1 nc -l 1234
sudo ip netns exec s2 nc 192.168.64.1 1234

# Simulate bad network (tc always deals with outgoing)
sudo ip netns exec s2 tc qdisc add dev veth1 root netem delay 100ms 50ms 50% loss 10%
# Reset
sudo ip netns exec s2 tc qdisc replace dev veth1 root pfifo

# Run gstreamer in network ns
sudo ip netns exec s2 \
    gst-launch-1.0 filesrc location=BigBuckBunny_320x180.mp4 \
    ! decodebin \
    ! jpegenc \
    ! rtpjpegpay seqnum-offset=63000 \
    ! udpsink host=192.168.64.1 port=1234

make && sudo ip netns exec s1 ./recv
sudo ip netns exec s1 wireshark
```

https://github.com/corkami/formats/blob/master/image/jpeg.md
https://components.espressif.com/components/espressif/esp_jpeg
https://docs.espressif.com/projects/esp-idf/en/latest/esp32p4/api-reference/peripherals/jpeg.html
CONFIG_JPEG_ENABLE_DEBUG_LOG
https://github.com/espressif/esp-idf/blob/master/examples/peripherals/spi_master/lcd/main/decode_image.c#L45
