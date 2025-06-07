CFLAGS = -std=c11 -Wall -Wextra -O2 -g

LIB_TARGET = libvibrex.a
TEST_TARGET = test_vibrex
DEMO_TARGET = demo_vibrex

.PHONY: all clean test demo

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

demo: vibrex.c demo_vibrex.c vibrex.h
	$(CC) $(CFLAGS) -o $(DEMO_TARGET) vibrex.c demo_vibrex.c

clean:
	rm -f $(TEST_TARGET) $(DEMO_TARGET) $(LIB_TARGET) vibrex.o demo_vibrex.o test_vibrex.o
