CC      = cc
CFLAGS  = -Wall -Wextra -Werror -std=c11 -g \
          -Wno-unused-variable -Wno-unused-function -Wno-unused-parameter \
          -Imocks \
          -I../components/hx711/include \
          -I../components/nextion/include \
          -I../components/weight/include

MOCK    = mocks/esp_mock.c

all: test

test: test_weight test_nextion test_hx711
	@echo ""
	@echo "=== Running all tests ==="
	@./test_weight && ./test_nextion && ./test_hx711

test_weight: test_weight.c ../components/weight/weight.c $(MOCK)
	$(CC) $(CFLAGS) -o $@ $^ -lm

test_nextion: test_nextion.c ../components/nextion/nextion.c $(MOCK)
	$(CC) $(CFLAGS) -o $@ $^ -lm

test_hx711: test_hx711.c ../components/hx711/hx711.c $(MOCK)
	$(CC) $(CFLAGS) -o $@ $^ -lm

clean:
	rm -f test_weight test_nextion test_hx711

.PHONY: all test clean
