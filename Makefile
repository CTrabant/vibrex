CFLAGS = -std=c11 -Wall -Wextra -O2 -g

LIB_TARGET = libvibrex.a
TEST_TARGET = test_vibrex
COMPARE_TARGET = compare_vibrex
CLI_TARGET = vibrex-cli

.PHONY: all clean test compare cli

all: $(LIB_TARGET) $(CLI_TARGET)

# Static library
$(LIB_TARGET): vibrex.o
	ar rcs libvibrex.a vibrex.o

$(TEST_TARGET): test_vibrex.c $(LIB_TARGET) vibrex.h
	$(CC) $(CFLAGS) -o $(TEST_TARGET) test_vibrex.c $(LIB_TARGET)

test: $(TEST_TARGET)
	./$(TEST_TARGET)

$(CLI_TARGET): vibrex-cli.c $(LIB_TARGET) vibrex.h
	$(CC) $(CFLAGS) -o $(CLI_TARGET) vibrex-cli.c $(LIB_TARGET)

cli: $(CLI_TARGET)

# Individual object files for library usage
vibrex.o: vibrex.c vibrex.h
	$(CC) $(CFLAGS) -c vibrex.c

compare: compare_vibrex.c $(LIB_TARGET) vibrex.h
	$(CC) $(CFLAGS) -o $(COMPARE_TARGET) compare_vibrex.c $(LIB_TARGET)

clean:
	rm -rf $(TEST_TARGET) $(COMPARE_TARGET) $(LIB_TARGET) $(CLI_TARGET) vibrex.o test_vibrex.o compare_vibrex.o vibrex-cli.o *.dSYM
