.PHONY: test

PROJECT_NAME := seclet

EXTRA_COMPONENT_DIRS := $(CURDIR)/vendor

include $(IDF_PATH)/make/project.mk

indent:
	# That's One True Style
	astyle -A10 -r "main/*.c" "main/*.h" \
		"components/*.c" "components/*.h"
