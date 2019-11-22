#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "../dump.h"

static uint32_t buffer_register;

#define IK_SIZE 5

static size_t match_IK(uint8_t* buffer, size_t len){
	if (len < IK_SIZE){
		return 0;
	}
	if ((buffer[0] & 0xf) >= 0x1 && (buffer[0] >> 4) == 0xa){
		return IK_SIZE;
	}
	if ((buffer[0] & 0xf) >= 0x8 && (buffer[0] >> 4) == 0xb){
		return IK_SIZE;
	}

	return 0;
}

static uint32_t get_IK(uint8_t* buffer){
	return *(uint32_t*)(buffer + 1);
}

#define FP_SIZE 2

static size_t match_FP(uint8_t* buffer, size_t len){
	if (len < FP_SIZE){
		return 0;
	}
	if (buffer[0] == 0xd9 && buffer[1] == 0xd0){
		return FP_SIZE;
	}
	if (buffer[0] == 0xd9 && buffer[1] == 0xe1){
		return FP_SIZE;
	}
	if (buffer[0] == 0xd9 && buffer[1] == 0xf6){
		return FP_SIZE;
	}
	if (buffer[0] == 0xd9 && buffer[1] == 0xf7){
		return FP_SIZE;
	}
	if (buffer[0] == 0xd9 && buffer[1] == 0xe5){
		return FP_SIZE;
	}
	if (buffer[0] == 0xd9 && buffer[1] >= 0xe8 && buffer[1] <= 0xee){
		return FP_SIZE;
	}
	if (buffer[0] == 0xd9 && buffer[1] >= 0xc0 && buffer[1] <= 0xcf){
		return FP_SIZE;
	}
	if (buffer[0] == 0xda && buffer[1] >= 0xc0 && buffer[1] <= 0xdf){
		return FP_SIZE;
	}
	if (buffer[0] == 0xdb && buffer[1] >= 0xc0 && buffer[1] <= 0xdf){
		return FP_SIZE;
	}
	if (buffer[0] == 0xdd && buffer[1] >= 0xc0 && buffer[1] <= 0xc7){
		return FP_SIZE;
	}

	return 0;
}

#define FN_SIZE 4

static size_t match_FN(uint8_t* buffer, size_t len){
	if (len >= 4 && *(uint32_t*)buffer == 0xf42474d9){
		return FN_SIZE;
	}

	return 0;
}

static size_t match_GP(uint8_t* buffer, size_t len){
	/* POP */
	if ((buffer[0] & 0xf) >= 0x8 && (buffer[0] >> 4) == 0x5){
		buffer_register = 0;
		return 1;
	}

	/* LEA */
	if (len >= 2 && buffer[0] == 0x8d && buffer[1] < 0x40){
		buffer_register = 1;
		return 2;
	}
	if (len >= 3 && buffer[0] == 0x8d && buffer[1] < 0x80){
		buffer_register = 1;
		return 3;
	}
	if (len >= 6 && buffer[0] == 0x8d && buffer[1] < 0xc0){
		buffer_register = 1;
		return 6;
	}

	/* MOV */
	if (len >= 2 && buffer[0] == 0x89 && buffer[1] >= 0xc0){
		buffer_register = 1;

		/* ADD & SUB */
		if (len >= 2 + 3 && buffer[2] == 0x83 && buffer[3] >= 0xc0){
			return 5;
		}
		if (len >= 2 + 6 && buffer[2] == 0x81 && buffer[3] >= 0xc0){
			return 8;
		}

		/* INC & DEC */
		if (len >= 2 + 1 && buffer[2] >= 0x40 && buffer[2] < 0x50){
			if (len >= 2 + 2 && buffer[3] >= 0x40 && buffer[3] < 0x50){
				if (len >= 2 + 3 && buffer[4] >= 0x40 && buffer[4] < 0x50){
					return 5;
				}

				return 4;
			}

			return 3;
		}

		return 2;
	}

	return 0;
}

