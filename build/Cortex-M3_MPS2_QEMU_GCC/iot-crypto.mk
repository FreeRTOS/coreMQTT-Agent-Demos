#Intended to be included from the Makefile.  Builds utility functions used by the
#OTA (Over the AIr update) PAL (Platform Abstraction Layer).

IOT_CRYPTO_DIR += ./../../lib/FreeRTOS/utilities/crypto
VPATH += $(IOT_CRYPTO_DIR)/src
INCLUDE_DIRS += -I$(IOT_CRYPTO_DIR)/include
SOURCE_FILES += $(wildcard $(IOT_CRYPTO_DIR)/src/*.c)
