CFLAGS=-W -Wall
SRC=artik_updater
TARGET=artik-updater

all : $(TARGET)
$(TARGET) : $(SRC)/$(TARGET).o
	$(CC) $(CFLAGS) -o $@ $^
