# Troubleshooting
Here are some troubleshooting tips we've noticed while making CribSense.

### No alarm is playing
- Have you make sure the speakers work?
- Can you play other sounds from the Pi outside of the CribSense alarm?
- Is your Pi trying to play audio through HDMI rather than the audio port? Check the [Raspberry Pi Audio Configuration](https://www.raspberrypi.org/documentation/configuration/audio-config.md) page to make sure you have the right output.
- Is CribSense detecting motion? Check with `journalctl -f` if CribSense is running the the background. If it is, perhaps you need to [calibrate CribSense](./config.md)

### CribSense is detecting motion even though there isn't an infant
- Have you properly [configured CribSense](./config.md)?
- Remember, CribSense is just looking for changes in pixel values
  - Is there a moving shadow within the frame?
  - Is there flickering or changing lighting?
  - Is CribSense mounted to a stable surface (e.g. something that won't shake if people are walking by it)?
