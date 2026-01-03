# Makefile for building Quackle with Python3 SWIG bindings
# Based on https://github.com/quackle/quackle.git

.PHONY: all clean help

# Directories
QUACKLE_DIR = quackle
QUACKLE_BUILD_DIR = $(QUACKLE_DIR)/build
QUACKLEIO_DIR = $(QUACKLE_DIR)/quackleio
QUACKLEIO_BUILD_DIR = $(QUACKLEIO_DIR)/build
BINDINGS_DIR = $(QUACKLE_DIR)/bindings
PYTHON3_BINDINGS_DIR = $(BINDINGS_DIR)/python3
LIB_RELEASE_DIR = $(QUACKLE_DIR)/lib/release
QUACKLEIO_LIB_RELEASE_DIR = $(QUACKLEIO_DIR)/lib/release
WORDLIST_DIR = dictionary

# Default target
all: get_quackle build_libquackle build_libquackleio build_python3_bindings download_wordlist

help:
	@echo "Available targets:"
	@echo "  get_quackle           - Clone Quackle repository from GitHub"
	@echo "  build_libquackle      - Build libquackle with -fPIC"
	@echo "  build_libquackleio    - Build libquackleio with -fPIC"
	@echo "  setup_dirs            - Create required directory structure"
	@echo "  build_python3_bindings - Build Python3 SWIG bindings"
	@echo "  all                   - Build everything (default)"
	@echo "  clean                 - Clean all build artifacts"
	@echo "  download_wordlist     - Download wordlists from GitHub"
	@echo "  help                  - Show this help message"

# Clone Quackle repository
get_quackle:
	@if [ ! -d "$(QUACKLE_DIR)" ]; then \
		echo "Cloning Quackle repository..."; \
		git clone --depth 1 https://github.com/quackle/quackle.git $(QUACKLE_DIR); \
	else \
		echo "Quackle directory already exists. Use 'git pull' to update."; \
	fi

# Setup directory structure for libraries
setup_dirs:
	@echo "Setting up directory structure..."
	@mkdir -p $(LIB_RELEASE_DIR)
	@mkdir -p $(QUACKLEIO_LIB_RELEASE_DIR)
	@mkdir -p $(PYTHON3_BINDINGS_DIR)

# Build libquackle with -fPIC
build_libquackle: get_quackle setup_dirs
	@mkdir -p $(QUACKLE_BUILD_DIR)
	@cd $(QUACKLE_BUILD_DIR) && \
		cmake -DCMAKE_POSITION_INDEPENDENT_CODE=ON .. && \
		cmake --build .
	@echo "Creating symlink for libquackle..."
	@ln -sf $$(pwd)/$(QUACKLE_BUILD_DIR)/liblibquackle.a $(LIB_RELEASE_DIR)/libquackle.a
	@echo "libquackle built successfully"

# Build libquackleio with -fPIC
build_libquackleio: get_quackle setup_dirs
	@mkdir -p $(QUACKLEIO_BUILD_DIR)
	@cd $(QUACKLEIO_BUILD_DIR) && \
		cmake -DCMAKE_POSITION_INDEPENDENT_CODE=ON .. && \
		cmake --build .
	@echo "Creating symlink for libquackleio..."
	@ln -sf $$(pwd)/$(QUACKLEIO_BUILD_DIR)/libquackleio.a $(QUACKLEIO_LIB_RELEASE_DIR)/libquackleio.a
	@echo "libquackleio built successfully"

# Build Python3 SWIG bindings
build_python3_bindings: build_libquackle build_libquackleio
	@echo "Building Python3 SWIG bindings..."
	@echo "Skipping this step for now"	
	@cd $(BINDINGS_DIR) && make python3

# Clean build artifacts
clean:
	@echo "Cleaning build artifacts..."
	@rm -rf $(QUACKLE_BUILD_DIR)
	@rm -rf $(QUACKLEIO_BUILD_DIR)
	@rm -rf $(LIB_RELEASE_DIR)
	@rm -rf $(QUACKLEIO_LIB_RELEASE_DIR)
	@rm -f $(PYTHON3_BINDINGS_DIR)/quackle_wrap.cxx
	@rm -f $(PYTHON3_BINDINGS_DIR)/quackle_wrap.o
	@rm -f $(PYTHON3_BINDINGS_DIR)/_quackle.so
	@rm -f $(PYTHON3_BINDINGS_DIR)/quackle.py
	@rm -rf $(PYTHON3_BINDINGS_DIR)/__pycache__
	@rm -rf $(WORDLIST_DIR)
	@echo "Clean complete"

# Download wordlists (existing targets)
download_wordlist: get_cs21 get_nwl_2023

get_cs21:
	@mkdir -p $(WORDLIST_DIR)
	@wget -O $(WORDLIST_DIR)/csw21.txt https://raw.githubusercontent.com/scrabblewords/scrabblewords/refs/heads/main/words/British/CSW21.txt

get_nwl_2023:
	@mkdir -p $(WORDLIST_DIR)
	@wget -O $(WORDLIST_DIR)/nwl_2023.txt https://raw.githubusercontent.com/scrabblewords/scrabblewords/refs/heads/main/words/North-American/NWL2023.txt
