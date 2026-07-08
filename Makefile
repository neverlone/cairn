CC     ?= cc
CFLAGS ?= -O2 -Wall -Wextra

cairn: cairn.c
	$(CC) $(CFLAGS) -o $@ cairn.c

.PHONY: test demo clean
test: cairn
	./test.sh

demo: cairn
	@for f in examples/*.cairn; do \
	  echo "== $$f =="; ./cairn $$f; echo; \
	done

clean:
	rm -f cairn
