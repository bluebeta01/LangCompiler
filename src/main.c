#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
	DIRECTIVE_ASSIGN,
	DIRECTIVE_CALL
} DirectiveType;

typedef enum
{
	VAR_TYPE_INVALID,
	VAR_TYPE_U16,
	VAR_TYPE_I16	
} VariableType;

typedef enum
{
	DIRECTIVE_VALUE_TYPE_INT,
	DIRECTIVE_VALUE_TYPE_CALL,
	DIRECTIVE_VALUE_TYPE_VAR,

} DirectiveValueType;

typedef struct
{
	DirectiveType type;
	Token* token;
	VariableType variable_type;
	DirectiveValueType value_type;
	int location; //Whether this var lives in the token(0), the stack(1), or in a register(2)
	int address; //A stack relative address or a register number
	int paren_count; //The number of parens prcedeing the directive
} Directive;

typedef struct
{
	Directive* data;
	int size;
} DirectiveStack;

typedef struct
{
	VariableType type;
	Token* token;
	int address; //Stack pointer relative address
	int scope; //What scope this var is in
} ProgramVariable;

typedef struct
{
	ProgramVariable* data;
	int length;
	int stack_size;
	int scope_counter;
} ProgramVariableStack;

void prog_var_stack_push(ProgramVariableStack* stack, ProgramVariable* var)
{
	stack->data[stack->length] = *var;
	stack->length++;
	stack->stack_size++;
}

void prog_var_stack_pop(ProgramVariableStack* stack)
{
	stack->length--;
}

ProgramVariable* prog_var_stack_find(ProgramVariableStack* stack, const char* name)
{
	for(int i = stack->length - 1; i >= 0; i--)
	{
		if(!strcmp(stack->data[i].token->name, name)) return &stack->data[i];
	}

	return NULL;
}

