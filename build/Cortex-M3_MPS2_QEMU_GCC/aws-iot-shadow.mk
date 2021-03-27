SHADOW_DIR += ./../../lib/AWS/shadow/source
VPATH += $(SHADOW_DIR)
INCLUDE_DIRS += -I$(SHADOW_DIR)/include
SOURCE_FILES += $(wildcard $(SHADOW_DIR)/*.c)
