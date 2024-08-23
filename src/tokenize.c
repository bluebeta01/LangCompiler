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
	char buffer[10];
	int buffer_length = 0;
	int buffer_read_index = 0;
	int eof = 0;
	bool read_more = false;

	while (true)
	{
		if (read_more || buffer_read_index >= buffer_length)
		{
			if (eof) return true;
			if (read_more && buffer_read_index == 0)
			{
				puts("Token length limit exceeded!");
				return false;
			}
			if (buffer_read_index < buffer_length)
			{
				int char_count = buffer_length - buffer_read_index;
				memmove(buffer, &buffer[buffer_read_index], char_count);
				buffer_length = fread(&buffer[char_count], sizeof(char), 10 - char_count, file);
				buffer_length += char_count;
			}
			else
			{
				buffer_length = fread(buffer, sizeof(char), 10, file);
			}
			buffer_read_index = 0;
			int read_result = ferror(file);
			eof = feof(file);
			if (read_result != 0)
			{
				puts("Error while reading file in tokenizer.");
				return false;
			}
			if (buffer_length == 0) return true;
			read_more = false;
		}

		if (buffer_length - buffer_read_index >= 3 && buffer[buffer_read_index] == 'v' && buffer[buffer_read_index + 1] == 'a' && buffer[buffer_read_index + 2] == 'r')
		{
			int chars_in_buffer = buffer_length - buffer_read_index;
			if(chars_in_buffer > 3 && !isalnum(buffer[buffer_read_index + 3]))
			{
				Token token = {0};
				token.type = TOKEN_TYPE_VAR;
				token_vector_push(tv, &token);
				buffer_read_index += 3;
				continue;
			}
			if(chars_in_buffer == 3)
			{
				if(eof)
				{
					Token token = {0};
					token.type = TOKEN_TYPE_VAR;
					token_vector_push(tv, &token);
					buffer_read_index += 3;
					continue;
				}
				else
				{
					read_more = true;
					continue;
				}
			}
		}

		if (buffer[buffer_read_index] == '=')
		{
			Token token = { 0 };
			token.type = TOKEN_TYPE_EQUALS;
			token_vector_push(tv, &token);
			buffer_read_index++;
			continue;
		}
		if (buffer[buffer_read_index] == '-')
		{
			Token token = { 0 };
			token.type = TOKEN_TYPE_MINUS;
			token_vector_push(tv, &token);
			buffer_read_index++;
			continue;
		}
		if (buffer[buffer_read_index] == '+')
		{
			Token token = { 0 };
			token.type = TOKEN_TYPE_PLUS;
			token_vector_push(tv, &token);
			buffer_read_index++;
			continue;
		}
		if (buffer[buffer_read_index] == ';')
		{
			Token token = { 0 };
			token.type = TOKEN_TYPE_SEMICOLON;
			token_vector_push(tv, &token);
			buffer_read_index++;
			continue;
		}
		if (buffer[buffer_read_index] == '(')
		{
			Token token = { 0 };
			token.type = TOKEN_TYPE_OPEN_PAREN;
			token_vector_push(tv, &token);
			buffer_read_index++;
			continue;
		}
		if (buffer[buffer_read_index] == ')')
		{
			Token token = { 0 };
			token.type = TOKEN_TYPE_CLOSE_PAREN;
			token_vector_push(tv, &token);
			buffer_read_index++;
			continue;
		}
		if (buffer[buffer_read_index] == '*')
		{
			Token token = { 0 };
			token.type = TOKEN_TYPE_STAR;
			token_vector_push(tv, &token);
			buffer_read_index++;
			continue;
		}
		if (buffer[buffer_read_index] == '&')
		{
			Token token = { 0 };
			token.type = TOKEN_TYPE_AMP;
			token_vector_push(tv, &token);
			buffer_read_index++;
			continue;
		}

		if(isdigit(buffer[buffer_read_index]))
		{
			int temp_read_index = buffer_read_index;
			bool match = false;
			while(true)
			{
				temp_read_index++;
				if(temp_read_index >= buffer_length)
				{
					if(eof)
					{
						int token_length = temp_read_index - buffer_read_index;
						if(token_length > 11)
						{
							puts("Tokenizer doesn't support numbers largers than 16 digits");
							return false;
						}
						char int_char_buffer[12];
						memcpy(int_char_buffer, &buffer[buffer_read_index], token_length);
						int_char_buffer[token_length] = 0;
						int value = atoi(int_char_buffer);

						Token token = {0};
						token.type = TOKEN_TYPE_INTEGER_LITERAL;
						token.int_literal = value;
						token_vector_push(tv, &token);
						buffer_read_index = temp_read_index;
						match = true;
						break;
					}
					else
					{
						read_more = true;
						break;
					}
				}
				if(!isdigit(buffer[temp_read_index]))
				{
					int token_length = temp_read_index - buffer_read_index;
					if (token_length > 11)
					{
						puts("Tokenizer doesn't support numbers largers than 16 digits");
						return false;
					}
					char int_char_buffer[12];
					memcpy(int_char_buffer, &buffer[buffer_read_index], token_length);
					int_char_buffer[token_length] = 0;
					int value = atoi(int_char_buffer);

					Token token = {0};
					token.type = TOKEN_TYPE_INTEGER_LITERAL;
					token.int_literal = value;
					token_vector_push(tv, &token);
					buffer_read_index = temp_read_index;
					match = true;
					break;
				}
			}
			if(temp_read_index >= buffer_length || match) continue;
		}

		if(isalpha(buffer[buffer_read_index]))
		{
			int temp_read_index = buffer_read_index;
			bool match = false;
			while(true)
			{
				temp_read_index++;
				if(temp_read_index >= buffer_length)
				{
					if(eof)
					{
						int token_length = temp_read_index - buffer_read_index;
						Token token = {0};
						token.type = TOKEN_TYPE_IDENTIFIER;
						token.name = malloc(sizeof(char) * (token_length + 1));
						memcpy(token.name, &buffer[buffer_read_index], token_length);
						token.name[token_length] = 0;
						token_vector_push(tv, &token);
						buffer_read_index = temp_read_index;
						match = true;
						break;
					}
					else
					{
						read_more = true;
						break;
					}
				}
				if(!isalnum(buffer[temp_read_index]))
				{
					int token_length = temp_read_index - buffer_read_index;
					Token token = {0};
					token.type = TOKEN_TYPE_IDENTIFIER;
					token.name = malloc(sizeof(char) * (token_length + 1));
					memcpy(token.name, &buffer[buffer_read_index], token_length);
					token.name[token_length] = 0;
					token_vector_push(tv, &token);
					buffer_read_index = temp_read_index;
					match = true;
					break;
				}
			}
			if(temp_read_index >= buffer_length || match) continue;
		}

		if(isspace(buffer[buffer_read_index]))
		{
			buffer_read_index++;
			continue;
		}

		//We should never hit this if the file is well formatted
		puts("Tokenizer error");
		return false;
	}
}

