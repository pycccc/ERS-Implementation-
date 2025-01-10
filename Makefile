CC = gcc

CFLAGS = -Wall 

TARGET = prog

SRCS = main.c



all: $(TARGET)



$(TARGET): $(SRCS) 

	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS)


clean:

	rm -f $(TARGET)


