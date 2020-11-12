#include "compress40.h"
#include <assert.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "uarray2.h"
#include "pnm.h"
#include "a2plain.h"
#include "a2methods.h"
#include "arith40.h"
#include "bitpack.h"

int main(int argc, char *argv[]){
	(void)argc;
	FILE *input = fopen(argv[1], "r");
	compress40(input);

	fclose(input);
	return 0;


	
	// (void)argc;
	// (void)argv;

	// uint64_t word1 = 0x3f4;
	// uint64_t word2 = 0x3f4;
	// uint64_t uno = Bitpack_getu(word1, 6, 2);
	// int64_t dos = Bitpack_gets(word2, 6, 2);
	// printf("unsigned: %lu\n", uno);
	// printf("signed: %ld\n", dos);

	// word1 = Bitpack_newu(word1, 6, 2, 50);
	// word2 = Bitpack_news(word2, 6, 2, -5);

	// printf("unsigned: %lu\n", Bitpack_getu(word1, 6, 2));
	// printf("signed: %ld\n", Bitpack_gets(word2, 6, 2));

	// return 0;
}
