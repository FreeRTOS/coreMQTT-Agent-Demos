#Intended to be included from the Makefile.  Builds the coreJSON library.

COREJSON_DIR += ./../../lib/AWS/ota/source/dependency/coreJSON/source
VPATH += $(COREJSON_DIR)
INCLUDE_DIRS += -I$(COREJSON_DIR)/include
SOURCE_FILES += $(wildcard $(COREJSON_DIR)/*.c)
