# Hardare Setup Guide

**Figure TODO: Hardware Block Diagram**

CribSense is relatively simple as far as hardware goes, and is largely made up of commercially available items. As seen in Figure TODO, there are only 4 main hardware components, and only 2 of the 4 them are custom made. This page will walk through how to construct the IR LED circuit, and the chassis we used.

## What you'll need

-   [Raspberry Pi 3 Model B](https://www.amazon.com/Raspberry-Pi-RASP-PI-3-Model-Motherboard/dp/B01CD5VC92/)
-   [5V 2.5A Micro USB Power Supply](https://www.amazon.com/gp/product/B00MARDJZ4/)
-   [Raspberry Pi NoIR Camera Module V2](https://www.amazon.com/Raspberry-Pi-NoIR-Camera-Module/dp/B01ER2SMHY)
-   [1W IR LED](https://www.amazon.com/DIYmall%C2%AE-Infrared-Adjustable-Resistor-Raspberry/dp/B00NUOO1HQ)
-   [MicroSD Card](https://www.amazon.com/Samsung-Class-Adapter-MB-MP32DA-AM/dp/B00IVPU7KE) (we went with a class 10 16GB card, the faster the card the better)
-   [Flex Cable for Raspberry Pi Camera (18")](https://www.adafruit.com/products/1730)
-   [[3x] 1N4001 diodes](<https://www.adafruit.com/product/755>)
-   [1 ohm, 1W resistor](http://www.parts-express.com/10-ohm-1w-flameproof-resistor-10-pcs--003-1)
-   Wires
-   Soldering iron
-   3D printer for chassis

## Prerequisites

Before you start our step-by-step guide, you should have already installed the latest version of [Raspbian](https://www.raspberrypi.org/downloads/raspbian/) on your SD card and ensured that your Pi is functional and booting. You'll also need to [enable the camera module](https://www.raspberrypi.org/documentation/configuration/camera.md) before you'll be able to interface with the camera.

## Build Instructions

### 3D Printed Chassis

TODO: William to fill in instructions about how to use the source files to print out the chassis (or links to good tutorials)

TODO: Build process along with pictures.

### IR LED Circuit

In order to provide adequate lighting at night, we use an IR LED, which is not visible to the human eye, but brightly illuminating for our NoIR camera. Because the Pi is plugged in, and because the LED is relatively low power, we can just leave it on for simplicity.

To power the LED from the GPIO header pins on the Pi, we construct the circuit in Figure TODO.

![led](../img/led-schematic.png)
**Figure TODO: LED Schematic**

TODO: Pan fill out description of why the diodes are there. Link to the GPIO documentation so people know why those are the pins used. Talk about the LED's specs etc.
TODO: Add pictures of the actual circuit.
