#Intended to be included from the Makefile.  Builds the AWS IoT OTA (Over the Air
#update) library and correct PAL (Platform Abstraction Layer) files.


#AWS OTA PAL files - required for header file only as this points to a Win32
# version of the c file.
OTA_PAL_DIR += ./../../lib/AWS/ota-pal/Win32
#VPATH += $(OTA_PAL_DIR)
INCLUDE_DIRS += -I$(OTA_PAL_DIR) \
				-I./../../lib/FreeRTOS/utilities/crypto/include



#AWS OTA files
OTA_DIR += ./../../lib/AWS/ota/source
VPATH += $(OTA_DIR) $(OTA_DIR)/portable/os
INCLUDE_DIRS += -I$(OTA_DIR)/include \
				-I$(OTA_DIR)/portable/os
SOURCE_FILES += $(wildcard $(OTA_DIR)/*.c) \
			    ($(OTA_DIR)/portable/os/ota_os_freertos.c
