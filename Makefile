CFLAGS = -Wall -O2
TARGET = artik-updater

OBJS_V1 = \
	src/artik-ota-v1/artik-updater.o
OBJS_V2 = \
	src/artik-ota-v2/artik-updater.o \
	src/artik-ota-v2/ota-common.o \
	src/artik-ota-v2/crc32.o


all: $(TARGET)

artik-ota-v1: $(OBJS_V1)
	$(CC) $(CFLAGS) -o $(TARGET) $^

$(TARGET): $(OBJS_V2)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	$(RM) $(TARGET) $(OBJS_V1) $(OBJS_V2)

.PHONY: all clean
