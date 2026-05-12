# OLED SH110X Linux Driver

This is an kernel module Linux driver for SH110X devices. Currently, only SH1107 is supported. The expected usage is 
to be probed through the device tree.

## How to use

Instantiate a device on the I2C bus: 
```c
i2c0: i2c@41e00000 {
    oled_screen: oled@3c {
    compatible = "sinowealth,sh110x";
    reg = <0x3c>;
    brightness = 200;
    };
};
```

To write to the screen: 
```shell
echo -n " " > /sys/bus/i2c/devices/0-003c/text # this clears the screen
echo "hello world" > /sys/bus/i2c/devices/0-003c/text
```

You can also try with this test script
```bash
#!/bin/bash

DELAY=0.1
TEXT_DEVICE=/sys/bus/i2c/drivers/i2c-sh110x/0-003c/text
CURSOR_DEVICE=/sys/bus/i2c/drivers/i2c-sh110x/0-003c/cursor

sleep 1
echo -n " " > ${TEXT_DEVICE}
echo -n "r" > ${CURSOR_DEVICE}
echo -n "n" > ${CURSOR_DEVICE}
echo "Driver Loaded!" > ${TEXT_DEVICE}
sleep 1
echo -n "r" > ${CURSOR_DEVICE}
for i in {0..8}; do
    for s in {0..4}; do
        echo -n "i" > ${CURSOR_DEVICE}
        echo "././././././././" > ${TEXT_DEVICE}
        echo -n "0 ${i}" > ${CURSOR_DEVICE}
	sleep ${DELAY}
        echo "/./././././././." > ${TEXT_DEVICE}
        echo -n "0 ${i}" > ${CURSOR_DEVICE}
	sleep ${DELAY}
        echo -n "n" > ${CURSOR_DEVICE}
    done
    echo -n "0 ${i}" > ${CURSOR_DEVICE}
    echo "OK!             " > ${TEXT_DEVICE}
done
```

