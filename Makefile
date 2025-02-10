CC = gcc
TARGET = main_program
SRCS = main.c my_lib.c


all:
	$(CC) $(SRCS) -o $(TARGET)


clean:
	rm -f $(TARGET)
