#Intended to be included from the Makefile.  Builds the third party Mbed TLS
#library.

MBEDTLS_DIR += ./../../lib/ThirdParty/mbedtls
VPATH += $(MBEDTLS_DIR)/library
INCLUDE_DIRS += -I$(MBEDTLS_DIR)/library -I$(MBEDTLS_DIR)/include
SOURCE_FILES += $(wildcard $(MBEDTLS_DIR)/library/*.c)
