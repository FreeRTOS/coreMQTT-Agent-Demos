# mbedTLS is build to an archive by a separate makefile, but that separate
# makefile is called from this makefile.  Therefore calling "make" will build
# the application and call the mbedTLS makefile to ensure the mbedTLS archive
# is up to date.  "make clean" will clean the application code, and
# "make clean_mbedtls" will clean the mbedTLS archive.
#
# The library-makefiles directory contains a makefile snippet for each library
# built by this makefile.  Each makefile snippet adds the source files and
# include paths necessary to build the corresponding library.

OUTPUT_DIR := ./output
IMAGE := RTOSDemo.elf
SUB_MAKEFILE_DIR = ./library-makefiles

CC = arm-none-eabi-gcc
LD = arm-none-eabi-gcc
MAKE = make


#mbedTLS is built to an archive to speed up the application build.
MBED_TLS_LIB = $(OUTPUT_DIR)/mbedTLS/mbedTLS.a

CFLAGS += $(INCLUDE_DIRS) -nostartfiles -ffreestanding -mthumb -mcpu=cortex-m3 \
		  -Wall -Wextra -g3 -O0 -ffunction-sections -fdata-sections \
		  -DMBEDTLS_CONFIG_FILE='<mbedtls_config.h>' -MMD -MP -MF"$(@:%.o=%.d)" -MT $@

#must be the first include paths to ensure the correct FreeRTOSConfig.h is used.
INCLUDE_DIRS += -I./target-specific-source
INCLUDE_DIRS += -I./target-specific-source/CMSIS
INCLUDE_DIRS += -I./../../lib/FreeRTOS/utilities/logging
INCLUDE_DIRS += -I./../../source/configuration-files
INCLUDE_DIRS += -I./../../lib/ThirdParty/mbedtls/include
INCLUDE_DIRS += -I./../../lib/FreeRTOS/utilities/mbedtls_freertos

#FreeRTOS specific library includes
include $(SUB_MAKEFILE_DIR)/freertos-kernel.mk
include $(SUB_MAKEFILE_DIR)/freertos-plus-tcp.mk

#Standalone libraries, with the transport interface to link the coreMQTT library
#to the FreeRTOS+TCP library.
include $(SUB_MAKEFILE_DIR)/coremqtt-agent.mk
include $(SUB_MAKEFILE_DIR)/corejson.mk
include $(SUB_MAKEFILE_DIR)/transport-interface.mk

#AWS IoT service client libraries
include $(SUB_MAKEFILE_DIR)/aws-iot-ota.mk
include $(SUB_MAKEFILE_DIR)/aws-iot-shadow.mk
include $(SUB_MAKEFILE_DIR)/aws-iot-device-defender.mk

#Utility libraries.  The backoff algorithm calculates a the time to wait between
#attempts to connect to the MQTT broker.  The time increases exponentially and
#includes some timing jitter so a fleet of IoT devices that all get disconnected
#at the same time don't then all try to reconnect at exactly the same time.
include $(SUB_MAKEFILE_DIR)/backoff-algorithm.mk

#Third party libraries - mbedTLS is also a third party library but has its own
#makefile.
include $(SUB_MAKEFILE_DIR)/tinycbor.mk

#Application source files
APPLICATION_DIR += ./../../source
BUILD_SPECIFIC_FILES += ./target-specific-source
VPATH += $(APPLICATION_DIR) $(APPLICATION_DIR)/subscription-manager $(APPLICATION_DIR)/demo-tasks $(BUILD_SPECIFIC_FILES)
INCLUDE_DIRS += -I$(APPLICATION_DIR)/subscription-manager -I./CMSIS -I$(BUILD_SPECIFIC_FILES)
SOURCE_FILES += $(wildcard $(APPLICATION_DIR)/*.c)
SOURCE_FILES += $(wildcard $(APPLICATION_DIR)/subscription-manager/*.c)
SOURCE_FILES += $(wildcard $(APPLICATION_DIR)/demo-tasks/*.c)
SOURCE_FILES += $(wildcard $(BUILD_SPECIFIC_FILES)/startup.c)
SOURCE_FILES += $(wildcard $(BUILD_SPECIFIC_FILES)/logging_output_qemu.c)

#Create a list of object files with the desired output directory path.
OBJS = $(SOURCE_FILES:%.c=%.o)
OBJS_NO_PATH = $(notdir $(OBJS))
OBJS_OUTPUT = $(OBJS_NO_PATH:%.o=$(OUTPUT_DIR)/%.o)

#Create a list of dependency files with the desired output directory path.
DEP_FILES := $(SOURCE_FILES:%.c=$(OUTPUT_DIR)/%.d)
DEP_FILES_NO_PATH = $(notdir $(DEP_FILES))
DEP_OUTPUT = $(DEP_FILES_NO_PATH:%.d=$(OUTPUT_DIR)/%.d)

all: $(OUTPUT_DIR)/$(IMAGE)

%.o : %.c
$(OUTPUT_DIR)/%.o : %.c $(OUTPUT_DIR)/%.d Makefile
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTPUT_DIR)/$(IMAGE): ./mps2_m3.ld $(OBJS_OUTPUT) Makefile FORCE_MBED_TLS_ARCHIVE_BUILD
	@echo ""
	@echo ""
	@echo "--- Final linking ---"
	@echo ""
	$(LD) $(OBJS_OUTPUT) $(MBED_TLS_LIB) $(CFLAGS) -Xlinker --gc-sections -Xlinker -T ./mps2_m3.ld \
		-Xlinker -Map=$(OUTPUT_DIR)/RTOSDemo.map -specs=nano.specs \
		-specs=nosys.specs -specs=rdimon.specs -o $(OUTPUT_DIR)/$(IMAGE)

$(DEP_OUTPUT):
include $(wildcard $(DEP_OUTPUT))

#Phony dependency of the executable image used to force make to get called for
#the makefile that builds the mbedTLS archive in case one of its dependencies
#has changed necessitating the archive be updated.
FORCE_MBED_TLS_ARCHIVE_BUILD:
	@echo ""
	@echo ""
	@echo "--- Building mbedTLS archive source files (mbedTLS-archive.mk) ---"
	@echo ""
	$(MAKE) -f mbedTLS-archive.mk

clean:
	rm -f $(OUTPUT_DIR)/$(IMAGE) $(OUTPUT_DIR)/*.o $(OUTPUT_DIR)/*.d

clean_mbedtls:
	rm -f $(OUTPUT_DIR)/mbedTLS/*.a $(OUTPUT_DIR)/mbedTLS/*.o $(OUTPUT_DIR)/mbedTLS/*.d

#use "make print-[VARIABLE_NAME] to print the value of a variable generated by
#this makefile.
print-%  : ; @echo $* = $($*)

.PHONY: all clean clean_mbedtls FORCE_MBED_TLS_ARCHIVE_BUILD


