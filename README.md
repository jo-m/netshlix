```bash
# Build
idf.py build

# Configure
idf.py menuconfig
idf.py save-defconfig
python -m kconfcheck ...

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
- [ ] HTTP API, image upload
- [ ] Display image from HTTP(s)
- [ ] SPIFFS/littlefs
- [ ] Flash encryption
- [ ] OTA updates
- [ ] Struct rearrangement for size opt https://justine.lol/sizetricks/#arrange
- [ ] Linting: clang-format, menuconfig format `python -m kconfcheck`
