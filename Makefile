CFLAGS=-Wall
SRC=artik_updater
TARGET=artik-updater

all : $(TARGET)
$(TARGET) : $(SRC)/$(TARGET).c
	$(CC) $(CFLAGS) -o $@ $^

clean :
	$(RM) $(TARGET)
