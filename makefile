# This makefile is made to work with the toolchain downloadable at https://launchpad.net/gcc-arm-embedded

CC = arm-none-eabi-gcc
LD = arm-none-eabi-gcc
SIZE = arm-none-eabi-size
OBJCOPY = arm-none-eabi-objcopy

CFLAGS = -I. -Os -fno-common -ffunction-sections -ffreestanding -fno-builtin -mthumb -mcpu=cortex-m3 -Wall -fstack-usage -Winline
ASMFLAGS = -S -fverbose-asm
LDFLAGS = -mcpu=cortex-m3 -mthumb -nostartfiles -gc-sections

DEPS = main.h gyro_xcel_compass.h lidar.h optflow.h own_std.h
OBJ = stm32init.o main.o gyro_xcel_compass.o lidar.o optflow.o own_std.o
ASMS = stm32init.s main.s gyro_xcel_compass.s lidar.s optflow.s own_std.s

all: main.bin

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

main.bin: $(OBJ)
	$(LD) -Tstm32.ld $(LDFLAGS) -o main.elf $^
	$(OBJCOPY) -Obinary main.elf main.bin
	$(SIZE) main.elf

flash: main.bin
	sudo stm32sprog -b 115200 -vw main.bin

stack:
	cat *.su

sections:
	arm-none-eabi-objdump -h main.elf

syms:
	arm-none-eabi-objdump -t main.elf

%.s: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS) $(ASMFLAGS)

asm: $(ASMS)

e: 
	nano main.c gyro_xcel_compass.h gyro_xcel_compass.c lidar.h lidar.c optflow.h optflow.c stm32init.c

s:
	sudo screen /dev/ttyUSB0 115200
