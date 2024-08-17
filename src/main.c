#include <stdio.h>
#include "tokenize.h"

int main(int argc, const char** argv)
{
	if (argc < 2)
	{
		puts("Filepath argument missing.");
		return 1;
	}

	FILE* file = fopen(argv[1], "r");
	if (!file)
	{
		printf("Failed to open file %s\n", argv[1]);
		return 1;
	}

	TokenVector tv = {0};
	token_vector_init(&tv, 10);
	tokenize_file(file, &tv);

	printf("Count: %d\n", tv.length);
}