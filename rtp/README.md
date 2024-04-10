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
- [ ] Change RTP/JPEG decoder to only keep one frame, reordering is done by buffer now
- [ ] Decode JPEG: https://github.com/bluenviron/gortsplib/blob/main/pkg/format/rtpmjpeg/decoder.go#L119

## Conventions

- Names: buf, sz, out
- Sizes: ptrdiff_t
- Pass structs by value
- All objects are responsible themselves to filter packets by SSRC etc.
