#
#             LUFA Library
#     Copyright (C) Dean Camera, 2014.
#
#  dean [at] fourwalledcubicle [dot] com
#           www.lufa-lib.org
#
# --------------------------------------
#         LUFA Project Makefile.
# --------------------------------------

# Run "make help" for target help.

VICTIM		 ?= OTA_12940
MCU          = at90usb1286
#MCU          = atmega32u4
ARCH         = AVR8
BOARD        = TEENSY2
F_CPU        = 16000000
F_USB        = $(F_CPU)
OPTIMIZATION = s
TARGET       = usb_hub
SRC          =  src/usb_device.c \
                src/usb_fake.c \
                src/main.c \
                src/usb_hub.c \
                src/usb_payload.c \
                src/usb_mem.c \
                src/uart.c \
                $(LUFA_SRC_USB) \
                $(LUFA_SRC_USBCLASS)
LUFA_PATH    = ../../LUFA
CC_FLAGS     = -D$(VICTIM) -DUSE_LUFA_CONFIG_HEADER -IConfig/ -Iinclude/
LD_FLAGS     =

# Default target
all:

# Include LUFA build script makefiles
include $(LUFA_PATH)/Build/lufa_core.mk
include $(LUFA_PATH)/Build/lufa_sources.mk
include $(LUFA_PATH)/Build/lufa_build.mk
include $(LUFA_PATH)/Build/lufa_cppcheck.mk
include $(LUFA_PATH)/Build/lufa_doxygen.mk
include $(LUFA_PATH)/Build/lufa_dfu.mk
include $(LUFA_PATH)/Build/lufa_hid.mk
include $(LUFA_PATH)/Build/lufa_avrdude.mk
include $(LUFA_PATH)/Build/lufa_atprogram.mk
