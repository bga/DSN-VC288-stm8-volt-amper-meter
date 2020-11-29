export PATH := $(PATH):$(HOME)/local/sdcc/bin

MCU  = stm8s003f3
ARCH = stm8

F_CPU   ?= 16000000
PROFILE ?= Release
TARGET  ?= iar-stm8-project/$(PROFILE)/Exe/app.out

# BUILD_DIR = ./sdcc-build

LIBDIR   = /E/p/!electronics/!stm8/stm8-bare-min/stm8s/lib

SRCS    := $(wildcard *.c $(LIBDIR)/*.c)
ASRCS   := $(wildcard src/*.s $(LIBDIR)/*.s)

all: $(TARGET) size

# $(BUILD_DIR):
# 	@mkdir -p $@

$(TARGET): $(SRCS)
	IarBuild.bat iar-stm8-project/stm8-volt-amper-meter-iar.ewp $(PROFILE)

$(TARGET)-flash.bin: $(TARGET)
	iar-stm8_dump-flash.sh $(TARGET)

$(TARGET)-eeprom.bin: $(TARGET)
	iar-stm8_dump-eeprom.sh $(TARGET)

size: $(TARGET)-flash.bin
	@echo "----------"
	@echo "Image size:"
	@stat -L -c %s $(TARGET)-flash.bin

flash-write: $(TARGET)-flash.bin
	stm8flash -c stlinkv2 -s flash -p $(MCU) -w $<
flash-read: $(TARGET)-flash.bin
	stm8flash -c stlinkv2 -s flash -p $(MCU) -r $<

eeprom-write: $(TARGET)-eeprom.bin
	stm8flash -c stlinkv2 -s eeprom -p $(MCU) -w $<
eeprom-read: $(TARGET)-eeprom.bin
	stm8flash -c stlinkv2 -s eeprom -p $(MCU) -r $<

# serial: $(TARGET).bin
# 	stm8gal -p /dev/ttyUSB0 -w $(TARGET).bin

clean:
	rm -rf iar-stm8-project/{Release,Debug,*.dep}
# 	IarBuild.bat iar-project/blink.ewp -clean Release

.PHONY: clean all size flash-write flash-read eeprom-write eeprom-read
