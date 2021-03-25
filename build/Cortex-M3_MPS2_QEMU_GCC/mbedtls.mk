LIB_OUTPUT_DIR := ./output/lib
IMAGE := libmbedtls.a
CC = arm-none-eabi-gcc
AR = arm-none-eabi-ar

#must be the first include to ensure the correct FreeRTOSConfig.h is used.
INCLUDE_DIRS += -I.
INCLUDE_DIRS += -I./../../source/configuration-files
INCLUDE_DIRS += -I./../../lib/ThirdParty/mbedtls/include
INCLUDE_DIRS += -I./../../lib/FreeRTOS/utilities/mbedtls_freertos
INCLUDE_DIRS += -I./../../lib/FreeRTOS/freertos-kernel/include
INCLUDE_DIRS += -I./../../lib/FreeRTOS/freertos-kernel/portable/GCC/ARM_CM3

#Mbed TLS
MBEDTLS_DIR += ./../../lib/ThirdParty/mbedtls
VPATH += $(MBEDTLS_DIR)/library
INCLUDE_DIRS += -I$(MBEDTLS_DIR)/library -I$(MBEDTLS_DIR)/include
SOURCE_FILES += $(wildcard $(MBEDTLS_DIR)/library/*.c)

#build flags
CFLAGS += $(INCLUDE_DIRS) -mthumb -mcpu=cortex-m3 -Wall -Wextra -g3 -O0 -ffunction-sections -fdata-sections -MMD

all: $(LIB_OUTPUT_DIR)/$(IMAGE)

OBJS = $(SOURCE_FILES:%.c=%.o)
OBJS_NO_PATH = $(notdir $(OBJS))
OBJS_OUTPUT = $(OBJS_NO_PATH:%.o=$(LIB_OUTPUT_DIR)/%.o)

$(LIB_OUTPUT_DIR)/%.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(LIB_OUTPUT_DIR)/$(IMAGE): $(OBJS_OUTPUT) Makefile
	$(AR) -rc $(LIB_OUTPUT_DIR)/$(IMAGE) $(OBJS_OUTPUT)

clean:
	rm -f $(LIB_OUTPUT_DIR)/$(IMAGE) $(LIB_OUTPUT_DIR)/*.o $(LIB_OUTPUT_DIR)/*.d

.PHONY: all clean

