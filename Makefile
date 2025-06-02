all:
	@echo "clog does not need to be built, just copy clog.h into your project."
	@echo ""
	@echo "To run tests, run 'make check'."

lua:
	$(CC) -shared -o log.so -fPIC -Wall lua/log.c $(shell pkg-config --cflags --libs luajit)
	luajit test/test-log.lua

check:
	@$(MAKE) -w -C test check

clean:
	@rm -rf *.so *.log *.log.old
	@$(MAKE) -w -C test clean

.PHONY: lua clean check
