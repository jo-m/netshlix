```bash
# Build
idf.py build

# Configure
idf.py menuconfig

# Build, flash and monitor the project.
idf.py -p PORT flash monitor
```

## Conventions

- Names: buf, sz, out
- Sizes: ptrdiff_t
- Pass structs by value

## Ideas, TODOs

- [ ] Replace ESP_ERROR_CHECK with real error handling
- [ ] WiFi setup, SoftAP
- [ ] Touch sensor
- [ ] RTP/MJPEG
- [ ] RTP/h264
- [ ] HTTP API, image upload
- [ ] Display image from HTTP(s)
- [ ] SPIFFS
- [ ] Flash encryption
- [ ] OTA updates
- [ ] Add menuconfig for some defs
- [ ] Struct rearrangement for size opt https://justine.lol/sizetricks/#arrange
