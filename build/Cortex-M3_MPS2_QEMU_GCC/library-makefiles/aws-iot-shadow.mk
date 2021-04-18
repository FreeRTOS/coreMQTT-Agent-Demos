#Intended to be included from the Makefile.  Builds the AWI IoT Shadow client
#library.

SHADOW_DIR += ./../../lib/AWS/shadow/source
VPATH += $(SHADOW_DIR)
INCLUDE_DIRS += -I$(SHADOW_DIR)/include
SOURCE_FILES += $(wildcard $(SHADOW_DIR)/*.c)
