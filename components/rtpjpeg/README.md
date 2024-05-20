# Handle RTP/JPEG as per RFC 2435

- RTP: https://datatracker.ietf.org/doc/html/rfc3550
- RTP MJPG: https://datatracker.ietf.org/doc/html/rfc2435

## Gstreamer pipeline for testing

Test video download: https://download.blender.org/peach/bigbuckbunny_movies/BigBuckBunny_320x180.mp4

Stream MJPEG via RTP:
```bash
# Send
gst-launch-1.0 filesrc location=BigBuckBunny_320x180.mp4 ! decodebin \
    ! videoconvert ! videoscale ! video/x-raw,width=240,height=240 \
    ! jpegenc ! rtpjpegpay seqnum-offset=63000 mtu=1400 ! udpsink host=localhost port=1234

# Receive/play
gst-launch-1.0 -v udpsrc address=localhost port=1234 ! application/x-rtp,encoding-name=JPEG,payload=26 \
    ! rtpjpegdepay ! jpegdec ! videoconvert ! videoscale ! autovideosink

# Dummy receive
nc -u -l 1234 | pv > /dev/null
```

## Simulate bad network

There is a Linux `linux_main.c` app included for testing and development.

We set up two Linux network namespaces with one veth each.
This allows us to use `tc` to simulate jitter and packet loss.

```bash
# Create NS.
sudo ip netns add s1
sudo ip netns add s2
# Create interfaces & attach cable.
sudo ip link add veth0 type veth peer name veth1
sudo ip link set veth0 netns s1
sudo ip link set veth1 netns s2
# Set IPs.
sudo ip -n s1 addr add 192.168.64.1/24 dev veth0
sudo ip -n s2 addr add 192.168.64.2/24 dev veth1
# Check.
sudo ip netns exec s1 ip addr
sudo ip netns exec s2 ip addr
sudo ip netns exec s1 arp
sudo ip netns exec s2 arp
sudo ip netns exec s1 route
sudo ip netns exec s2 route
# Bring up.
sudo ip -n s1 link set veth0 up
sudo ip -n s2 link set veth1 up
# Test via ping.
sudo ip netns exec s1 ping 192.168.64.2
sudo ip netns exec s2 ping 192.168.64.1
# Test via netcat.
sudo ip netns exec s1 nc -l 1234
sudo ip netns exec s2 nc 192.168.64.1 1234

# Simulate bad network (tc always deals with outgoing).
sudo ip netns exec s2 tc qdisc add dev veth1 root netem delay 100ms 50ms 50% loss 10%
# Reset.
sudo ip netns exec s2 tc qdisc replace dev veth1 root pfifo

# Run gstreamer through veth1.
sudo ip netns exec s2 \
    gst-launch-1.0 filesrc location=BigBuckBunny_320x180.mp4 \
    ! decodebin \
    ! jpegenc \
    ! rtpjpegpay seqnum-offset=63000 mtu=1400 \
    ! udpsink host=192.168.64.1 port=1234

# Run our app through veth0.
mkdir -p frames
make && sudo ip netns exec s1 ./linux_main

# With valgrind (sudo apt-get install valgrind).
make clean default && sudo ip netns exec s1 valgrind --leak-check=yes ./linux_main

# With clang sanitizers (see Makefile).
make clean linux_main_san && sudo ip netns exec s1 ./linux_main_san

# Fuzz (sudo apt-get install afl++ libpcap-dev).
make clean linux_fuzztarget_pcap
# Test the target
./linux_fuzztarget_pcap seeds/rtp.pcapng
# FUZZ!!
echo core >/proc/sys/kernel/core_pattern
export AFL_SKIP_CPUFREQ=1
afl-fuzz -i seeds/ -o fuzz_out/ -- ./linux_fuzztarget_pcap  '@@'

# Run Wireshark.
sudo ip netns exec s1 wireshark
```
