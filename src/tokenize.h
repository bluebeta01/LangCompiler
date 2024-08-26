#ifndef TOKENIZE_H
#define TOKENIZE_H
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

typedef enum
{
	TOKEN_TYPE_INVALID,
	TOKEN_TYPE_VAR,
	TOKEN_TYPE_EQUALS,
	TOKEN_TYPE_PLUS,
	TOKEN_TYPE_MINUS,
	TOKEN_TYPE_STAR,
	TOKEN_TYPE_AMP,
	TOKEN_TYPE_SEMICOLON,
	TOKEN_TYPE_IDENTIFIER,
	TOKEN_TYPE_INTEGER_LITERAL,
	TOKEN_TYPE_OPEN_PAREN,
	TOKEN_TYPE_CLOSE_PAREN,
	TOKEN_TYPE_U16,
	TOKEN_TYPE_I16,
	TOKEN_TYPE_COMMA,
} TokenType;

typedef struct
{
	TokenType type;
	char* name;
	int int_literal;
} Token;

typedef struct
{
	Token* data;
	int length;
	int capacity;
} TokenVector;

void token_vector_init(TokenVector* tv, int capacity);
void token_vector_push(TokenVector* tv, Token* token);
Token* token_vector_at(TokenVector* tv, int index);

bool tokenize_file(FILE* file, TokenVector* tv);

void token_vector_print(TokenVector* tv);

#endif // !TOKENIZE_H
