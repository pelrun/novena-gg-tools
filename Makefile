CC=gcc
CFLAGS=-O2
OBJ_TEST=test.o gg.o
OBJ_DUMP=dump.o gg.o
OBJ_FLASH=flash.o gg.o

all: test dump flash

test: $(OBJ_TEST)
	$(CC) $(CFLAGS) -o $@ $(OBJ_TEST)

dump: $(OBJ_DUMP)
	$(CC) $(CFLAGS) -o $@ $(OBJ_DUMP)

flash: $(OBJ_FLASH)
	$(CC) $(CFLAGS) -o $@ $(OBJ_FLASH)

%.o: %.c
	$(CC) $(CFLAGS) -c $< 
