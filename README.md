WebRTC implementation written in C. The library aims to integrate IoT/Embedded device video/audio streaming with WebRTC, such as ESP32.

### Features

- Vdieo/Audio Codec
  - H264
  - G.711 PCM (A-law)
  - G.711 PCM (µ-law)
  - OPUS
- DataChannel
- STUN/TURN
- IPV4/IPV6
- Signaling LibWebSocket

### Dependencies

* [mbedtls](https://github.com/Mbed-TLS/mbedtls)
* [libsrtp](https://github.com/cisco/libsrtp)
* [usrsctp](https://github.com/sctplab/usrsctp)
* [cJSON](https://github.com/DaveGamble/cJSON.git)

### Getting Started with Generic Example
```bash
$ sudo apt -y install git cmake
$ git clone --recursive https://github.com/Thien021103/LibPeer2Way
$ cd LibPeer2Way
$ cmake -S . -B build && cmake --build build
$ wget http://www.live555.com/liveMedia/public/264/test.264 # Download test video file
$ wget https://mauvecloud.net/sounds/alaw08m.wav # Download test audio file
$ ./generic/sample/sample
```