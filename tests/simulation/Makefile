BUILD_DIR = build
SRCS = main.cpp
OUTPUT = test_runner

# These lines are used for creating .d files with dependency information. This
# site is useful for understanding how it works:
#   http://make.mad-scientist.net/papers/advanced-auto-dependency-generation/
DEP_FLAGS = -MT $@ -MMD -MP -MF $@.Td
DEP_RENAME = mv -f $@.Td $@.d && touch $@

OBJS := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(SRCS))

# add path to find pe_array.h
CPPFLAGS += -I ../../inc

# use C++11
CPPFLAGS += -std=c++11 -g

# configure warnings and errors
#CPPFLAGS += -pedantic-errors -Wall -Wno-unused-function -Wno-unused-variable -Werror
CPPFLAGS += -pedantic-errors -Wall -Wno-unused-function -Wno-unused-variable 

.PHONY: all
all: $(BUILD_DIR)/$(OUTPUT)

# When using the "run" target you can also specify a test filter with the TEST
# argument, which will be passed to the test binary as a command line argument.
.PHONY: run
run: all
	cd $(BUILD_DIR) && ./$(OUTPUT) $(TEST)

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

$(BUILD_DIR)/%.o.d: ;
.PRECIOUS: $(BUILD_DIR)/%.o.d
-include $(patsubst %.cpp,$(BUILD_DIR)/%.o.d,$(SRCS))
