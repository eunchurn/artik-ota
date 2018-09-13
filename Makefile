CFLAGS = -Wall -O2
TARGET = artik-updater

OBJS_V1 = \
	src/artik-ota-v1/artik-updater.o

all: $(TARGET)

$(TARGET): $(OBJS_V1)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	$(RM) $(TARGET) $(OBJS_V1)

.PHONY: all v1 clean
