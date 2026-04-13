# gpkg Makefile - Follows EE Ethos (Minimalist & Portable)

CC = cc
CFLAGS = -std=c2x -Wall -Wextra -Wpedantic -Iinclude \
         -DPKG_SCRIPTS_PATH="\"scripts\"" \
         -DPKG_DB_PATH="\"db\""
TARGET = gpkg
SRC = src/main.c
OBJ = $(SRC:.c=.o)

.PHONY: all clean install

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ)

clean:
	rm -f $(TARGET) $(OBJ)

install: $(TARGET)
	mkdir -p $(DESTDIR)/usr/local/bin
	mkdir -p $(DESTDIR)/usr/local/lib/gpkg/scripts
	mkdir -p $(DESTDIR)/var/db/gpkg/local/installed
	install -m 755 $(TARGET) $(DESTDIR)/usr/local/bin/$(TARGET)
