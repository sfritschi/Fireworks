# C Compilation Configuration
CC=clang
CFLAGS=-Wall -Wextra -Wpedantic -std=c17
LDFLAGS=-lglfw -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi -lm

INCLUDE =-Iinclude
# Include CGLM library for 2D/3D/4D math
INCLUDE+=-I$(USER_LIB_DIR)/cglm/include
SRCDIR=src
OBJDIR=bin

SRC=$(wildcard $(SRCDIR)/*.c)
OBJ=$(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SRC))

# GLSL Compilation Configuration
SHADERC=glslc
SHADER_FLAGS=-std=450core

SHADER_SRCDIR=shaders
SHADER_BINDIR=shaders/bin

SHADER_SRC=$(wildcard $(SHADER_SRCDIR)/shader.*)
SHADER_BIN=$(patsubst $(SHADER_SRCDIR)/shader.%,$(SHADER_BINDIR)/%.spv,$(SHADER_SRC))

TARGET=main
.PHONY: all, release, clean
all: $(TARGET)

all:     CFLAGS+=-gdwarf-4 -O2
release: CFLAGS+=-DNDEBUG -O3
release: $(TARGET)

# Compile C source
$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) $(INCLUDE) -c $< -o $@

# Compile GLSL source
$(SHADER_BINDIR)/%.spv: $(SHADER_SRCDIR)/shader.% | $(SHADER_BINDIR)
	$(SHADERC) $(SHADER_FLAGS) -c $< -o $@

# Link C object files (require shaders to be compiled -> needed during runtime)
$(TARGET): $(OBJ) | $(SHADER_BIN)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

# Create output directories for binaries
$(OBJDIR):
	mkdir -p $@

$(SHADER_BINDIR):
	mkdir -p $@

# Cleanup
clean:
	$(RM) $(TARGET)
	$(RM) -r $(OBJDIR)
	$(RM) -r $(SHADER_BINDIR)
