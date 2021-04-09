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
