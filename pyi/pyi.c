#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <zlib.h>

 #include "../dump.h"

struct toc{
	int32_t structlen; 	/* len of this one - including full len of name */
	int32_t pos; 		/* pos rel to start of concatenation */
	int32_t len; 		/* len of the data (compressed) */
	int32_t ulen; 		/* len of data (uncompressed) */
	char 	cflag; 		/* is it compressed (really a byte) */
	char 	typcd; 		/* type code -'b' binary, 'z' zlib, 'm' module, 's' script (v3),'x' data, 'o' runtime option */
	char 	name[1]; 	/* the name to save it as */
};

static int pyi_write(const char* buffer, size_t size, const char* file_name){
	int file;

	if ((file = dump_open("pyi", file_name)) < 0){
		fprintf(stderr, "[-] PYI: unable to open dump file\n");
		return -1;
	}

	write(file, buffer, size);

	close(file);

	return 0;
}

static int pyi_dump(size_t s, const struct toc* t, const void* buffer, size_t len){
	size_t 		off = s + ntohl(t->pos);
	uint32_t 	siz = ntohl(t->len);

	printf(" size %u, @ 0x%zx, name %s\n", siz, off, t->name);

	if (off >= len){
		fprintf(stderr, "[-] PYI: incorrect offset: 0x%zx is out of bound\n", off);
		return -1;
	}

	if (siz > len - off){
		fprintf(stderr, "[-] PYI: incorrect len: %u is out of bound\n", siz);
		return -1;
	}

	if (t->cflag){
		void* 		out;
		z_stream 	zstream;
		int 		rc = -1;

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
				rc = pyi_write(out, ntohl(t->ulen), t->name);
			}
			else{
				fprintf(stderr, "[-] PYI: unable to inflate: %s\n", zstream.msg);
			}
			inflateEnd(&zstream);
		}
		else{
			fprintf(stderr, "[-] PYI: unable to init zlib: %s\n", zstream.msg);
		}

		free(out);

		return rc;
	}

	return pyi_write((const char*)buffer + off, siz, t->name);
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

			for (j = 0; j < ntohl(c->toc_len); j += ntohl(t->structlen)){
				t = (const struct toc*)((const char*)buffer + s + ntohl(c->toc_off) + j);
				switch (t->typcd){
					case 'M' : {
						printf("[+] PYI: package item");
						break;
					}
					case 'Z' : {
						printf("[+] PYI: zipFile ");
						break;
					}
					case 'b' : {
						printf("[+] PYI: binary");
						break;
					}
					case 'd' : {
						printf("[+] PYI: dependency");
						break;
					}
					case 'm' : {
						printf("[+] PYI: python module");
						break;
					}
					case 's' : {
						printf("[+] PYI: python script");
						break;
					}
					case 'z' : {
						printf("[+] PYI: pyz");
						break;
					}
					default : {
						fprintf(stderr, "[-] PYI: unk TOC type: '%c'\n", t->typcd);
						break;
					}
				}
				pyi_dump(s, t, buffer, len);
			}

			if (j != ntohl(c->toc_len)){
				fprintf(stderr, "[-] PYI: toc walk does not reached specified toc length %u\n", ntohl(c->toc_len));
			}
		}
	}

	return 0;
}
