SRCS+=		mini_gzip.c miniz.c
CFLAGS=		-D_POSIX_C_SOURCE=200112L -D_BSD_SOURCE
CFLAGS+=	-g -std=c99 -pedantic
CFLAGS+=	-DTEST_PROG
CFLAGS+=	-DMINI_GZ_DEBUG

all: mini_gzip

mini_gzip: mini_gzip.c mini_gzip_testprog.c miniz.c makefile
	gcc $(CFLAGS) -Wall -c mini_gzip.c
	gcc $(CFLAGS) -Wall -c mini_gzip_testprog.c
	gcc $(CFLAGS) -Wall -c miniz.c
	gcc $(CFLAGS) -Wall mini_gzip.o mini_gzip_testprog.o miniz.o -o mini_gzip

clean:
	rm -rf mini_gzip mini_gzip.d* *.o
	rm -rf data*

test: mini_gzip

	cp mini_gzip data
	#cp /etc/services data
	chmod 644 data
	
	@echo "# ------------------ Compress with gzip(1), uncompress with mini_gzip"
	@ls -la data
	gzip -c data > data.gz
	./mini_gzip -d data.gz data.o
	@ls -la data.o
	diff data.o data
	
notyet:
	@echo "# ------------------ Compress with mini_gzip and gzip and compare"
	gzip -9c data > data_sys_gzip.gz
	@ls -la data_sys_gzip.gz

	./mini_gzip -9c data data_mini_gzip.gz
	@ls -la data_mini_gzip.gz

	gunzip -9c data_mini_gzip.gz > data_mini_gzip_unpacked
	diff data_mini_gzip_unpacked data