#define CL_SIZE 2

static size_t match_CL(uint8_t* buffer, size_t len){
	if (len < CL_SIZE){
		return 0;
	}

	if (buffer[0] == 0x31 && buffer[1] == 0xc9){
		return CL_SIZE;
	}
	if (buffer[0] == 0x29 && buffer[1] == 0xc9){
		return CL_SIZE;
	}
	if (buffer[0] == 0x33 && buffer[1] == 0xc9){
		return CL_SIZE;
	}
	if (buffer[0] == 0x2b && buffer[1] == 0xc9){
		return CL_SIZE;
	}

	return 0;
}

static size_t match_IC(uint8_t* buffer, size_t len){
	if (len >= 2 && buffer[0] == 0xb1){
		return 2;
	}
	if (len >= 4 && buffer[0] == 0x66 && buffer[1] == 0xb9){
		return 4;
	}
	if (len >= 5 && buffer[0] == 0xb9){
		return 5;
	}

	return 0;
}

static size_t get_size(uint8_t* buffer){
	if (buffer[0] == 0xb1){
		return (size_t)buffer[1];
	}
	if (buffer[0] == 0xb9){
		return (size_t)*(uint32_t*)(buffer + 1);
	}

	return (size_t)*(uint16_t*)(buffer + 2);
}

enum sched {
	IK_FP_FN_GP_CL_IC = 0,
	FP_IK_FN_GP_CL_IC = 1,
	FP_FN_IK_GP_CL_IC = 2,
	FP_FN_GP_IK_CL_IC = 3,
	FP_FN_GP_CL_IK_IC = 4,
	FP_FN_GP_CL_IC_IK = 5
};

static size_t (*const match_table[6][6])(uint8_t*, size_t) = {
	{match_IK, match_FP, match_FN, match_GP, match_CL, match_IC},
	{match_FP, match_IK, match_FN, match_GP, match_CL, match_IC},
	{match_FP, match_FN, match_IK, match_GP, match_CL, match_IC},
	{match_FP, match_FN, match_GP, match_IK, match_CL, match_IC},
	{match_FP, match_FN, match_GP, match_CL, match_IK, match_IC},
	{match_FP, match_FN, match_GP, match_CL, match_IC, match_IK}
};

static uint32_t* decode(uint32_t* buffer, uint32_t key, size_t size){
	uint32_t* 	result;
	size_t 		i;

	if ((result = malloc(size * 4)) == NULL){
		fprintf(stderr, "[-] SGN: unable to allocate memory\n");
		return NULL;
	}

	for (i = 0; i < size; i++){
		result[i] = buffer[i] ^ key;
		key += result[i];
	}

	return result;
}

#define BODY_SIZE 9
#define LOOP_SIZE (BODY_SIZE + 2)
#define LOOP_OPCODE 0xf5e2
#define MIN_ENCRYPTED (LOOP_SIZE + 1)

