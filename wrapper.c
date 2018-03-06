#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#include "dump.h"

int upk_rtn(const void* data, size_t len);

int main(int argc, char** argv){
	int 		i;
	int 		file;
	struct stat sb;
	void*		buffer;
	size_t 		len;
	int 		res;

	if (argc < 2){
		printf("Usage: %s [-o output] file [...]\n", argv[0]);
		return EXIT_FAILURE;
	}

	if (!strcmp(argv[1], "-o") && argc >= 4){
		dump_register_path(argv[2]);
		argv = argv + 2;
	}
	else{
		dump_register_path("out_");
	}

	for (i = 1; i < argc; i++){
		if ((file = open(argv[i], O_RDONLY)) == -1){
			fprintf(stderr, "[-] unable to open file %s read only", argv[i]);
			return EXIT_FAILURE;
		}

		if (fstat(file, &sb) < 0){
			fprintf(stderr, "[-] unable to read the size of file %s", argv[i]);
			close(file);
			return EXIT_FAILURE;
		}

		len = (size_t)sb.st_size;
		buffer = mmap(NULL, len, PROT_READ, MAP_PRIVATE, file, 0);
		close(file);
		if (buffer == MAP_FAILED){
			fprintf(stderr, "[-] unable to map file %s, (size=%zu)", argv[i], len);
			return EXIT_FAILURE;
		}

		res = upk_rtn(buffer, len);
		munmap(buffer, len);

		if (res){
			return EXIT_FAILURE;
		}
	}

	return EXIT_SUCCESS;
}
