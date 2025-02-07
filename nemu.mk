#Update memory analysis mode NEMU
NEMU_REPO_URL = https://github.com/OpenXiangShan/NEMU

NEMU_DIR = $(abspath .)/NEMU

NEMU_BINARY = build/riscv64-nemu-interpreter

nemu-update:
	@if [ ! -d "$(NEMU_DIR)" ]; then \
		echo "Cloning NEMU repository..." && \
		git clone $(NEMU_REPO_URL) $(NEMU_DIR); \
	fi
	@echo "Building NEMU..."
	cd $(NEMU_DIR) && export NEMU_HOME=$(NEMU_DIR) && \
	make riscv64-xs_defconfig && \
	echo "CONFIG_MEMORY_REGION_ANALYSIS=y" >> $(NEMU_DIR)/.config  && \
	make -j8
	@echo "Copying generated binary to current repository..."
	cp $(NEMU_DIR)/$(NEMU_BINARY) ./ready-to-run/
	@echo "Update completed."

nemu-clean:
	@echo "Clean NEMU..."
	rm -rf $(NEMU_DIR)