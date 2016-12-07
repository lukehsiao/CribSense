# Configuration

Configuration of Cribsense uses a file which is usually located in `/etc/cribsense/config.ini`.

The format is that of INI files: each line is a configuration value, and directives are grouped in section introduced by square brackets. Comments are delimited with ; to the end of the line.

# Input/Output

The `[io]` section of the file configures the input to Cribsense. 
You should not need to modify this section, as it is already calibrated to work on the Raspberry Pi.

The `input` directive specifies that the magnifier should look for a video file instead of real-time camera input, while `camera` directive chooses the camera device to use (eg. `camera = 0` means to use `/dev/video0` as input). File input exists as a demo or debugging feature only.
If you change the input, you must also specify the fps parameters to match the input. For file input, there is only one fps setting, because frames are never dropped, while for camera input, to reduce latency you must specify the frame per second at full frame size and at cropped frame size (roughly 3x the full frame size fps). The latter values depend on the speed of the CPU on which cribsense run.

# Cropping

The `[cropping]` section controls the adaptive motion-based cropping, which focuses the magnification process on a smaller Region of Interest (ROI) where the most motion is occurring, reducing the CPU load.

The different parameters affect the latency of detection, as they control the number of "slow" frames (fully uncropped). Optimal values for the parameters depend on the target CPU. If you're running on a different device than the Pi, and it's sufficiently powerful, you can also disable cropping altogheter.

# Motion & Magnification

The `[motion]` and `[magnification]` sections control the motion detection and video magnification algorithm respectively.

These parameters depend on the setting in which the cribsense is deployed, such as lighting condition and contrast on the baby.

In general, you should not need to change the magnification setting, as it is tuned to detect normal breathing rates. You will need to calibrate `diff_threshold` and `pixel_threshold` manually, but you might want to tweak the `amplification` parameter too for best results.

# Debugging features

The `show_diff` flag in `[motion]` will show a window where the areas where motion is detected in the frame are highlighted in white. The `show_magnification` flag in `[magnification]` controls a window that shows the output of just video magnification (which should look like the camera feed, in black and white, which enlarged motion).
You can use this two flags to show the result of your changes to the motion and magnification parameters.

Finally, the `print_times` in the `[debug]` section controls printing of frame times in the standard output, which you can use to calibrate the FPS and latency settings when running on a device different than the Raspberry Pi.

These features must be left to off when cribsense is started through systemd (automatically on boot or with `systemctl start`). They are only useful if you run cribsense manually.

# Calibrating the Motion & Magnification algorithm

Calibration of the algorithm is an interative effort, with no right or wrong answer. We encourage you to experiment with various values, combining them with the debugging features, to find the combination most suitable to your environment.

As a guideline though, increasing the `amplification` and the `phase_threshold` values increases the amount of magnification applied to the input video. You should change these values until you clearly see the movement from your baby breathing, and no significant artifact in other areas of the video. If you experience artifacts, reducing the `phase_threshold` while keeping the same `amplification` might help.

As for the motion detection parameters, the main driver is the amount of noise. If you run with `show_diff` enabled and you notice that either the crop in the wrong location, or too much of the input video is white, or some completely unrelated part of the video is detected as motion (eg. a flickering lamp), you'll want to increase the `diff_threshold` until only the portion of the video corresponding to your breathing baby is white. If that's not possible, you might want to ignore parts of the motion by increasing the `pixel_threshold` value so that other moving objects are ignored.
