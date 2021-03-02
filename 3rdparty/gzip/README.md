mini_gzip - embeddable, minimal, in-memory GZIP API
===================================================

[![Build Status](https://travis-ci.org/wkoszek/mini_gzip.svg?branch=master)](https://travis-ci.org/wkoszek/mini_gzip)

Embeddable, minimal GZIP functionality, with API designed to work from
memory. Only supports decompression for now.

I wrote it when I needed a small piece of code to give me decompression in
an embedded system I was working with, and I found out that most of the
GZIP-related programs use POSIX `FILE` API, which made the examples not work
for my case.

The `mini_gzip` is based on `miniz` library
(https://code.google.com/p/miniz/), which provides an API for operating on
data compressed according to deflate algorithm. Added is a layer which
provides a container for the GZIP files and let me do some verification.

# To build

Everything should be fairly self-contained, and built with:

	make

# How to use

The API operates on `struct mini_gz` structures, which are containers for
the memory with GZIPed data. You must call `mini_gz_init()` on the structure
1st for the rest of the API to work correctly. Next, you must call
`mini_gz_start(&gz, mem_in, size)`, where `gz` is the initialized container
and with GZIPed data pointed by `mem_in` of the size of `size`.  To unpack,
you call `mini_gz_unpack(&gz, mem_out, memsize)`, where the first argument
is the initialized container, the second `mem_out` is a pointer to the
output memory, and `memsize` is its lenght. It's important to have enough
bytes in the output buffer for decompressed data.

# To test

Simple test is provided:

	make test

To test by yourself, you can do:

~~~shell
	wk:/w/repos/mini_gzip> ls -la miniz.o
	-rw-r--r--  1 wk  staff  114348 20 paź 09:46 miniz.o
	wk:/w/repos/mini_gzip> md5 miniz.o
	MD5 (miniz.o) = e6199aade2020b6040fa160baee47d68
	wk:/w/repos/mini_gzip> gzip miniz.o
	wk:/w/repos/mini_gzip> ls -la miniz.o.gz
	-rw-r--r--  1 wk  staff  44965 20 paź 09:46 miniz.o.gz
	wk:/w/repos/mini_gzip> ./mini_gzip miniz.o.gz miniz.o
	flag_c: 0 is_gzipped: 1
	in_fn: miniz.o.gz out_fn: miniz.o level 6
	--- testing decompression --
	out_len = 114348
	ret = 114348
	wk:/w/repos/mini_gzip> md5 miniz.o
	MD5 (miniz.o) = e6199aade2020b6040fa160baee47d68
~~~

# References

After publishing `mini_gzip` on HackerNews, byuu (http://byuu.org/) mentioned he has much leaner implementation of GZIP:

- https://gitlab.com/higan/higan/blob/master/nall/decode/gzip.hpp
- https://gitlab.com/higan/higan/blob/master/nall/decode/inflate.hpp

# Author

- Wojciech Adam Koszek, [wojciech@koszek.com](mailto:wojciech@koszek.com)
- [http://www.koszek.com](http://www.koszek.com)
