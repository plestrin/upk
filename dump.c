#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <linux/limits.h>
#include <fcntl.h>
#include <unistd.h>

#include "dump.h"

static char base_path[PATH_MAX];

void dump_register_path(const char* path){
	strncpy(base_path, path, sizeof base_path);
	base_path[sizeof base_path - 1] = 0;
}

int dump_open(const char* mod, const char* file_name){
	char 	file_path[PATH_MAX];
	size_t 	len;

	strncpy(file_path, base_path, sizeof file_path);
	file_path[sizeof file_path - 1] = 0;

	if (access(file_path, W_OK)){
		if (mkdir(file_path, S_IRUSR | S_IWUSR)){
			fprintf(stderr, "[-] unable to create directory: \"%s\"\n", file_path);
			return -1;
		}
	}

	len = strlen(file_path);

	if (file_path[len - 1] == '/'){
		strncpy(file_path + len, mod, sizeof file_path - len);
	}
	else if (len < sizeof file_path - 1){
		file_path[len ++] = '/';
		strncpy(file_path + len, mod, sizeof file_path - len);
	}
	file_path[sizeof file_path - 1] = 0;


	if (access(file_path, W_OK)){
		if (mkdir(file_path, S_IRUSR | S_IWUSR)){
			fprintf(stderr, "[-] unable to create directory: \"%s\"\n", file_path);
			return -1;
		}
	}

	len = strlen(file_path);

	if (file_path[len - 1] == '/'){
		strncpy(file_path + len, file_name, sizeof file_path - len);
	}
	else if (len < sizeof file_path - 1){
		file_path[len ++] = '/';
		strncpy(file_path + len, file_name, sizeof file_path - len);
	}
	file_path[sizeof file_path - 1] = 0;

	return open(file_path, O_WRONLY | O_CREAT);
}
