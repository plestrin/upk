#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <zlib.h>

#include "../dump.h"

#define ARCHIVE_ITEM_PYPACKAGE 		'M' /* Python package (__init__.py) */
#define ARCHIVE_ITEM_ZIPFILE 		'Z' /* zlib (pyz) - frozen Python code */
#define ARCHIVE_ITEM_BINARY 		'b' /* binary */
#define ARCHIVE_ITEM_DEPENDENCY 	'd' /* runtime option */
#define ARCHIVE_ITEM_PYMODULE 		'm' /* Python module */
#define ARCHIVE_ITEM_RUNTIME_OPTION 'o' /* runtime option */
#define ARCHIVE_ITEM_PYSOURCE 		's' /* Python script (v3) */
#define ARCHIVE_ITEM_DATA 			'x' /* data */
#define ARCHIVE_ITEM_PYZ 			'z' /* zlib (pyz) - frozen Python code */

struct toc{
	int32_t structlen; 	/* len of this one - including full len of name */
	int32_t pos; 		/* pos rel to start of concatenation */
	int32_t len; 		/* len of the data (compressed) */
	int32_t ulen; 		/* len of data (uncompressed) */
	char 	cflag; 		/* is it compressed (really a byte) */
	char 	typcd; 		/* type code -'b' binary, 'z' zlib, 'm' module, 's' script (v3),'x' data, 'o' runtime option */
	char 	name[1]; 	/* the name to save it as */
};

enum python_version{
	VERSION_36,
};

static int pyi_dump(size_t s, const struct toc* t, const void* buffer, size_t len, enum python_version version){
	size_t 		off = s + ntohl(t->pos);
	uint32_t 	siz = ntohl(t->len);
	void* 		out = NULL;
	const void* wrt;
	int 		rc = -1;
	int 		file;

	printf(" size %u, @ 0x%zx, name %s\n", siz, off, t->name);

	if (off >= len){
		fprintf(stderr, "[-] PYI: incorrect offset: 0x%zx is out of bound\n", off);
		return -1;
	}

	if (siz > len - off){
		fprintf(stderr, "[-] PYI: incorrect len: %u is out of bound\n", siz);
		return -1;
	}

	wrt = (const void*)((const char*)buffer + off);

	if (t->cflag){
		z_stream zstream;

		if ((out = malloc(ntohl(t->ulen))) == NULL){
			fprintf(stderr, "[-] PYI: unable to allocate memory\n");
			return -1;
		}

		zstream.zalloc 		= NULL;
		zstream.zfree 		= NULL;
		zstream.opaque 		= NULL;
		zstream.next_in 	= (unsigned char*)buffer + off;
		zstream.avail_in 	= siz;
		zstream.next_out 	= out;
		zstream.avail_out 	= ntohl(t->ulen);

		if (inflateInit(&zstream) == Z_OK){
			if (inflate(&zstream, Z_FINISH) != Z_STREAM_ERROR){
				wrt = out;
				siz = ntohl(t->ulen);
			}
			else{
				fprintf(stderr, "[-] PYI: unable to inflate: %s\n", zstream.msg);
			}
			inflateEnd(&zstream);
		}
		else{
			fprintf(stderr, "[-] PYI: unable to init zlib: %s\n", zstream.msg);
			goto exit;
		}
	}


	if ((file = dump_open("pyi", t->name)) < 0){
		fprintf(stderr, "[-] PYI: unable to open dump file\n");
		goto exit;
	}

	if (t->typcd == ARCHIVE_ITEM_PYSOURCE){
		char header[8] = {0x00, 0x00, '\r', '\n', 0x00, 0x00, 0x00, 0x00};

		switch (version){
			case VERSION_36 : {
				header[0] = 0x33;
				header[1] = 0x0d;
				break;
			}
		}

		write(file, header, sizeof header);
	}

	write(file, wrt, siz);

	close(file);

	rc = 0;

	exit:

	free(out);

	return rc;
}

static const char magic[8] = {'M', 'E', 'I', 0x0c, 0x0b, 0x0a, 0x0b, 0x0e};

struct cookie{
	char 		magic[8]; 		/* 'MEI\014\013\012\013\016' */
	uint32_t 	len; 			/* len of entire package */
	uint32_t 	toc_off; 		/* pos (rel to start) of TableOfContents */
	uint32_t 	toc_len; 		/* length of TableOfContents */
	uint32_t 	pyvers; 		/* new in v4 */
	char 		pylibname[64]; 	/* Filename of Python dynamic library e.g. python2.7.dll. */
};

int pyi_rtn(const void* buffer, size_t len){
	size_t 					i;
	size_t 					j;
	size_t 					s;
	const struct cookie* 	c;
	const struct toc* 		t;
	enum python_version 	ver;

	for (i = 0; i < len - sizeof(struct cookie) + 1; i++){
		if (!memcmp((const char*)buffer + i, magic, sizeof magic)){
			c = (const struct cookie*)((const char*)buffer + i);

			if (!ntohl(c->len)){
				continue;
			}

			if (ntohl(c->len) != ntohl(c->toc_off) + ntohl(c->toc_len) + sizeof(struct cookie)){
				continue;
			}

			s =  i + sizeof(struct cookie) - ntohl(c->len);

			printf("[+] PYI: found cookie at 0x%zx, archive of size %u at 0x%zx for %s\n", i, ntohl(c->len), s, c->pylibname);

			if (i + sizeof(struct cookie) < ntohl(c->len)){
				printf("[-] PYI: incorrect archive length\n");
				continue;
			}

			if (!strcmp(c->pylibname, "python36.dll")){
				ver = VERSION_36;
			}
			else{
				fprintf(stderr, "[-] PYI: unk python version %s -> skip\n", c->pylibname);
				continue;
			}

			for (j = 0; j < ntohl(c->toc_len); j += ntohl(t->structlen)){
				t = (const struct toc*)((const char*)buffer + s + ntohl(c->toc_off) + j);
				switch (t->typcd){
					case ARCHIVE_ITEM_PYPACKAGE : {
						printf("[+] PYI: package item");
						break;
					}
					case ARCHIVE_ITEM_ZIPFILE : {
						printf("[+] PYI: zipFile ");
						break;
					}
					case ARCHIVE_ITEM_BINARY : {
						printf("[+] PYI: binary");
						break;
					}
					case ARCHIVE_ITEM_DEPENDENCY : {
						printf("[+] PYI: dependency");
						break;
					}
					case ARCHIVE_ITEM_PYMODULE : {
						printf("[+] PYI: python module");
						break;
					}
					case ARCHIVE_ITEM_RUNTIME_OPTION : {
						printf("[+] PYI: runtime option");
						break;
					}
					case ARCHIVE_ITEM_PYSOURCE : {
						printf("[+] PYI: python script");
						break;
					}
					case ARCHIVE_ITEM_DATA : {
						printf("[+] PYI: data");
						break;
					}
					case ARCHIVE_ITEM_PYZ : {
						printf("[+] PYI: pyz");
						break;
					}
					default : {
						fprintf(stderr, "[-] PYI: unk TOC type: '%c'", t->typcd);
						break;
					}
				}
				pyi_dump(s, t, buffer, len, ver);
			}

			if (j != ntohl(c->toc_len)){
				fprintf(stderr, "[-] PYI: toc walk does not reached specified toc length %u\n", ntohl(c->toc_len));
			}
		}
	}

	return 0;
}
