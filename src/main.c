#include <stdio.h>
#include <stdlib.h>
#include "tokenize.h"

typedef enum
{
	DIRECTIVE_INVALID,
	DIRECTIVE_VAR,
	DIRECTIVE_ADD,
	DIRECTIVE_SUB,
	DIRECTIVE_MUL,
	DIRECTIVE_REF,
	DIRECTIVE_DEREF,
	DIRECTIVE_ASSIGN
} DirectiveType;

typedef enum
{
	VAR_TYPE_INVALID,
	VAR_TYPE_U16,
	VAR_TYPE_I16	
} VariableType;

typedef struct
{
	DirectiveType type;
	Token* token;
	VariableType variable_type;
	int location; //Whether this var lives in the token(0), the stack(1), or in a register(2)
	int address; //A stack relative address or a register number
	int paren_count; //The number of parens prcedeing the directive
} Directive;

typedef struct
{
	Directive* data;
	int size;
} DirectiveStack;

int directive_type_precedence(DirectiveType type)
{
	switch(type)
	{
		case DIRECTIVE_INVALID:
		case DIRECTIVE_VAR:
		case DIRECTIVE_ASSIGN:
		return 0;

		case DIRECTIVE_ADD:
		case DIRECTIVE_SUB:
		return 1;

		case DIRECTIVE_MUL:
		case DIRECTIVE_REF:
		case DIRECTIVE_DEREF:
		return 2;
	}
}

void directive_stack_push(DirectiveStack* stack, Directive* directive)
{
	stack->data[stack->size] = *directive;
	stack->size++;
}

void directive_stack_pop(DirectiveStack* stack)
{
	stack->size--;
}

void process_directive_stack(DirectiveStack* stack, int next_precedence, bool close_paren)
{
	while(true)
	{
		if(stack->size < 2) return;
		if(directive_type_precedence(stack->data[stack->size - 1].type) < next_precedence) return;

		Directive* current_directive = &stack->data[stack->size - 1];
		Directive* previous_directive = &stack->data[stack->size - 2];

		if(current_directive->type == DIRECTIVE_MUL)
		{
			if(current_directive->location == 0) 
			{
				printf("movi #%d\n", current_directive->token->int_literal);
				puts("mov r0, r1");
			}
			if(current_directive->location == 1) puts("pop r1");

			if(previous_directive->location == 0) 
			{
				printf("movi #%d\n", previous_directive->token->int_literal);
			}
			if(previous_directive->location == 1) puts("pop r0");

			puts("mul r0, r1");
			puts("push r0");

			previous_directive->location = 1;
			directive_stack_pop(stack);
			continue;
		}
		if(current_directive->type == DIRECTIVE_ADD)
		{
			if(current_directive->location == 0) 
			{
				printf("movi #%d\n", current_directive->token->int_literal);
				puts("mov r0, r1");
			}
			if(current_directive->location == 1) puts("pop r1");

			if(previous_directive->location == 0) 
			{
				printf("movi #%d\n", previous_directive->token->int_literal);
			}
			if(previous_directive->location == 1) puts("pop r0");

			puts("add r0, r1");
			puts("push r0");

			previous_directive->location = 1;
			directive_stack_pop(stack);
			continue;
		}
		if(current_directive->type == DIRECTIVE_SUB)
		{
			if(current_directive->location == 0) 
			{
				printf("movi #%d\n", current_directive->token->int_literal);
				puts("mov r0, r1");
			}
			if(current_directive->location == 1) puts("pop r1");

			if(previous_directive->location == 0) 
			{
				printf("movi #%d\n", previous_directive->token->int_literal);
			}
			if(previous_directive->location == 1) puts("pop r0");

			puts("sub r0, r1");
			puts("push r0");

			previous_directive->location = 1;
			directive_stack_pop(stack);
			continue;
		}

		puts("Compiler error. Unhandled directive.");
		directive_stack_pop(stack);
	}
}

bool compile_tokens(TokenVector* tv)
{
	DirectiveStack stack = {0};
	stack.data = malloc(sizeof(Directive) * 100);

	for(int token_vector_index = 0; token_vector_index < tv->length; token_vector_index++)
	{
		Token* current_token = &tv->data[token_vector_index];
		if(current_token->type == TOKEN_TYPE_VAR)
		{
			token_vector_index++;
			if(token_vector_index >= tv->length)
			{
				puts("Invalid variable definition.");
				return false;
			}
			current_token = &tv->data[token_vector_index];
			if(current_token->type != TOKEN_TYPE_IDENTIFIER)
			{
				puts("Invalid variable definition. Expected identifier.");
				return false;
			}
			Directive directive = {0};
			directive.token = current_token;
			directive.type = DIRECTIVE_VAR;
			directive_stack_push(&stack, &directive);
			continue;
		}
		if(current_token->type == TOKEN_TYPE_SEMICOLON)
		{
			process_directive_stack(&stack, 0, false);
			return true;
		}

		if(current_token->type == TOKEN_TYPE_CLOSE_PAREN)
		{
			process_directive_stack(&stack, 0, true);
			continue;
		}

		DirectiveType directive_type = DIRECTIVE_INVALID;
		switch(current_token->type)
		{
			case TOKEN_TYPE_EQUALS:
			directive_type = DIRECTIVE_ASSIGN;
			break;

			case TOKEN_TYPE_PLUS:
			directive_type = DIRECTIVE_ADD;
			break;

			case TOKEN_TYPE_MINUS:
			directive_type = DIRECTIVE_SUB;
			break;

			case TOKEN_TYPE_STAR:
			directive_type = DIRECTIVE_MUL;
			break;

			default:
			puts("Invalid token type encountered while compiling.");
			return false;
		}
		int directive_precedence = directive_type_precedence(directive_type);

		if(stack.size == 0)
		{
			puts("Expression parsing error. Expected a preceeding token.");
			return false;
		}

		int previous_directive_precedence = directive_type_precedence(stack.data[stack.size - 1].type);
		if(previous_directive_precedence >= directive_precedence)
		{
			process_directive_stack(&stack, directive_precedence, false);
		}


		//Account for multiple opening parens (((())))
		int paren_count = 0;
		while(true)
		{
			token_vector_index++;
			if (token_vector_index >= tv->length)
			{
				puts("Unexpected end of expression. Expected value after operator");
				return false;
			}

			if(tv->data[token_vector_index].type == TOKEN_TYPE_OPEN_PAREN)
			{
				paren_count++;
				continue;
			}

			break;
		}

		Token* next_token = &tv->data[token_vector_index];

		Directive directive = {0};
		directive.token = next_token;
		directive.type = directive_type;
		directive.paren_count = paren_count;
		directive_stack_push(&stack, &directive);
	}

	return true;
}

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
	token_vector_print(&tv);
	compile_tokens(&tv);
}