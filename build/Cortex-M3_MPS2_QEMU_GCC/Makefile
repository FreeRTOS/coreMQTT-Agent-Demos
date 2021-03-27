OUTPUT_DIR := ./output
LIB_OUTPUT_DIR := ./output/lib
IMAGE := RTOSDemo.elf
CC = arm-none-eabi-gcc
LD = arm-none-eabi-gcc

#must be the first include to ensure the correct FreeRTOSConfig.h is used.
INCLUDE_DIRS += -I.
INCLUDE_DIRS += -I./../../lib/FreeRTOS/utilities/logging
INCLUDE_DIRS += -I./../../source/configuration-files
INCLUDE_DIRS += -I./../../lib/ThirdParty/mbedtls/include


#kernel files
KERNEL_DIR += ./../../lib/FreeRTOS/freertos-kernel
INCLUDE_DIRS += -I$(KERNEL_DIR)/include \
				-I$(KERNEL_DIR)/portable/GCC/ARM_CM3
VPATH += $(KERNEL_DIR) $(KERNEL_DIR)/portable/GCC/ARM_CM3 $(KERNEL_DIR)/portable/MemMang
SOURCE_FILES += $(KERNEL_DIR)/tasks.c
SOURCE_FILES += $(KERNEL_DIR)/list.c
SOURCE_FILES += $(KERNEL_DIR)/queue.c
SOURCE_FILES += $(KERNEL_DIR)/timers.c
SOURCE_FILES += $(KERNEL_DIR)/event_groups.c
SOURCE_FILES += $(KERNEL_DIR)/stream_buffer.c
SOURCE_FILES += $(KERNEL_DIR)/portable/MemMang/heap_4.c
SOURCE_FILES += $(KERNEL_DIR)/portable/GCC/ARM_CM3/port.c

