# ctop Makefile
# A simple system resource monitor in C

CC = clang
CFLAGS = -Wall -Wextra -O2 -std=c99
LDFLAGS = 
TARGET = dtop

# Source files
SRC_DIR = src
SRCS = $(TARGET).c $(SRC_DIR)/utils.c $(SRC_DIR)/stats.c $(SRC_DIR)/draw.c $(SRC_DIR)/ui.c
OBJS = $(SRCS:.c=.o)
DEPS = config.h termbox2.h

# Detect OS for potential platform-specific flags
UNAME_S := $(shell uname -s)

# Installation prefix (default: /usr/local)
PREFIX ?= /usr/local
INSTALL_DIR = $(PREFIX)/bin

# Linux-specific flags
ifeq ($(UNAME_S),Linux)
    LDFLAGS += -lm
endif

# macOS-specific flags
ifeq ($(UNAME_S),Darwin)
    CFLAGS += -D_DARWIN_C_SOURCE
endif

.PHONY: all clean install uninstall debug rebuild help

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LDFLAGS)

# Pattern rule for object files
%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c $< -o $@

debug: CFLAGS += -g -DDEBUG -O0
debug: clean $(TARGET)

# Clean build artifacts
clean:
	rm -f $(TARGET) $(OBJS)
	rm -f main.o
	rm -f $(SRC_DIR)/*.o

# Install to system
install: $(TARGET)
	install -d $(INSTALL_DIR)
	install -m 755 $(TARGET) $(INSTALL_DIR)/

# Uninstall from system
uninstall:
	rm -f $(INSTALL_DIR)/$(TARGET)

# Rebuild from scratch
rebuild: clean all

# Show help
help:
	@echo "ctop - System Resource Monitor"
	@echo ""
	@echo "Available targets:"
	@echo "  all        - Build ctop (default)"
	@echo "  debug      - Build with debug symbols and no optimization"
	@echo "  clean      - Remove build artifacts"
	@echo "  install    - Install to $(INSTALL_DIR)"
	@echo "  uninstall  - Remove from $(INSTALL_DIR)"
	@echo "  rebuild    - Clean and rebuild"
	@echo "  help       - Show this help message"
	@echo ""
	@echo "Variables:"
	@echo "  PREFIX     - Installation prefix (default: /usr/local)"
	@echo "  CC         - C compiler (default: gcc)"
	@echo ""
	@echo "Examples:"
	@echo "  make"
	@echo "  make debug"
	@echo "  make install PREFIX=/usr"
	@echo "  make clean"
