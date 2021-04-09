#Intended to be included from the Makefile.  Builds the FreeRTOS implementation
#of the Mbed TLS threading layer.

MBEDTLS_UTILS_DIR += ./../../lib/FreeRTOS/utilities/mbedtls_freertos
VPATH += $(MBEDTLS_UTILS_DIR)
INCLUDE_DIRS += -I$(MBEDTLS_UTILS_DIR)
SOURCE_FILES += $(wildcard $(MBEDTLS_UTILS_DIR)/*.c)
