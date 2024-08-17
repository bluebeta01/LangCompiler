#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "tokenize.h"

void token_vector_init(TokenVector* tv, int capacity)
{
	tv->length = 0;
	tv->capacity = capacity;
	tv->data = malloc(sizeof(Token) * capacity);
}

void token_vector_push(TokenVector* tv, Token* token)
{
	if (tv->length == tv->capacity)
	{
		Token* new_array = malloc(sizeof(Token) * (tv->capacity * 2 + 1));
		memcpy(new_array, tv->data, sizeof(Token) * tv->length);
		free(tv->data);
		tv->data = new_array;
		tv->capacity = tv->capacity * 2 + 1;
	}
	tv->data[tv->length] = *token;
	tv->length++;
}

Token* token_vector_at(TokenVector* tv, int index)
{
	if (index >= tv->capacity)
		return NULL;
	return &tv->data[index];
}

bool tokenize_file(FILE* file, TokenVector* tv)
{
	char buffer[64];
	int buffer_length = 0;
	int buffer_index = 0;
	buffer[63] = 0;
	buffer_length = fread(buffer, sizeof(char), 63, file);
	int read_result = ferror(file);
	int eof = feof(file);
	if (read_result != 0)
	{
		puts("Error while reading file in tokenizer.");
		return false;
	}
	if (buffer_length == 0) return true;

	bool read_more = false;

	while (true)
	{
		if (buffer_index == buffer_length) break;
		if (read_more)
		{

		}
		bool token_found = false;
		int token_length = 0;
		int next_read_index = buffer_index;
		if (isalpha(buffer[next_read_index]))
		{
			while (true)
			{
				next_read_index++;
				token_length++;
				if (next_read_index == buffer_length)
				{
					token_found = eof != 0;
					read_more = eof == 0;
					break;
				}
				if (!isalnum(buffer[next_read_index]))
				{
					token_found = true;
					break;
				}
			}
		}
		if (token_found)
		{
			if (token_length == 3 && buffer[buffer_index] == 'v' && buffer[buffer_index + 1] == 'a' && buffer[buffer_index + 2] == 'r')
			{
				Token token = {0};
				token.type = TOKEN_TYPE_VAR;
				token_vector_push(tv, &token);
				buffer_index = next_read_index;
				continue;
			}
			else if (token_length == 3 && buffer[buffer_index] == 'u' && buffer[buffer_index + 1] == '1' && buffer[buffer_index + 2] == '6')
			{
				Token token = {0};
				token.type = TOKEN_TYPE_U16;
				token_vector_push(tv, &token);
				buffer_index = next_read_index;
				continue;
			}
			else if (token_length == 3 && buffer[buffer_index] == 'i' && buffer[buffer_index + 1] == '1' && buffer[buffer_index + 2] == '6')
			{
				Token token = {0};
				token.type = TOKEN_TYPE_I16;
				token_vector_push(tv, &token);
				buffer_index = next_read_index;
				continue;
			}
			Token token = { 0 };
			token.type = TOKEN_TYPE_IDENTIFIER;
			token.name = malloc(sizeof(char) * (token_length + 1));
			memcpy(token.name, &buffer[buffer_index], token_length);
			token.name[token_length] = 0;
			token_vector_push(tv, &token);
			buffer_index = next_read_index;
			continue;
		}

		next_read_index = buffer_index;
		token_length = 0;


		if (isspace(buffer[buffer_index]))
		{
			buffer_index++;
			if (buffer_index == buffer_length) break;
		}

		buffer_index++;
	}
}
