CC = clang
CFLAGS = -Wall -Wextra -Ilib/uds-c/src
LDFLAGS = -lm

SRC = src/main.c src/web_server.c src/uds_obd.c
OBJ = $(SRC:.c=.o)
TARGET = ecu-tuner

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(OBJ) $(TARGET)

run: $(TARGET)
	./$(TARGET)
