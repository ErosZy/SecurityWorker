/*
 * BSD 2-clause license
 * Copyright (c) 2013 Wojciech A. Koszek <wkoszek@FreeBSD.org>
 * 
 * Based on:
 * 
 * https://github.com/strake/gzip.git
 *
 * I had to rewrite it, since strake's version was powered by UNIX FILE* API,
 * while the key objective was to perform memory-to-memory operations
 */

#include <sys/stat.h>
#include <stdint.h>
#include <unistd.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>

#include "miniz.h"
#include "mini_gzip.h"

int
main(int argc, char **argv)
{
	struct mini_gzip gz;
	struct stat st;
	char	in_fn[MAX_PATH_LEN], out_fn[MAX_PATH_LEN];
	char	*sptr, *mem_in, *mem_out;
	int	level, flag_c, o, in_fd, out_fd, ret, is_gzipped, out_len;

	level = 6;
	flag_c = 0;
	while ((o = getopt(argc, argv, "cd123456789")) != -1) {
		switch (o) {
		case 'd':
			level = 0;
			break;
		case 'c':
			flag_c = 1;
			break;
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			assert(o != '0');
			level = o - '0';
			break;
		default:
			abort();
			break;
		}
	}
	
	argc -= optind;
	argv += optind;

	GZAS(argc == 2, "2 file names must be passed: input and output file");

	is_gzipped = 0;
	sptr = strstr(argv[0], ".gz");
	if (sptr != NULL) {
		is_gzipped = 1;
	}
	printf("flag_c: %d is_gzipped: %d\n", flag_c, is_gzipped);

	if (is_gzipped) {
		GZAS(flag_c == 0, "Requesting to compress .gz file? Looks wrong");
	} else {
		GZAS(flag_c == 1, "Requesting to decompress normal file?");
	}

	snprintf(in_fn, sizeof(in_fn) - 1, "%s", argv[0]);
	snprintf(out_fn, sizeof(out_fn) - 1, "%s", argv[1]);

	printf("in_fn: %s out_fn: %s level %d\n", in_fn, out_fn, level);

	mem_in = calloc(1024*1024, 1);
	mem_out = calloc(1024*1024, 1);
	GZAS(mem_in != NULL, "Couldn't allocate memory for input file");
	GZAS(mem_out != NULL, "Couldn't allocate memory for output file");

	in_fd = open(in_fn, O_RDONLY);
	GZAS(in_fd != -1, "Couldn't open file %s for reading", in_fn);
	ret = fstat(in_fd, &st);
	GZAS(ret == 0, "Couldn't call fstat(), %d returned", ret);
	ret = read(in_fd, mem_in, st.st_size);
	GZAS(ret == st.st_size, "Read only %d bytes, %jd expected", ret,
							(uintmax_t)st.st_size);
	out_fd = open(out_fn, O_WRONLY|O_CREAT, st.st_mode);
	GZAS(out_fd != -1, "Couldn't create output file '%s' for writing",
								out_fn);

	if (flag_c) {
		abort();
#if 0
		mini_gz_init(&gz);
		printf("COMP\n");
		out_len = mini_gz_pack(&gz, level, mem_in, st.st_size, mem_out,
								1024*1024);
		printf("out_len = %d\n", out_len);
		ret = write(out_fd, mem_out, out_len);
		printf("ret = %d\n", ret);
		GZAS(ret == out_len, "Wrote only %d bytes, expected %d", ret, out_len);
#endif
	} else {
		printf("--- testing decompression --\n");
		ret = mini_gz_start(&gz, mem_in, st.st_size);
		GZAS(ret == 0, "mini_gz_start() failed, ret=%d", ret);
		out_len = mini_gz_unpack(&gz, mem_out, 1024*1024);
		printf("out_len = %d\n", out_len);
		ret = write(out_fd, mem_out, out_len);
		printf("ret = %d\n", ret);
		GZAS(ret == out_len, "Wrote only %d bytes, expected %d", ret, out_len);
	}
	close(in_fd);
	close(out_fd);

	return 0;
}
