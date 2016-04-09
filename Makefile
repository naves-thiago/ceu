all:
	./ceu $(CEUFILE)
	perl -p -i -e 's/line ([0-9]+)\.0/line $$1/g' _ceu_app.c
	gcc main.c $(CFLAGS)

clean:
	rm -f *.exe _ceu_* a.out

.PHONY: all clean
