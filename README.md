# GPIO joypad driver for Odroid C0/C1/C1+
This is a GamePad emulator using GPIO and analog an MC14051B multiplexer for Odroid C.

This tool requires WiringPi. It creates a Gamepad using linux's uinput and reads states from GPIO.
This tool has been designed to work with a MC14051B analog multiplexer and 2 PSP thumbsticks.

It has been created for my GameOdroiD C0, a home made portable retrogaming console based on an Odroid C0 SBC.
More on http://www.bluemind.org/gamodroid-c0-odroid-based-portable-retrogaming

It handle special keys for ajusting display contrast and brighness throught /sysfs/class/video/vpp*:
- start + up = more brightness
- start + down = less brightness
- start + right = more contrast
- start + left = less contrast

Value are saved in /media/boot/contrast and /media/boot/brightness

How to compile:
gcc -O2 -o gpio_joypad gpio_joypad.c -lwiringPi -lpthread -lm -lrt -lcrypt

Below are GPOI wiring plan and MC14051B schema. You can adapt gpio_joypad.h for a different wiring.

![Wiring plan](https://github.com/jit06/gpio_joypad/raw/master/gpio_joypad_wiring.png)
![MC14051B schema](https://github.com/jit06/gpio_joypad/raw/master/MC14051B.jpg) 