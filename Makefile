CC = gcc
CFLAGS = -Wall -Wextra -fPIC -shared $(shell pkg-config --cflags evolution-shell-3.0 evolution-data-server-1.2 libebook-contacts-1.2 glib-2.0 gtk+-3.0 json-glib-1.0)
LIBS = $(shell pkg-config --libs evolution-shell-3.0 evolution-data-server-1.2 libebook-contacts-1.2 glib-2.0 gtk+-3.0 json-glib-1.0) -lcurl

PLUGIN_NAME = module-llm-assistant
PLUGIN_FILE = $(PLUGIN_NAME).so

SRCDIR = src
CONFIGDIR = config
SOURCES = $(SRCDIR)/evolution-llm-extension.c $(SRCDIR)/llm_client.c $(SRCDIR)/llm-preferences-dialog.c $(CONFIGDIR)/config.c
HEADERS = $(SRCDIR)/evolution-llm-extension.h $(SRCDIR)/llm_client.h $(SRCDIR)/llm-preferences-dialog.h $(CONFIGDIR)/config.h

PLUGIN_DIR = /usr/lib/evolution/modules
INSTALL_DIR = $(DESTDIR)$(PLUGIN_DIR)

.PHONY: all clean install install-user uninstall uninstall-user check-deps

all: check-deps $(PLUGIN_FILE)

$(PLUGIN_FILE): $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) $(SOURCES) $(LIBS) -o $(PLUGIN_FILE)

check-deps:
	@echo "Checking dependencies..."
	@pkg-config --exists evolution-shell-3.0 || (echo "Error: evolution development files not found. Install evolution-dev or evolution-devel package." && exit 1)
	@pkg-config --exists evolution-data-server-1.2 || (echo "Error: evolution-data-server development files not found." && exit 1)
	@pkg-config --exists libebook-contacts-1.2 || (echo "Error: libebook-contacts development files not found." && exit 1)
	@pkg-config --exists glib-2.0 || (echo "Error: glib development files not found." && exit 1)
	@pkg-config --exists gtk+-3.0 || (echo "Error: gtk3 development files not found." && exit 1)
	@pkg-config --exists json-glib-1.0 || (echo "Error: json-glib development files not found." && exit 1)
	@which curl-config >/dev/null || (echo "Error: libcurl development files not found." && exit 1)
	@echo "All dependencies found."

install: $(PLUGIN_FILE)
	@echo "Installing plugin to $(INSTALL_DIR)"
	@mkdir -p $(INSTALL_DIR)
	@cp $(PLUGIN_FILE) $(INSTALL_DIR)/
	@echo "Plugin installed successfully."
	@echo "Restart Evolution to load the plugin."
	@echo ""
	@echo "Configuration file will be created at:"
	@echo "~/.config/evolution-llm-assistant/config.conf"
	@echo ""
	@echo "Please edit this file with your OpenAI API key before using the plugin."

install-user: $(PLUGIN_FILE)
	@echo "Installing plugin to ~/.local/share/evolution/modules/"
	@mkdir -p ~/.local/share/evolution/modules
	@cp $(PLUGIN_FILE) ~/.local/share/evolution/modules/
	@echo "Plugin installed successfully for current user."
	@echo "Restart Evolution to load the plugin."
	@echo ""
	@echo "Configuration file will be created at:"
	@echo "~/.config/evolution-llm-assistant/config.conf"
	@echo ""
	@echo "Please edit this file with your OpenAI API key before using the plugin."

uninstall:
	@echo "Removing plugin from $(INSTALL_DIR)"
	@rm -f $(INSTALL_DIR)/$(PLUGIN_FILE)
	@echo "Plugin uninstalled. Restart Evolution to complete removal."

uninstall-user:
	@echo "Removing plugin from ~/.local/share/evolution/modules/"
	@rm -f ~/.local/share/evolution/modules/$(PLUGIN_FILE)
	@echo "Plugin uninstalled for current user. Restart Evolution to complete removal."

clean:
	rm -f $(PLUGIN_FILE)

help:
	@echo "Evolution LLM Assistant Plugin Build System"
	@echo ""
	@echo "Available targets:"
	@echo "  all           - Build the plugin (default)"
	@echo "  install       - Install plugin system-wide (requires sudo)"
	@echo "  install-user  - Install plugin for current user only"
	@echo "  uninstall     - Remove plugin system-wide (requires sudo)"
	@echo "  uninstall-user- Remove plugin for current user"
	@echo "  clean         - Remove built files"
	@echo "  check-deps    - Check for required dependencies"
	@echo "  help          - Show this help message"
	@echo ""
	@echo "Usage (without sudo):"
	@echo "  make && make install-user"
	@echo ""
	@echo "Usage (system-wide):"
	@echo "  make && sudo make install"
	@echo ""
	@echo "Required packages (Ubuntu/Debian):"
	@echo "  evolution-dev evolution-data-server-dev libebook1.2-dev"
	@echo "  libglib2.0-dev libgtk-3-dev libjson-glib-dev libcurl4-openssl-dev"