#coreJSON files
COREJSON_DIR += ./../../lib/FreeRTOS/coreJSON/source
VPATH += $(COREJSON_DIR)
INCLUDE_DIRS += -I$(COREJSON_DIR)/include
SOURCE_FILES += $(wildcard $(COREJSON_DIR)/*.c)

#coreMQTT and coreMQTT_AGENT files
COREMQTT_AGENT_DIR += ./../../lib/FreeRTOS/coreMQTT-Agent/source
COREMQTT_DIR = $(COREMQTT_AGENT_DIR)/dependency/coreMQTT/source

INCLUDE_DIRS += -I$(COREMQTT_AGENT_DIR)/include \
				-I$(COREMQTT_DIR)/include \
				-I$(COREMQTT_DIR)/interface

VPATH += $(COREMQTT_AGENT_DIR) $(COREMQTT_AGENT_DIR)/portable/freertos $(COREMQTT_DIR)
SOURCE_FILES += $(wildcard $(COREMQTT_AGENT_DIR)/*.c)
SOURCE_FILES += $(wildcard $(COREMQTT_AGENT_DIR)/portable/freertos/*.c)
SOURCE_FILES += $(wildcard $(COREMQTT_DIR)/*.c)

#MQTT agent interface
MQTT_AGENT_INTERFACE_DIR += ./../../lib/FreeRTOS/mqtt-agent-interface

INCLUDE_DIRS += -I$(MQTT_AGENT_INTERFACE_DIR)
VPATH += $(MQTT_AGENT_INTERFACE_DIR)
SOURCE_FILES += $(wildcard $(MQTT_AGENT_INTERFACE_DIR)/*.c)


#FreeRTOS+TCP files
FREERTOS_TCP_DIR += ./../../lib/FreeRTOS/freertos-plus-tcp

INCLUDE_DIRS += -I$(FREERTOS_TCP_DIR)/include \
				-I$(FREERTOS_TCP_DIR)/portable/Compiler/GCC \
				-I$(FREERTOS_TCP_DIR)/tools/tcp_utilities/include
VPATH += $(FREERTOS_TCP_DIR) $(FREERTOS_TCP_DIR)/portable/BufferManagement $(FREERTOS_TCP_DIR)/tools/tcp_utilities/
SOURCE_FILES += $(wildcard $(FREERTOS_TCP_DIR)/tools/tcp_utilities/*.c)
SOURCE_FILES += $(wildcard $(FREERTOS_TCP_DIR)/*.c)
SOURCE_FILES += $(FREERTOS_TCP_DIR)/portable/BufferManagement/BufferAllocation_2.c

#Ethernet driver files
ETHERNET_DRIVER_DIR += ./../../lib/FreeRTOS/freertos-plus-tcp/portable/NetworkInterface/MPS2_AN385

INCLUDE_DIRS += -I$(ETHERNET_DRIVER_DIR)
VPATH += $(ETHERNET_DRIVER_DIR) $(ETHERNET_DRIVER_DIR)/ether_lan9118
SOURCE_FILES += $(wildcard $(ETHERNET_DRIVER_DIR)/*.c)
SOURCE_FILES += $(wildcard $(ETHERNET_DRIVER_DIR)/ether_lan9118/*.c)


#Mbed TLS utilities - must be before the transport interface.
MBEDTLS_UTILS_DIR += ./../../lib/FreeRTOS/utilities/mbedtls_freertos
VPATH += $(MBEDTLS_UTILS_DIR)
INCLUDE_DIRS += -I$(MBEDTLS_UTILS_DIR)
SOURCE_FILES += $(wildcard $(MBEDTLS_UTILS_DIR)/*.c)


#network transport files
NETWORK_TRANSPORT_COMMON_DIR += ./../../lib/FreeRTOS/network_transport/freertos_plus_tcp
NETWORK_TRANSPORT_MBEDTLS_DIR += $(NETWORK_TRANSPORT_COMMON_DIR)/using_mbedtls
NETWORK_TRANSPORT_PLAINTEXT_DIR += $(NETWORK_TRANSPORT_COMMON_DIR)/using_plaintext
INCLUDE_DIRS += -I$(NETWORK_TRANSPORT_COMMON_DIR) \
				-I$(NETWORK_TRANSPORT_MBEDTLS_DIR) \
				-I$(NETWORK_TRANSPORT_PLAINTEXT_DIR)
VPATH += $(NETWORK_TRANSPORT_COMMON_DIR) $(NETWORK_TRANSPORT_MBEDTLS_DIR) $(NETWORK_TRANSPORT_PLAINTEXT_DIR)
SOURCE_FILES += $(wildcard $(NETWORK_TRANSPORT_COMMON_DIR)/*.c)
SOURCE_FILES += $(wildcard $(NETWORK_TRANSPORT_MBEDTLS_DIR)/*.c)
SOURCE_FILES += $(wildcard $(NETWORK_TRANSPORT_PLAINTEXT_DIR)/*.c)


#backoff algorithm files
BACKOFF_ALGORITHM_DIR += ./../../lib/FreeRTOS/utilities/backoffAlgorithm/source
VPATH += $(BACKOFF_ALGORITHM_DIR)
INCLUDE_DIRS += -I$(BACKOFF_ALGORITHM_DIR)/include
SOURCE_FILES += $(wildcard $(BACKOFF_ALGORITHM_DIR)/*.c)


#AWS Device Defender files
DEVICE_DEFENDER_DIR += ./../../lib/AWS/defender/source
DEVICE_DEFENDER_TOOLS_DIR += ./../../source/defender-tools
VPATH += $(DEVICE_DEFENDER_DIR) $(DEVICE_DEFENDER_TOOLS_DIR)
INCLUDE_DIRS += -I$(DEVICE_DEFENDER_DIR)/include
INCLUDE_DIRS += -I$(DEVICE_DEFENDER_TOOLS_DIR)
SOURCE_FILES += $(wildcard $(DEVICE_DEFENDER_DIR)/*.c)
SOURCE_FILES += $(wildcard $(DEVICE_DEFENDER_TOOLS_DIR)/*.c)

#tinyCBOR files
TINYCBOR_DIR += ./../../lib/AWS/ota/source/dependency/3rdparty/tinycbor/src
VPATH += $(TINYCBOR_DIR)
INCLUDE_DIRS += -I$(TINYCBOR_DIR)
SOURCE_FILES += $(TINYCBOR_DIR)/cborencoder.c \
				$(TINYCBOR_DIR)/cborencoder_close_container_checked.c \
				$(TINYCBOR_DIR)/cborerrorstrings.c \
				$(TINYCBOR_DIR)/cborparser.c \
				$(TINYCBOR_DIR)/cborparser_dup_string.c \
				$(TINYCBOR_DIR)/cborpretty.c \
				$(TINYCBOR_DIR)/cborpretty_stdio.c \
				$(TINYCBOR_DIR)/cborvalidation.c


#AWS OTA PAL files - required for header file only as this points to a Win32
# version of the c file.
OTA_PAL_DIR += ./../../lib/AWS/ota-pal/Win32
#VPATH += $(OTA_PAL_DIR)
INCLUDE_DIRS += -I$(OTA_PAL_DIR) \
				-I./../../lib/FreeRTOS/utilities/crypto/include
#SOURCE_FILES += $(wildcard $(OTA_PAL_DIR)/*.c)


#AWS OTA files
OTA_DIR += ./../../lib/AWS/ota/source
VPATH += $(OTA_DIR) $(OTA_DIR)/portable/os
INCLUDE_DIRS += -I$(OTA_DIR)/include \
				-I$(OTA_DIR)/portable/os
SOURCE_FILES += $(wildcard $(OTA_DIR)/*.c) \
			    ($(OTA_DIR)/portable/os/ota_os_freertos.c


#AWS OTA files
SHADOW_DIR += ./../../lib/AWS/shadow/source
VPATH += $(SHADOW_DIR)
INCLUDE_DIRS += -I$(SHADOW_DIR)/include
SOURCE_FILES += $(wildcard $(SHADOW_DIR)/*.c)


#IoT Crypto files
IOT_CRYPTO_DIR += ./../../lib/FreeRTOS/utilities/crypto
VPATH += $(IOT_CRYPTO_DIR)/src
INCLUDE_DIRS += -I$(IOT_CRYPTO_DIR)/include
SOURCE_FILES += $(wildcard $(IOT_CRYPTO_DIR)/src/*.c)


#application files
APPLICATION_DIR += ./../../source
VPATH += $(APPLICATION_DIR) $(APPLICATION_DIR)/subscription-manager $(APPLICATION_DIR)/shadow-tasks
INCLUDE_DIRS += -I$(APPLICATION_DIR)/subscription-manager -I./CMSIS
SOURCE_FILES += $(wildcard $(APPLICATION_DIR)/*.c)
SOURCE_FILES += $(wildcard $(APPLICATION_DIR)/subscription-manager/*.c)
SOURCE_FILES += $(wildcard $(APPLICATION_DIR)/shadow-tasks/*.c)
SOURCE_FILES += $(wildcard *.c)

include mbedtls.mk

#build flags
CFLAGS += $(INCLUDE_DIRS) -nostartfiles -ffreestanding -mthumb -mcpu=cortex-m3 -Wall -Wextra -g3 -O0 -ffunction-sections -fdata-sections -MMD -MP -MF"$(@:%.o=%.d)" -MT $@

OBJS = $(SOURCE_FILES:%.c=%.o)
OBJS_NO_PATH = $(notdir $(OBJS))
OBJS_OUTPUT = $(OBJS_NO_PATH:%.o=$(OUTPUT_DIR)/%.o)

DEP_FILES := $(SOURCE_FILES:%.c=$(OUTPUT_DIR)/%.d)
DEP_FILES_NO_PATH = $(notdir $(DEP_FILES))
DEP_OUTPUT = $(DEP_FILES_NO_PATH:%.d=$(OUTPUT_DIR)/%.d)

all: $(OUTPUT_DIR)/$(IMAGE)

%.o : %.c
$(OUTPUT_DIR)/%.o : %.c $(OUTPUT_DIR)/%.d Makefile
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTPUT_DIR)/$(IMAGE): ./mps2_m3.ld $(OBJS_OUTPUT) Makefile  ./FreeRTOSConfig.h ./FreeRTOSIPConfig.h
	$(LD) $(OBJS_OUTPUT) $(CFLAGS) -Xlinker --gc-sections -Xlinker -T ./mps2_m3.ld -Xlinker -Map=$(OUTPUT_DIR)/RTOSDemo.map -specs=nano.specs -specs=nosys.specs -specs=rdimon.specs -o $(OUTPUT_DIR)/$(IMAGE)

$(DEP_OUTPUT):
include $(wildcard $(DEP_OUTPUT))

clean:
	rm -f $(OUTPUT_DIR)/$(IMAGE) $(OUTPUT_DIR)/*.o $(OUTPUT_DIR)/*.d

print-%  : ; @echo $* = $($*)

.PHONY: all clean


