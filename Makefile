CFLAGS = -std=c11 -Wall -Wextra -O2 -g

LIB_TARGET = libvibrex.a
TEST_TARGET = vibrex-test
COMPARE_TARGET = vibrex-compare
CLI_TARGET = vibrex-cli

.PHONY: all clean test compare cli

all: $(LIB_TARGET) $(CLI_TARGET)

# Static library
$(LIB_TARGET): vibrex.o
	ar rcs libvibrex.a vibrex.o

$(TEST_TARGET): vibrex-test.c $(LIB_TARGET) vibrex.h
	$(CC) $(CFLAGS) -o $(TEST_TARGET) vibrex-test.c $(LIB_TARGET)

test: $(TEST_TARGET)
	./$(TEST_TARGET)

$(CLI_TARGET): vibrex-cli.c $(LIB_TARGET) vibrex.h
	$(CC) $(CFLAGS) -o $(CLI_TARGET) vibrex-cli.c $(LIB_TARGET)

cli: $(CLI_TARGET)

# Individual object files for library usage
vibrex.o: vibrex.c vibrex.h
	$(CC) $(CFLAGS) -c vibrex.c

compare: vibrex-compare.c $(LIB_TARGET) vibrex.h
	$(CC) $(CFLAGS) -o $(COMPARE_TARGET) vibrex-compare.c $(LIB_TARGET)

clean:
	rm -rf $(TEST_TARGET) $(COMPARE_TARGET) $(LIB_TARGET) $(CLI_TARGET) vibrex.o vibrex-test.o vibrex-compare.o vibrex-cli.o *.dSYM
