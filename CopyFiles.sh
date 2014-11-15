#!/bin/sh
./dtbToolCM -o arch/arm/boot/dt.img -s 2048 -d "htc,project-id = <" -p ./scripts/dtc/ ./arch/arm/boot/dts/
find . -iname "*.ko" -type f -exec cp {} /home/schqiushui/Android/kernel_image/A51/stock/4.4.4 \;
cp ./arch/arm/boot/zImage /home/schqiushui/Android/kernel_image/A51/stock/4.4.4
cp ./arch/arm/boot/dt.img /home/schqiushui/Android/kernel_image/A51/stock/4.4.4