int directive_type_precedence(DirectiveType type)
{
	switch(type)
	{
		case DIRECTIVE_INVALID:
		return 0;

		case DIRECTIVE_VAR:
		case DIRECTIVE_ASSIGN:
		return 1;

		case DIRECTIVE_ADD:
		case DIRECTIVE_SUB:
		return 2;

		case DIRECTIVE_MUL:
		case DIRECTIVE_REF:
		case DIRECTIVE_DEREF:
		return 3;
	}
	return 0;
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

void process_directive_stack(DirectiveStack* stack, int next_precedence, bool close_paren, ProgramVariableStack* local_var_stack)
{
	while(true)
	{
		if(stack->size <= 0) return;

		Directive* current_directive = &stack->data[stack->size - 1];

		if(current_directive->paren_count > 0 && close_paren)
		{
			current_directive->paren_count--;
			close_paren = false;
			if (current_directive->value_type == DIRECTIVE_VALUE_TYPE_CALL)
			{
				printf("call %s\n", current_directive->token->name);
				current_directive->location = 1;
				current_directive->value_type = DIRECTIVE_VALUE_TYPE_INT;
			}
			return;
		}

		if(current_directive->paren_count > 0)
		{
			return;
		}
		
		if(directive_type_precedence(stack->data[stack->size - 1].type) < next_precedence && !close_paren) return;

		if(current_directive->type == DIRECTIVE_ASSIGN)
		{
			if(stack->size < 2)
			{
				puts("Failed to compile assignment directive");
				return;
			}
			Directive* previous_directive = &stack->data[stack->size - 2];
			if(previous_directive->type == DIRECTIVE_VAR)
			{
				ProgramVariable pv = {0};
				pv.address = local_var_stack->stack_size + 1;
				pv.token = previous_directive->token;
				pv.type = current_directive->variable_type;
				pv.scope = local_var_stack->scope_counter;
				prog_var_stack_push(local_var_stack, &pv);
				directive_stack_pop(stack);
				directive_stack_pop(stack);
				continue;
			}
			if(previous_directive->location == 1)
			{
				puts("mov r0, sp");
				printf("subi %d\n", previous_directive->location);
				puts("mov r1, r0");
				if(current_directive->location == 0)
				{
					printf("movi %d\n", current_directive->token->int_literal);
				}
				if(current_directive->location == 1)
				{
					puts("pop r0");
				}
				puts("str r1, r0");
				directive_stack_pop(stack);
				directive_stack_pop(stack);
				continue;
			}
			puts("Failed to compile assignment directive");
			return;
		}

		if(current_directive->type == DIRECTIVE_INVALID)
		{
			if(current_directive->location == 0) 
			{
				printf("movi #%d\n", current_directive->token->int_literal);
			}
			if(current_directive->location == 1) puts("pop r0");

			puts("push r0");

			directive_stack_pop(stack);
			continue;
		}

		if(stack->size == 1) return;
		Directive* previous_directive = &stack->data[stack->size - 2];

		if(current_directive->type == DIRECTIVE_MUL)
		{
			if(current_directive->location == 0) 
			{
				printf("movi #%d\n", current_directive->token->int_literal);
				puts("mov r1, r0");
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
				puts("mov r1, r0");
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
				puts("mov r1, r0");
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

bool compile_tokens(TokenVector* tv, int start_index, DirectiveStack* stack, ProgramVariableStack* local_var_stack, int* last_index)
{
	int token_vector_index = start_index;
	for(; token_vector_index < tv->length; token_vector_index++)
	{
		int paren_count = 0;
		Token* current_token = &tv->data[token_vector_index];
		if(current_token->type == TOKEN_TYPE_VAR)
		{
			token_vector_index++;
			if(token_vector_index >= tv->length)
			{
				puts("Invalid variable definition.");
				*last_index = token_vector_index;
				return false;
			}
			current_token = &tv->data[token_vector_index];
			if(current_token->type != TOKEN_TYPE_IDENTIFIER)
			{
				puts("Invalid variable definition. Expected identifier.");
				*last_index = token_vector_index;
				return false;
			}
			Directive directive = {0};
			directive.token = current_token;
			directive.type = DIRECTIVE_VAR;
			directive_stack_push(stack, &directive);
			continue;
		}
		if(current_token->type == TOKEN_TYPE_SEMICOLON)
		{
			process_directive_stack(stack, 0, false, local_var_stack);
			if (stack->size != 0)
			{
				puts("Failed to compile expression. Some directives could not be processed");
				*last_index = token_vector_index;
				return false;
			}
			*last_index = token_vector_index;
			return true;
		}
		if(current_token->type == TOKEN_TYPE_COMMA)
		{
			process_directive_stack(stack, 0, false, local_var_stack);
			continue;
		}

		if(current_token->type == TOKEN_TYPE_CLOSE_PAREN)
		{
			process_directive_stack(stack, 0, true, local_var_stack);
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

			case TOKEN_TYPE_OPEN_PAREN:
			directive_type = DIRECTIVE_INVALID;
			paren_count++;
			break;

			case TOKEN_TYPE_INTEGER_LITERAL:
			case TOKEN_TYPE_IDENTIFIER:
			directive_type = DIRECTIVE_INVALID;
			break;

			default:
			puts("Invalid token type encountered while compiling.");
			*last_index = token_vector_index;
			return false;
		}
		int directive_precedence = directive_type_precedence(directive_type);

		if(stack->size != 0)
		{
			int previous_directive_precedence = directive_type_precedence(stack->data[stack->size - 1].type);
			if (previous_directive_precedence >= directive_precedence)
			{
				process_directive_stack(stack, directive_precedence, false, local_var_stack);
			}
		}


		//Account for multiple opening parens (((())))
		if(current_token->type != TOKEN_TYPE_INTEGER_LITERAL && current_token->type != TOKEN_TYPE_IDENTIFIER)
		{
			while (true)
			{
				token_vector_index++;
				if (token_vector_index >= tv->length)
				{
					puts("Unexpected end of expression. Expected value after operator");
					*last_index = token_vector_index;
					return false;
				}

				if (tv->data[token_vector_index].type == TOKEN_TYPE_OPEN_PAREN)
				{
					paren_count++;
					continue;
				}

				break;
			}
		}

		Token* next_token = &tv->data[token_vector_index];

		if(next_token->type == TOKEN_TYPE_IDENTIFIER)
		{
			int next_next_token_index = token_vector_index + 1;
			//This is a function call
			if(next_next_token_index < tv->length && tv->data[next_next_token_index].type == TOKEN_TYPE_OPEN_PAREN)
			{
				Directive directive = {0};
				directive.token = next_token;
				directive.type = directive_type;
				directive.paren_count = paren_count + 1;
				directive.value_type = DIRECTIVE_VALUE_TYPE_CALL;
				directive_stack_push(stack, &directive);

				token_vector_index++;
				continue;
			}

			//This is a variable
			ProgramVariable* pv = prog_var_stack_find(local_var_stack, next_token->name);
			if(!pv)
			{
				puts("Could not find variable.");
				*last_index = token_vector_index;
				return false;
			}

			Directive directive = {0};
			directive.token = next_token;
			directive.location = 1;
			directive.address = pv->address;
			directive.type = directive_type;
			directive.paren_count = paren_count;
			directive.value_type = DIRECTIVE_VALUE_TYPE_INT;
			directive_stack_push(stack, &directive);
			continue;
		}

		Directive directive = {0};
		directive.token = next_token;
		directive.type = directive_type;
		directive.paren_count = paren_count;
		directive.value_type = DIRECTIVE_VALUE_TYPE_INT;
		directive_stack_push(stack, &directive);
	}

	if(stack->size != 0)
	{
		puts("Failed to compile expression. Some directives could not be processed");
		*last_index = token_vector_index;
		return false;
	}
	*last_index = token_vector_index;
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

	DirectiveStack stack = {0};
	stack.data = malloc(sizeof(Directive) * 100);
	ProgramVariableStack local_var_stack = {0};
	local_var_stack.data = malloc(sizeof(ProgramVariable) * 100);

	printf("Count: %d\n", tv.length);
	token_vector_print(&tv);
	int last_index;
	compile_tokens(&tv, 0, &stack, &local_var_stack, &last_index);
	last_index++;
	if(last_index >= tv.length) return 0;
	compile_tokens(&tv, last_index, &stack, &local_var_stack, &last_index);
}