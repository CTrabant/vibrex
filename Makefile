CFLAGS = -std=c11 -Wall -Wextra -O2 -g

LIB_TARGET = libvibrex.a
TEST_TARGET = test_vibrex
COMPARE_TARGET = compare_vibrex

.PHONY: all clean test compare

all: $(LIB_TARGET)

# Static library
$(LIB_TARGET): vibrex.o
	ar rcs libvibrex.a vibrex.o

$(TEST_TARGET): test_vibrex.c $(LIB_TARGET) vibrex.h
	$(CC) $(CFLAGS) -o $(TEST_TARGET) test_vibrex.c $(LIB_TARGET)

test: $(TEST_TARGET)
	./$(TEST_TARGET)

# Individual object files for library usage
vibrex.o: vibrex.c vibrex.h
	$(CC) $(CFLAGS) -c vibrex.c

compare: compare_vibrex.c $(LIB_TARGET) vibrex.h
	$(CC) $(CFLAGS) -o $(COMPARE_TARGET) compare_vibrex.c $(LIB_TARGET)

clean:
	rm -rf $(TEST_TARGET) $(COMPARE_TARGET) $(LIB_TARGET) vibrex.o test_vibrex.o compare_vibrex.o *.dSYM