void token_vector_print(TokenVector* tv)
{
	for(int i = 0; i < tv->length; i++)
	{
		Token* token = &tv->data[i];
		switch(token->type)
		{
		case TOKEN_TYPE_INVALID:
			puts("Invalid Token");
			break;
		case TOKEN_TYPE_VAR:
			puts("VAR");
			break;
		case TOKEN_TYPE_EQUALS:
			puts("=");
			break;
		case TOKEN_TYPE_PLUS:
			puts("+");
			break;
		case TOKEN_TYPE_MINUS:
			puts("-");
			break;
		case TOKEN_TYPE_STAR:
			puts("*");
			break;
		case TOKEN_TYPE_IDENTIFIER:
			printf("%s\n", token->name);
			break;
		case TOKEN_TYPE_INTEGER_LITERAL:
			printf("%d\n", token->int_literal);
			break;
		case TOKEN_TYPE_OPEN_PAREN:
			puts("(");
			break;
		case TOKEN_TYPE_CLOSE_PAREN:
			puts(")");
			break;
		case TOKEN_TYPE_U16:
			puts("U16");
			break;
		case TOKEN_TYPE_I16:
			puts("I16");
			break;
		case TOKEN_TYPE_AMP:
			puts("&");
			break;
		case TOKEN_TYPE_SEMICOLON:
			puts(";");
			break;
		}
	}
}