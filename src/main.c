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
	DIRECTIVE_CALL,
	DIRECTIVE_COMMA,
	DIRECTIVE_OPEN_PAREN,
	DIRECTIVE_VARIABLE,
	DIRECTIVE_ADDRESS,
	DIRECTIVE_INT
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
	//VariableType variable_type;
	//DirectiveValueType value_type;
	int ref_count;
	int location; //Whether this var lives in the token(0), the stack(1), or in a register(2)
	int address; //A stack relative address or a register number
	//int paren_count; //The number of parens prcedeing the directive
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
		case DIRECTIVE_VAR:
		case DIRECTIVE_ASSIGN:
		return 1;

		case DIRECTIVE_REF:
		case DIRECTIVE_DEREF:
		case DIRECTIVE_ADD:
		case DIRECTIVE_SUB:
		return 2;

		case DIRECTIVE_MUL:
		return 3;

		case DIRECTIVE_CALL:
		return 4;

		case DIRECTIVE_COMMA:
		return 5;

		default:
		return 0;
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

void process_directive_stack(DirectiveStack* stack, int next_precedence, bool close_paren, ProgramVariableStack* local_var_stack)
{
	int directive_index = stack->size - 1;
	while(true)
	{
		if(directive_index < 0) return;

		Directive* current_directive = &stack->data[directive_index];

		if(current_directive->type == DIRECTIVE_OPEN_PAREN)
		{

			if(!close_paren) return;
			if(directive_index + 1 >= stack->size)
			{
				puts("Failed to compile open paren.");
				return;
			}
			Directive directive = stack->data[directive_index + 1];
			directive_stack_pop(stack);
			directive_stack_pop(stack);
			directive_stack_push(stack, &directive);
			directive_index = stack->size - 1;
			return;
		}

		if(current_directive->type == DIRECTIVE_INT || current_directive->type == DIRECTIVE_VARIABLE || current_directive->type == DIRECTIVE_ADDRESS)
		{
			directive_index--;
			continue;
		}

		if(directive_type_precedence(current_directive->type) < next_precedence) return;

		Directive* last_pushed_directive = &stack->data[stack->size - 1];
		if (last_pushed_directive == current_directive)
		{
			return;
		}

		if(current_directive->type == DIRECTIVE_REF)
		{
			puts("mov r0, sp");
			printf("addi #%d\n", local_var_stack->stack_size - last_pushed_directive->address);
			puts("push r0");

			Directive directive = *last_pushed_directive;
			directive.ref_count = 0;
			directive.type = DIRECTIVE_ADDRESS;
			directive.location = 1;
			directive.address = local_var_stack->stack_size;
			local_var_stack->stack_size++;
			directive_stack_pop(stack);
			directive_stack_pop(stack);
			directive_stack_push(stack, &directive);
			directive_index = stack->size - 1;
			continue;
		}
		if(current_directive->type == DIRECTIVE_DEREF)
		{
			Directive directive = *last_pushed_directive;
			directive.ref_count++;
			directive_stack_pop(stack);
			directive_stack_pop(stack);
			directive_stack_push(stack, &directive);
			directive_index = stack->size - 1;
			continue;
		}

		if (directive_index == 0)
		{
			puts("Failed to compile assignment directive");
			return;
		}
		Directive *previous_directive = &stack->data[directive_index - 1];

		if(current_directive->type == DIRECTIVE_ASSIGN)
		{
			if(previous_directive->type == DIRECTIVE_VAR)
			{
				if(last_pushed_directive->location == 0)
				{
					if(last_pushed_directive->type == DIRECTIVE_INT)
					{
						printf("movi #%d\n", last_pushed_directive->token->int_literal);
						puts("push r0");
						local_var_stack->stack_size++;
					}
					else
					{
						puts("mov r0, sp");
						printf("addi #%d\n", local_var_stack->stack_size - last_pushed_directive->address);
						puts("ldr r2, r0");
						puts("push r2");
						local_var_stack->stack_size++;
					}
				}
				ProgramVariable pv = {0};
				pv.address = local_var_stack->stack_size - 1;
				pv.token = previous_directive->token;
				pv.scope = local_var_stack->scope_counter;
				prog_var_stack_push(local_var_stack, &pv);
				directive_stack_pop(stack);
				directive_stack_pop(stack);
				directive_stack_pop(stack);
				directive_index = stack->size - 1;
				continue;
			}
			if(last_pushed_directive->location == 1)
			{
				puts("pop r2");
				local_var_stack->stack_size--;

				for (int i = 0; i < last_pushed_directive->ref_count; i++)
				{
					puts("ldr r2, r2");
				}
			}
			else
			{
				if(last_pushed_directive->type == DIRECTIVE_INT)
				{
					printf("movi #%d\n", last_pushed_directive->token->int_literal);
					puts("mov r2, r0");
				}
				if(last_pushed_directive->type == DIRECTIVE_VARIABLE)
				{
					puts("mov r0, sp");
					printf("addi #%d\n", local_var_stack->stack_size - last_pushed_directive->address);
					puts("ldr r2, r0");
				}
				for (int i = 0; i < last_pushed_directive->ref_count; i++)
				{
					puts("ldr r2, r2");
				}
			}
			if(previous_directive->location == 0)
			{
				puts("mov r0, sp");
				printf("addi #%d\n", local_var_stack->stack_size - previous_directive->address);
				puts("mov r1, r0");

				for (int i = 0; i < previous_directive->ref_count; i++)
				{
					puts("ldr r1, r1");
				}
			}
			if(previous_directive->location == 1)
			{
				puts("pop r1");
				local_var_stack->stack_size--;

				for (int i = 0; i < previous_directive->ref_count - 1; i++)
				{
					puts("ldr r1, r1");
				}
			}

			puts("str r1, r2");
			directive_stack_pop(stack);
			directive_stack_pop(stack);
			directive_stack_pop(stack);
			directive_index = stack->size - 1;
			continue;

			puts("Failed to compile assignment directive");
			return;
		}

		if(current_directive->type == DIRECTIVE_ADD)
		{
			if(last_pushed_directive->location == 1)
			{
				puts("pop r2");
				local_var_stack->stack_size--;
				
				for (int i = 0; i < last_pushed_directive->ref_count; i++)
				{
					puts("ldr r2, r2");
				}
			}
			else
			{
				if(last_pushed_directive->type == DIRECTIVE_INT)
				{
					printf("movi #%d\n", last_pushed_directive->token->int_literal);
					puts("mov r2, r0");
				}
				if(last_pushed_directive->type == DIRECTIVE_VARIABLE)
				{
					puts("mov r0, sp");
					printf("addi #%d\n", local_var_stack->stack_size - last_pushed_directive->address);
					puts("ldr r2, r0");
				}
				for (int i = 0; i < last_pushed_directive->ref_count; i++)
				{
					puts("ldr r2, r2");
				}
			}
			if(previous_directive->location == 0)
			{
				if(previous_directive->type == DIRECTIVE_INT)
				{
					printf("movi #%d\n", previous_directive->token->int_literal);
					puts("mov r1, r0");
				}
				if(previous_directive->type == DIRECTIVE_VARIABLE)
				{
					puts("mov r0, sp");
					printf("addi #%d\n", local_var_stack->stack_size - previous_directive->address);
					puts("ldr r1, r0");

					for (int i = 0; i < previous_directive->ref_count; i++)
					{
						puts("ldr r1, r1");
					}
				}
			}
			if(previous_directive->location == 1)
			{
				puts("pop r1");
				local_var_stack->stack_size--;

				if(previous_directive->type == DIRECTIVE_VARIABLE)
				{
					puts("ldr r1, r1");

					for (int i = 0; i < previous_directive->ref_count; i++)
					{
						puts("ldr r1, r1");
					}
				}
			}

			puts("add r1, r2");
			puts("push r1");
			local_var_stack->stack_size++;
			Directive directive = *last_pushed_directive;
			directive.location = 1;
			directive_stack_pop(stack);
			directive_stack_pop(stack);
			directive_stack_pop(stack);
			directive_stack_push(stack, &directive);
			directive_index = stack->size - 1;
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

		if(current_token->type == TOKEN_TYPE_CLOSE_PAREN)
		{
			process_directive_stack(stack, 0, true, local_var_stack);
			continue;
		}
		if(current_token->type == TOKEN_TYPE_OPEN_PAREN)
		{
			Directive directive = {0};
			directive.type = DIRECTIVE_OPEN_PAREN;
			directive_stack_push(stack, &directive);
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
			if(stack->size == 0)
			{
				directive_type = DIRECTIVE_DEREF;
				break;
			}
			if(stack->size >= 1 && stack->data[stack->size - 1].type != DIRECTIVE_VARIABLE && stack->data[stack->size - 1].type != DIRECTIVE_INT)
			{
				directive_type = DIRECTIVE_DEREF;
				break;
			}
			directive_type = DIRECTIVE_MUL;
			break;

			case TOKEN_TYPE_AMP:
			directive_type = DIRECTIVE_REF;
			break;

			case TOKEN_TYPE_INTEGER_LITERAL:
			directive_type = DIRECTIVE_INT;
			break;

			case TOKEN_TYPE_IDENTIFIER:
			directive_type = DIRECTIVE_VARIABLE;
			break;

			default:
			puts("Invalid token type encountered while compiling.");
			*last_index = token_vector_index;
			return false;
		}
		int directive_precedence = directive_type_precedence(directive_type);

		if(directive_precedence != 0)
		{
			for (int i = stack->size - 1; i >= 0; i--)
			{
				if (stack->data[i].type != DIRECTIVE_VARIABLE &&
					stack->data[i].type != DIRECTIVE_INT &&
					stack->data[i].type != DIRECTIVE_OPEN_PAREN &&
					stack->data[i].type != DIRECTIVE_VAR)
				{
					if (directive_precedence <= directive_type_precedence(stack->data[i].type))
					{
						process_directive_stack(stack, directive_precedence, false, local_var_stack);
					}
					break;
				}
			}
		}

		current_token = &tv->data[token_vector_index];

		if(current_token->type == TOKEN_TYPE_IDENTIFIER)
		{
			int next_next_token_index = token_vector_index + 1;
			//This is a function call
			if(next_next_token_index < tv->length && tv->data[next_next_token_index].type == TOKEN_TYPE_OPEN_PAREN)
			{
				Directive directive = {0};
				directive.token = current_token;
				directive.type = DIRECTIVE_CALL;
				directive_stack_push(stack, &directive);
				continue;
			}

			//This is a variable
			ProgramVariable* pv = prog_var_stack_find(local_var_stack, current_token->name);
			if(!pv)
			{
				puts("Could not find variable.");
				*last_index = token_vector_index;
				return false;
			}

			Directive directive = {0};
			directive.token = current_token;
			directive.location = 0;
			directive.address = pv->address;
			directive.type = DIRECTIVE_VARIABLE;
			directive_stack_push(stack, &directive);
			continue;
		}

		if(current_token->type == TOKEN_TYPE_INTEGER_LITERAL)
		{
			Directive directive = {0};
			directive.token = current_token;
			directive.type = DIRECTIVE_INT;
			directive_stack_push(stack, &directive);
			continue;
		}

		Directive directive = {0};
		directive.token = current_token;
		directive.type = directive_type;
		directive_stack_push(stack, &directive);
		continue;
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
	int last_index = 0;
	while(true)
	{
		compile_tokens(&tv, last_index, &stack, &local_var_stack, &last_index);
		last_index++;
		if (last_index >= tv.length) break;
	}
}