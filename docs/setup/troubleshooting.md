# Troubleshooting

Here are some troubleshooting tips we've noticed while making CribSense.

### No alarm is playing

-   Have you make sure the speakers work?
-   Can you play other sounds from the Pi outside of the CribSense alarm?
-   Is your Pi trying to play audio through HDMI rather than the audio port? Check the [Raspberry Pi Audio Configuration](https://www.raspberrypi.org/documentation/configuration/audio-config.md) page to make sure you have the right output.
-   Is CribSense detecting motion? Check with `journalctl -f` if CribSense is running the the background. If it is, perhaps you need to [calibrate CribSense](./config.md)

### CribSense is detecting motion even though there isn't an infant

-   Have you properly [configured CribSense](./config.md)?
-   Remember, CribSense is just looking for changes in pixel values
    -   Is there a moving shadow within the frame?
    -   Is there flickering or changing lighting?
    -   Is CribSense mounted to a stable surface (e.g. something that won't shake if people are walking by it)?

### CribSense is NOT detecting motion even though there is motion

-   Have you properly [configured CribSense](./config.md)?
-   Is there anything in the way of the camera?
-   Are you able to connect to the camera from the Raspberry Pi at all? Check by running `raspistill -v` in a terminal to open the camera on the Pi for a few seconds.
-   If you look at `sudo systemctl status cribsense`, is it actually running?

### CribSense doesn't build

-   Did you [install all of the dependencies](./sw-setup.md)?

### I can't run `cribsense` from the commandline

-   Did you accidentally mistype anything when you ran `./autogen.sh --prefix=/usr --sysconfdir=/etc --disable-debug`?
-   Is `cribsense` present in `/usr/bin/`?
