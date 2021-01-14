TARGET_EXEC := ncdu
VERSION     := 1.16

TARGET_DIR ?= .
BUILD_DIR ?= build
SRC_DIRS ?= src

CXX := g++
CC := gcc

TARGET := $(TARGET_DIR)/$(TARGET_EXEC)
SRCS := $(shell find $(SRC_DIRS) -name *.cpp -or -name *.c)
OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)
DEPS := $(OBJS:.o=.d)

INC_FLAGS := -I. -Ideps
DEFINES   :=
LIBS      := -lcurses

CPPFLAGS ?= -O2 -MMD $(INC_FLAGS) $(DEFINES)
LDFLAGS += $(LIBS)



all: $(TARGET) $(TARGET_EXEC).1

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)


# c source
$(BUILD_DIR)/%.c.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# c++ source
$(BUILD_DIR)/%.cpp.o: %.cpp
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

# Don't "clean" ncdu.1, it should be in the tarball so that pod2man isn't a
# build dependency for those who use the tarball.
$(TARGET_EXEC).1: doc/ncdu.pod
	pod2man --center "ncdu manual" --release "$(TARGET_EXEC)-$(VERSION)" "doc/ncdu.pod" >$(TARGET_EXEC).1

update:
	wget -q https://raw.github.com/tylov/STC/master/stc/ccommon.h -O "deps/ccommon.h"
	wget -q https://raw.github.com/tylov/STC/master/stc/cvec.h -O "deps/cvec.h"
	wget -q https://raw.github.com/tylov/STC/master/stc/cmap.h -O "deps/cmap.h"
	@#wget -q https://raw.github.com/attractivechaos/klib/master/khashl.h -O "deps/khashl.h"
	wget -q http://g.blicky.net/ylib.git/plain/yopt.h -O "deps/yopt.h"

.PHONY: clean all

clean:
	$(RM) -r $(BUILD_DIR) $(TARGET)

-include $(DEPS)