int sgn_rtn(const void* buffer, size_t len){
	size_t 		i;
	uint32_t 	j;
	uint32_t 	k;
	size_t 		l;
	size_t 		s[6];

	for (i = 0; i < len - LOOP_SIZE; i++){
		for (j = 0; j < 6; j++){
			for (k = 0, l = i; k < 6; l = s[k++]){
				s[k] = l + match_table[j][k]((uint8_t*)buffer + l, len - l);
				if (s[k] == l){
					break;
				}
			}
			if (k == 6 && l + LOOP_SIZE < len){
				uint32_t 	key = 0;
				size_t 		off = 0;
				size_t 		siz = 0;
				uint32_t* 	d;
				uint32_t 	c;
				int 		file;
				uint8_t* 	clone;
				uint32_t 	first;
				uint32_t 	disp;

				switch ((enum sched)j){
					case IK_FP_FN_GP_CL_IC : {
						key = get_IK((uint8_t*)buffer + i);
						siz = get_size((uint8_t*)buffer + s[4]);
						if (!buffer_register){
							off = s[0];
						}
						break;
					}
					case FP_IK_FN_GP_CL_IC : {
						key = get_IK((uint8_t*)buffer + s[0]);
						siz = get_size((uint8_t*)buffer + s[4]);
						if (!buffer_register){
							off = i;
						}
						break;
					}
					case FP_FN_IK_GP_CL_IC : {
						key = get_IK((uint8_t*)buffer + s[1]);
						siz = get_size((uint8_t*)buffer + s[4]);
						if (!buffer_register){
							off = i;
						}
						break;
					}
					case FP_FN_GP_IK_CL_IC : {
						key = get_IK((uint8_t*)buffer + s[2]);
						siz = get_size((uint8_t*)buffer + s[4]);
						if (!buffer_register){
							off = i;
						}
						break;
					}
					case FP_FN_GP_CL_IK_IC : {
						key = get_IK((uint8_t*)buffer + s[3]);
						siz = get_size((uint8_t*)buffer + s[4]);
						if (!buffer_register){
							off = i;
						}
						break;
					}
					case FP_FN_GP_CL_IC_IK : {
						key = get_IK((uint8_t*)buffer + s[4]);
						siz = get_size((uint8_t*)buffer + s[3]);
						if (!buffer_register){
							off = i;
						}
						break;
					}
				}

				if (((uint8_t*)buffer)[s[5]] == 0x83){
					off += (size_t)((uint8_t*)buffer)[s[5] + 5] + 4;
				}
				else{
					off += (size_t)((uint8_t*)buffer)[s[5] + 2];
				}

				printf("[+] SGN: key: 0x%x, size: 0x%zx, off: 0x%zx, buffer register: %c\n", key, siz, off, buffer_register ? 'Y' : 'N');

				if (off + siz * 4 > len){
					fprintf(stderr, "[-] SGN: incorrect size\n");
					continue;
				}

				if (off >= l + LOOP_SIZE || off + 4 < l + LOOP_SIZE){
					fprintf(stderr, "[-] SGN: incorrect offset\n");
					continue;
				}

				c = l + LOOP_SIZE - off;
				first = *(uint32_t*)((uint8_t*)buffer + off) ^ key;
				disp = 8 * (4 - c);

				if ((c == 1) ? (first << 24) == (((uint32_t)LOOP_OPCODE << 16) & 0xff000000) : ((first << disp) & 0xffff0000) == ((uint32_t)LOOP_OPCODE << 16)){
					printf("[+] SGN: cutoff verification succeed\n");
				}
				else{
					fprintf(stderr, "[-] SGN: cutoff verification failed (%x), will continue anyway ...\n", first << disp);
				}

				if ((d = decode((uint32_t*)((uint8_t*)buffer + off), key, siz)) == NULL){
					fprintf(stderr, "[-] SGN: unable to decode buffer\n");
					continue;
				}

				if ((file = dump_open("sgn", "extrude")) < 0){
					fprintf(stderr, "[-] SGN: unable to open dump file\n");
				}
				else{
					write(file, (uint8_t*)d + c, siz * 4 - c);
					close(file);

					printf("[+] SGN: the decoded data was saved\n");
				}

				if ((clone = malloc(len)) == NULL){
					fprintf(stderr, "[-] SGN: unable to allocate memory\n");
				}
				else{
					memcpy(clone, buffer, len);
					memcpy(clone + off, d, siz * 4);
					memset(clone + i, 0x90, l + LOOP_SIZE - i);

					if ((file = dump_open("sgn", "inplace")) < 0){
						fprintf(stderr, "[-] SGN: unable to open dump file\n");
					}
					else{
						write(file, clone, len);
						close(file);

						printf("[+] SGN: the original file with the decoded data was saved\n");
					}

					free(clone);
				}

				free(d);
			}
		}
	}

	return 0;
}
