# `make install` puts sd-put on your PATH (as a symlink in ~/bin, next to pi-install).
BIN ?= $(HOME)/bin

all:
	$(MAKE) -C pi

install: all
	mkdir -p $(BIN)
	ln -sf $(CURDIR)/sd-put $(BIN)/sd-put
	@echo "installed: $(BIN)/sd-put -> $(CURDIR)/sd-put"

clean:
	$(MAKE) -C pi clean
.PHONY: all install clean
