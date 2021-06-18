# Copyright 2020 Bga <bga.email@gmail.com>

# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at

#   http://www.apache.org/licenses/LICENSE-2.0

# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


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
	tools/iar-stm8_dump-flash.sh $(TARGET)

$(TARGET)-eeprom.bin: $(TARGET)
	tools/iar-stm8_dump-eeprom.sh $(TARGET)

size: $(TARGET)-flash.bin
	@echo "----------"
	@echo "Image size:"
	@stat -L -c %s $(TARGET)-flash.bin

flash-write: $(TARGET)-flash.bin
	stm8flash -c stlinkv2 -s flash -p $(MCU) -w $<
flash-verify: $(TARGET)-flash.bin
	stm8flash -c stlinkv2 -s flash -p $(MCU) -v $<
flash-read: $(TARGET)-flash.bin
	stm8flash -c stlinkv2 -s flash -p $(MCU) -r $<

eeprom-write: $(TARGET)-eeprom.bin
	stm8flash -c stlinkv2 -s eeprom -p $(MCU) -w $<
eeprom-verify: $(TARGET)-eeprom.bin
	stm8flash -c stlinkv2 -s eeprom -p $(MCU) -v $<
eeprom-read: $(TARGET)-eeprom.bin
	stm8flash -c stlinkv2 -s eeprom -p $(MCU) -r $<

# serial: $(TARGET).bin
# 	stm8gal -p /dev/ttyUSB0 -w $(TARGET).bin

clean:
	rm -rf iar-stm8-project/{Release,Debug,*.dep}
# 	IarBuild.bat iar-project/blink.ewp -clean Release

.PHONY: clean all size flash-write flash-read eeprom-write eeprom-read
