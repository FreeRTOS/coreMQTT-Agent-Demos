#Intended to be included from the Makefile.  Builds the AWS IoT Devices Defender
#library.

DEVICE_DEFENDER_DIR += ./../../lib/AWS/defender/source
DEVICE_DEFENDER_TOOLS_DIR += ./../../source/defender-tools
VPATH += $(DEVICE_DEFENDER_DIR) $(DEVICE_DEFENDER_TOOLS_DIR)
INCLUDE_DIRS += -I$(DEVICE_DEFENDER_DIR)/include
INCLUDE_DIRS += -I$(DEVICE_DEFENDER_TOOLS_DIR)
SOURCE_FILES += $(wildcard $(DEVICE_DEFENDER_DIR)/*.c)
SOURCE_FILES += $(wildcard $(DEVICE_DEFENDER_TOOLS_DIR)/*.c)
