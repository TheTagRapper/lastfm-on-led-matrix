# lastfm-on-led-matrix

This project retrieves whatever song is currently playing and it's cover art, using the LastFM API and drives it to a 64x64 LED Matrix display.

This project is built using a Raspberry Pi Pico WH using the libraries:
	- lwIP to handle HTTP Requests
	- mbedtls in order to handle the TLS.
	- cJSON to parse the JSON


## Checklist

This checklist is not final as there are certain stages which I may have not reached yet that are actually more complex than they actually are

Linear Progress:
- [x] Establish WiFi Connection
- [x] Build out lwIP callback functions
- [x] Successfully request LastFM API Track Data
- [x] Parse Track Data JSON 
- [x] Construct Request for Track Image
- [ ] Build PNG->PPM Decoder
- [ ] Build JPG->PPM Decoder
- [ ] Create 64x64 Frame Buffer
- [ ] Build Driver for HUB75
- [ ] Drive Buffer to LED Screen

Touchup Progress:
- [ ] Retry Wifi Connection on Failure
- [ ] Automatic Repeated Requesting on Data

## How to Build

First, you would need a LastFM API Key in order to use this service. There is no fee and you can get it pretty much instantly.

Second of all, you should have both CMake and Make installed

Other than that, the process is easy to setup.

1. Clone the repo whether that be via git CLI or downloading via zip on pressing code

2. Go to `env_template.h` and add:
	- Wifi Name
	- Wifi Password
	- The user who you are trying to track
	- The API key you received from LastFM

3. Rename `env_template.h` to `env.h`

4. Make a folder named `build`

5. Inside `build`, open your terminal/command prompt/powershell

6. Run `cmake ..`

7. Run `make -j4`

8. If the process is successful, you should have the file `build_main.uf2`

9. Holding the **BOOTSEL** Button on your Raspberry Pi Pico W, connect it via USB to your computer

10. Drag over the .uf2 file over to the Raspberry Pi

11. At this current stage, you can use PuTTy on Windows to see the output or minicom on linux using the command `sudo minicom -b 115200 -D /dev/ttyACM0`
