# Hardare Setup Guide

![hw-blocks](../img/hw-blocks.png)

**Figure TODO: Hardware Block Diagram**

CribSense is relatively simple as far as hardware goes, and is largely made up of commercially available items. As seen in Figure TODO, there are only 5 main hardware components, and only 2 of them are custom made. This page will walk through how to construct the IR LED circuit, and the chassis we used.

## What you'll need

-   [Raspberry Pi 3 Model B](https://www.amazon.com/Raspberry-Pi-RASP-PI-3-Model-Motherboard/dp/B01CD5VC92/)
-   [5V 2.5A Micro USB Power Supply](https://www.amazon.com/gp/product/B00MARDJZ4/)
-   [Raspberry Pi NoIR Camera Module V2](https://www.amazon.com/Raspberry-Pi-NoIR-Camera-Module/dp/B01ER2SMHY)
-   [1W IR LED](https://www.amazon.com/DIYmall%C2%AE-Infrared-Adjustable-Resistor-Raspberry/dp/B00NUOO1HQ)
-   [MicroSD Card](https://www.amazon.com/Samsung-Class-Adapter-MB-MP32DA-AM/dp/B00IVPU7KE) (we went with a class 10 16GB card, the faster the card the better)
-   [Flex Cable for Raspberry Pi Camera (12")](https://www.adafruit.com/products/1648)
-   [\[3x\] 1N4001 diodes](https://www.adafruit.com/product/755)
-   [1 ohm, 1W resistor](http://www.parts-express.com/10-ohm-1w-flameproof-resistor-10-pcs--003-1)
-   Wires
-   Soldering iron
-   3D printer for chassis
-   Speakers with 3.5mm input

## Prerequisites

Before you start our step-by-step guide, you should have already installed the latest version of [Raspbian](https://www.raspberrypi.org/downloads/raspbian/) on your SD card and ensured that your Pi is functional and booting. You'll also need to [enable the camera module](https://www.raspberrypi.org/documentation/configuration/camera.md) before you'll be able to interface with the camera.

## Build Instructions

### 3D Printed Chassis

TODO: William to fill in instructions about how to use the source files to print out the chassis (or links to good tutorials)

TODO: Build process along with pictures.

### IR LED Circuit

In order to provide adequate lighting at night, we use an IR LED, which is not visible to the human eye, but brightly illuminating for our NoIR camera. Because the Pi is plugged in, and because the LED is relatively low power, we just leave it on for simplicity.

To power the LED from the GPIO header pins on the Pi, we construct the circuit in Figure TODO.

![led](../img/led-schematic.png)
**Figure TODO: LED Schematic**

In earlier versions of the Pi, the maximum current output of these pins was [50mA](http://pinout.xyz/pinout/pin1_3v3_power). The Raspberry Pi B+ increased this to 500mA. However, for simplicity and backwards compatibility, we just use the 5V power pins, which can supply up to [1.5A](http://pinout.xyz/pinout/pin2_5v_power). The forward voltage of the IR LED is about 1.7~1.9V according to our measurements. Although the IR LED can draw 500mA without damaging itself, we decided to reduce the current to around 200mA to reduce heat and overall power consumption. Experimental result also show that the IR LED is bright enough with this input. To bridge the gap between 5V and 1.9V, we decided to use three 1N4001 diodes and a 1 Ohm resistor in series with the IR LED. The voltage loss on the wire is about 0.2V, over the diodes it's 0.9V (for each one), and over the resistor it's 0.2V. So the voltage over the IR LED is `5V - 0.2V - (3 * 0.9V) - 0.2V = 1.9V`. The heat dissipation over each LED is 0.18W, and is 0.2W over the resistor, all well within the maximum ratings.

The completed circuit is pictured below:

TODO: Add pictures of the actual circuit.
