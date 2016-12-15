# Troubleshooting

Here are some troubleshooting tips we've gathered while making CribSense.

### No alarm is playing

-   Are your speakers working?
-   Can you play other sounds from the Pi outside of the CribSense alarm?
-   Is your Pi trying to play audio through HDMI rather than the audio port? Check the [Raspberry Pi Audio Configuration](https://www.raspberrypi.org/documentation/configuration/audio-config.md) page to make sure that you have selected the right output.
-   Is CribSense detecting motion? Check with `journalctl -f` to see if CribSense is running in the background. If it is, perhaps you need to [calibrate CribSense](./config.md)

### IR LED is not working

- Can you see a faint red color when you look at the IR LED? A faint red ring should be visible when the LED is on.
- Check the polarity of the connections. If +5V and GND are reversed, it will not work. 
- Connect the LED to a power supply with a 5V/0.5A voltage/current limit. Normally, it should consume 0.2A at 5V. If it does not, your LED may be malfunctioning.

### CribSense is detecting motion even though there isn't an infant

-   Have you properly [configured CribSense](./config.md)?
-   Remember, CribSense is just looking for changes in pixel values
    -   Is there a moving shadow within the frame?
    -   Is there flickering or changing lighting?
    -   Is CribSense mounted to a stable surface (e.g. something that won't shake if people are walking by it)?
    -   Is there any other sources of movement in the frame (mirrors catching reflections, etc)?

### CribSense is NOT detecting motion even though there is motion

-   Have you properly [configured CribSense](./config.md)?
-   Is there anything in the way of the camera?
-   Are you able to connect to the camera from the Raspberry Pi at all? Check by running `raspistill -v` in a terminal to open the camera on the Pi for a few seconds.
-   If you look at `sudo systemctl status cribsense`, is it actually running?
-   Is your infant under a blanket that is "tented" up so that it is not making contact with the child? If there are significant air gaps between the blanket and the child, the blanket may mask the motion.
-   Can you see the motion if you amplify the video more?
-   Can you see the motion if you tune the low and high frequency cutoffs?
-   If this is happening in low-light only, did you make sure your calibration works in low-light?

### CribSense doesn't build

-   Did you [install all of the dependencies](./sw-setup.md)?

### I can't run `cribsense` from the commandline

-   Did you accidentally mistype anything when you ran `./autogen.sh --prefix=/usr --sysconfdir=/etc --disable-debug` during your software build?
-   Is `cribsense` present in `/usr/bin/`?
