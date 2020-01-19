LOCAL_PATH := $(call my-dir)

install: LIB_PATH := $(call my-dir)/../assets

include $(CLEAR_VARS)

LOCAL_MODULE    := pktriggercord-cli
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../../src/external/js0n/
LOCAL_SRC_FILES := ../../src/external/js0n/js0n.c \
	../../pslr_enum.c \
	../../pslr_lens.c \
	../../pslr_model.c \
	../../pslr_scsi.c \
	../../pslr.c \
	../../pktriggercord-servermode.c \
	../../pktriggercord-cli.c
DEFINES 	:= -DANDROID -DVERSION=\"$(VERSION)\" -DPKTDATADIR=\".\"
LOCAL_CFLAGS  	:= $(DEFINES) -frtti -Isrc/external/js0n -Istlport -g -fPIE
LOCAL_LDLIBS	:= -llog -lstdc++ -fPIE -pie

include $(BUILD_EXECUTABLE)

install: $(LOCAL_INSTALLED)
	-mkdir -p $(LIB_PATH)
	-rm -r $(LIB_PATH)/*
	mv $< $(LIB_PATH)
