BUILD_DIR = build
SRCS = main.cpp
OUTPUT = area_model_generator
QUARTUS_CPP = ../common/src/quartus.cpp

# These lines are used for creating .d files with dependency information. This
# site is useful for understanding how it works:
#   http://make.mad-scientist.net/papers/advanced-auto-dependency-generation/
DEP_FLAGS = -MT $@ -MMD -MP -MF $@.Td
DEP_RENAME = mv -f $@.Td $@.d && touch $@

OBJS := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(SRCS)) $(BUILD_DIR)/quartus.o

# add include paths
CPPFLAGS += -I ../../inc -I ../common/inc

# use C++11
CPPFLAGS += -std=c++11

# configure warnings and errors
CPPFLAGS += -pedantic-errors -Wall -Wno-unused-function -Wno-unused-variable -Werror

ifeq ($(GDB),1)
  DEBUG ?= 1
endif

ifeq ($(DEBUG),1)
  CPPFLAGS += -g -O0
endif

ifeq ($(GPROF),1)
  CPPFLAGS += -pg
endif

ifeq ($(GDB),1)
  RUN_CMD := cd $(BUILD_DIR) && gdb --args ./$(OUTPUT)
else
  RUN_CMD := cd $(BUILD_DIR) && ./$(OUTPUT)
endif

.PHONY: all
all: $(BUILD_DIR)/$(OUTPUT)

ACTIONS := projects model check
.PHONY: $(ACTIONS)
$(ACTIONS): all
	$(RUN_CMD) $@

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)

$(BUILD_DIR)/$(OUTPUT): $(OBJS)
	@mkdir -p $(dir $@)
	$(LINK.cc) $^ $(LDLIBS) -o $@

$(BUILD_DIR)/%.o: %.cpp $(BUILD_DIR)/%.o.d
	@mkdir -p $(dir $@)
	$(COMPILE.cc) $(DEP_FLAGS) $< -o $@
	$(DEP_RENAME)

$(BUILD_DIR)/quartus.o: $(QUARTUS_CPP) $(BUILD_DIR)/quartus.o.d
	@mkdir -p $(dir $@)
	$(COMPILE.cc) $(DEP_FLAGS) $< -o $@
	$(DEP_RENAME)

$(BUILD_DIR)/%.o.d: ;
.PRECIOUS: $(BUILD_DIR)/%.o.d
-include $(patsubst %.cpp,$(BUILD_DIR)/%.o.d,$(SRCS))
