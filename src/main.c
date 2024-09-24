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
	PRIMITIVE_TYPE_INVALID,
	PRIMITIVE_TYPE_VOID,
	PRIMITIVE_TYPE_U16,
	PRIMITIVE_TYPE_I16,
	PRIMITIVE_TYPE_STRUCT
} PrimitiveType;

typedef struct TypeDescriptor TypeDescriptor;
typedef struct StructDescriptorEntry StructDescriptorEntry;

typedef struct
{
	int length;
	int capacity;
	StructDescriptorEntry *data;
} StructDescriptorEntryVector;

typedef struct
{
	int size;
	StructDescriptorEntryVector entries;
} StructDescriptor;

struct TypeDescriptor
{
	PrimitiveType primitive_type;
	int pointer_count;
	int size;
	const char *type_name;
	StructDescriptor struct_descriptor;
};

struct StructDescriptorEntry
{
	TypeDescriptor *type_descriptor;
	const char *entry_name;
	int offset;
	int pointer_count;
};

typedef struct
{
	TypeDescriptor *data;
	int length;
	int capacity;
} TypeDescriptorVector;

typedef struct
{
	DirectiveType type;
	Token *token;
	TypeDescriptor *type_descriptor;
	int ref_count; // The number of times we need to defref this to get to it's value
	int location; // Whether this var lives in the token(0), the stack(1), or in a register(2)
	int address;  // A stack relative address or a register number
	int pointer_count; // The number of stars if this directive represents a number (eg. u16** has pointer_count of 2)
} Directive;

typedef struct
{
	Directive *data;
	int size;
} DirectiveStack;

typedef struct
{
	Token *token;
	TypeDescriptor *type_descriptor;
	int address; // Stack pointer relative address
	int scope;	 // What scope this var is in
	int pointer_count;
} ProgramVariable;

typedef struct
{
	ProgramVariable *data;
	int length;
	int stack_size;
	int scope_counter;
} ProgramVariableStack;

TypeDescriptorVector g_tdv;

void sdev_push(StructDescriptorEntryVector *vector, StructDescriptorEntry *entry)
{
	if(vector->length == vector->capacity)
	{
		//TODO: implement this
		puts("StructDescriptorEntryVector full!");
		return;
	}

	vector->data[vector->length] = *entry;
	vector->length++;
}

void struct_descriptor_init(StructDescriptor *descriptor, int initial_entry_capacity)
{
	StructDescriptor desc = {0};
	desc.entries.data = malloc(sizeof(StructDescriptorEntry) * initial_entry_capacity);
	desc.entries.capacity = initial_entry_capacity;
	*descriptor = desc;
}

void struct_descriptor_free(StructDescriptor *descriptor)
{
	free(descriptor->entries.data);
}

void prog_var_stack_push(ProgramVariableStack *stack, ProgramVariable *var)
{
	stack->data[stack->length] = *var;
	stack->length++;
}

void prog_var_stack_pop(ProgramVariableStack *stack)
{
	stack->length--;
}

void type_desc_vector_push(TypeDescriptorVector *tdv, TypeDescriptor *td)
{
	if(tdv->length == tdv->capacity)
	{
		puts("TypeDescriptorVector is full!");
		return;
	}
	tdv->data[tdv->length] = *td;
	tdv->length++;
}

void type_desc_vector_init(TypeDescriptorVector *tdv)
{
	tdv->length = 0;
	tdv->capacity = 10;
	tdv->data = malloc(sizeof(TypeDescriptorVector) * 10);
}

TypeDescriptor *get_type_by_name(TypeDescriptorVector *tdv, Token *name_token)
{
	if(name_token->type == TOKEN_TYPE_U16)
	{
		static TypeDescriptor desc = {0};
		desc.primitive_type = PRIMITIVE_TYPE_U16;
		desc.type_name = "u16";
		desc.size = 1;
		return &desc;
	}
	if(name_token->type == TOKEN_TYPE_I16)
	{
		static TypeDescriptor desc = {0};
		desc.primitive_type = PRIMITIVE_TYPE_I16;
		desc.type_name = "i16";
		desc.size = 1;
		return &desc;
	}
	if(name_token->type == TOKEN_TYPE_VOID)
	{
		static TypeDescriptor desc = {0};
		desc.primitive_type = PRIMITIVE_TYPE_VOID;
		desc.type_name = "void";
		desc.size = 0;
		return &desc;
	}
	if(!name_token->name) return NULL;
	for(int i = 0; i < tdv->length; i++)
	{
		if(!strcmp(tdv->data[i].type_name, name_token->name)) return &tdv->data[i];
	}
	return NULL;
}

ProgramVariable *prog_var_stack_find(ProgramVariableStack *stack, const char *name)
{
	for (int i = stack->length - 1; i >= 0; i--)
	{
		if (!strcmp(stack->data[i].token->name, name))
			return &stack->data[i];
	}

	return NULL;
}

int directive_type_precedence(DirectiveType type)
{
	switch (type)
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

void directive_stack_push(DirectiveStack *stack, Directive *directive)
{
	stack->data[stack->size] = *directive;
	stack->size++;
}

void directive_stack_pop(DirectiveStack *stack)
{
	stack->size--;
}

void compile_add(Directive *lvalue_directive, Directive *rvalue_directive, ProgramVariableStack *local_var_stack)
{
	if (lvalue_directive->type_descriptor->primitive_type == PRIMITIVE_TYPE_I16 ||
		rvalue_directive->type_descriptor->primitive_type == PRIMITIVE_TYPE_U16)
	{
		if (rvalue_directive->location == 1)
		{
			puts("pop r2");
			local_var_stack->stack_size--;

			for (int i = 0; i < rvalue_directive->ref_count; i++)
			{
				puts("ldr r2, r2");
			}
		}
		else
		{
			if (rvalue_directive->type == DIRECTIVE_INT)
			{
				printf("movi #%d\n", rvalue_directive->token->int_literal);
				puts("mov r2, r0");
			}
			if (rvalue_directive->type == DIRECTIVE_VARIABLE)
			{
				puts("mov r0, sp");
				printf("addi #%d\n", local_var_stack->stack_size - rvalue_directive->address);
				puts("ldr r2, r0");
			}
			for (int i = 0; i < rvalue_directive->ref_count; i++)
			{
				puts("ldr r2, r2");
			}
		}
		if (lvalue_directive->location == 0)
		{
			if (lvalue_directive->type == DIRECTIVE_INT)
			{
				printf("movi #%d\n", lvalue_directive->token->int_literal);
				puts("mov r1, r0");
			}
			if (lvalue_directive->type == DIRECTIVE_VARIABLE)
			{
				puts("mov r0, sp");
				printf("addi #%d\n", local_var_stack->stack_size - lvalue_directive->address);
				puts("ldr r1, r0");

				for (int i = 0; i < lvalue_directive->ref_count; i++)
				{
					puts("ldr r1, r1");
				}
			}
		}
		if (lvalue_directive->location == 1)
		{
			puts("pop r1");
			local_var_stack->stack_size--;

			if (lvalue_directive->type == DIRECTIVE_VARIABLE)
			{
				puts("ldr r1, r1");

				for (int i = 0; i < lvalue_directive->ref_count; i++)
				{
					puts("ldr r1, r1");
				}
			}
		}

		puts("add r1, r2");
		puts("push r1");
		local_var_stack->stack_size++;
		lvalue_directive->location = 1;
	}
}

void process_directive_stack(DirectiveStack *stack, int next_precedence, bool close_paren,
							 ProgramVariableStack *local_var_stack)
{
	int directive_index = stack->size - 1;
	while (true)
	{
		if (directive_index < 0)
			return;

		Directive *current_directive = &stack->data[directive_index];

		if (current_directive->type == DIRECTIVE_OPEN_PAREN)
		{

			if (!close_paren)
				return;
			if (directive_index + 1 >= stack->size)
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

		if(current_directive->type == DIRECTIVE_VAR && next_precedence == 0)
		{
			puts("movi #0");
			if(current_directive->pointer_count + current_directive->type_descriptor->pointer_count > 0)
			{
				puts("push r0");
			}
			else
			{
				for(int i = 0; i < current_directive->type_descriptor->size; i++)
				{
					puts("push r0");
				}
			}

			ProgramVariable pv = {0};
			pv.address = local_var_stack->stack_size - 1;
			pv.token = current_directive->token;
			pv.scope = local_var_stack->scope_counter;
			pv.pointer_count = current_directive->pointer_count;
			pv.type_descriptor = current_directive->type_descriptor;
			prog_var_stack_push(local_var_stack, &pv);
			directive_stack_pop(stack);
			directive_index = stack->size - 1;
			continue;
		}

		if (current_directive->type == DIRECTIVE_INT ||
			current_directive->type == DIRECTIVE_VARIABLE ||
			current_directive->type == DIRECTIVE_ADDRESS)
		{
			directive_index--;
			continue;
		}

		if (directive_type_precedence(current_directive->type) < next_precedence)
			return;

		Directive *rvalue_directive = &stack->data[stack->size - 1];
		if (rvalue_directive == current_directive)
		{
			return;
		}

		if (current_directive->type == DIRECTIVE_REF)
		{
			puts("mov r0, sp");
			printf("addi #%d\n", local_var_stack->stack_size - rvalue_directive->address);
			puts("push r0");

			Directive directive = *rvalue_directive;
			directive.ref_count = 0;
			directive.type = DIRECTIVE_ADDRESS;
			directive.location = 1;
			directive.address = local_var_stack->stack_size;
			directive.pointer_count++;
			local_var_stack->stack_size++;
			directive_stack_pop(stack);
			directive_stack_pop(stack);
			directive_stack_push(stack, &directive);
			directive_index = stack->size - 1;
			continue;
		}
		if (current_directive->type == DIRECTIVE_DEREF)
		{
			Directive directive = *rvalue_directive;
			directive.ref_count++;
			directive.pointer_count--;
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
		Directive *lvalue_directive = &stack->data[directive_index - 1];

		if (lvalue_directive->type_descriptor->primitive_type != rvalue_directive->type_descriptor->primitive_type ||
			lvalue_directive->type_descriptor->pointer_count != rvalue_directive->type_descriptor->pointer_count ||
			lvalue_directive->pointer_count != rvalue_directive->pointer_count)
		{
			puts("Types not compatible");
			return;
		}

		if (current_directive->type == DIRECTIVE_ASSIGN)
		{
			if (lvalue_directive->type == DIRECTIVE_VAR)
			{
				if (rvalue_directive->location == 0)
				{
					if (rvalue_directive->type == DIRECTIVE_INT)
					{
						printf("movi #%d\n", rvalue_directive->token->int_literal);
						puts("push r0");
						local_var_stack->stack_size++;
					}
					else
					{
						puts("mov r0, sp");
						printf("addi #%d\n", local_var_stack->stack_size - rvalue_directive->address);
						puts("ldr r2, r0");
						puts("push r2");
						local_var_stack->stack_size++;
					}
				}
				ProgramVariable pv = {0};
				pv.address = local_var_stack->stack_size - 1;
				pv.token = lvalue_directive->token;
				pv.scope = local_var_stack->scope_counter;
				pv.pointer_count = lvalue_directive->pointer_count;
				pv.type_descriptor = lvalue_directive->type_descriptor;
				prog_var_stack_push(local_var_stack, &pv);
				directive_stack_pop(stack);
				directive_stack_pop(stack);
				directive_stack_pop(stack);
				directive_index = stack->size - 1;
				continue;
			}
			if (rvalue_directive->location == 1)
			{
				puts("pop r2");
				local_var_stack->stack_size--;

				for (int i = 0; i < rvalue_directive->ref_count; i++)
				{
					puts("ldr r2, r2");
				}
			}
			else
			{
				if (rvalue_directive->type == DIRECTIVE_INT)
				{
					printf("movi #%d\n", rvalue_directive->token->int_literal);
					puts("mov r2, r0");
				}
				if (rvalue_directive->type == DIRECTIVE_VARIABLE)
				{
					puts("mov r0, sp");
					printf("addi #%d\n", local_var_stack->stack_size - rvalue_directive->address);
					puts("ldr r2, r0");
				}
				for (int i = 0; i < rvalue_directive->ref_count; i++)
				{
					puts("ldr r2, r2");
				}
			}
			if (lvalue_directive->location == 0)
			{
				puts("mov r0, sp");
				printf("addi #%d\n", local_var_stack->stack_size - lvalue_directive->address);
				puts("mov r1, r0");

				for (int i = 0; i < lvalue_directive->ref_count; i++)
				{
					puts("ldr r1, r1");
				}
			}
			if (lvalue_directive->location == 1)
			{
				puts("pop r1");
				local_var_stack->stack_size--;

				for (int i = 0; i < lvalue_directive->ref_count - 1; i++)
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

		bool operator_handled = false;
		if (current_directive->type == DIRECTIVE_ADD)
		{
			compile_add(lvalue_directive, rvalue_directive, local_var_stack);
			operator_handled = true;
		}

		if (operator_handled)
		{
			Directive directive = *lvalue_directive;
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

int type_descriptor_size(TypeDescriptor *descriptor)
{
	if (descriptor->pointer_count > 0)
		return 1;
	switch (descriptor->primitive_type)
	{
	case PRIMITIVE_TYPE_U16:
	case PRIMITIVE_TYPE_I16:
		return 1;

	case PRIMITIVE_TYPE_VOID:
		return 0;

	case PRIMITIVE_TYPE_STRUCT:
		// TODO: Calculate struct size
		return 0;

	default:
		return 1;
	}
}

bool compile_struct(TokenVector *tv, int start_index, int *last_index)
{
	StructDescriptor struct_descriptor;
	struct_descriptor_init(&struct_descriptor, 10);
	if(tv->length - start_index < 3) goto error_cleanup;
	Token *identifier_token = &tv->data[start_index + 1];
	if(tv->data[start_index].type != TOKEN_TYPE_STRUCT) goto error_cleanup;
	if(tv->data[start_index + 2].type != TOKEN_TYPE_OPEN_BRACE) goto error_cleanup;
	if(identifier_token->type != TOKEN_TYPE_IDENTIFIER) goto error_cleanup;
	start_index += 3;

	for(; start_index < tv->length; start_index++)
	{
		if(tv->data[start_index].type == TOKEN_TYPE_CLOSE_BRACE) break;

		TypeDescriptor *type_descriptor = get_type_by_name(&g_tdv, &tv->data[start_index]);
		if(!type_descriptor)
		{
			puts("Expected type name in struct definition!");
			goto error_cleanup;
		}
		start_index++;
		if(start_index >= tv->length)
		{
			puts("Unexpected end of struct definition!");
			goto error_cleanup;
		}

		int pointer_count = 0;
		for(; start_index < tv->length && tv->data[start_index].type == TOKEN_TYPE_STAR; start_index++)
		{
			pointer_count++;
		}
		if(start_index >= tv->length)
		{
			puts("Unexpected end of struct definition!");
			goto error_cleanup;
		}

		Token *name_token = &tv->data[start_index];
		if(name_token->type != TOKEN_TYPE_IDENTIFIER)
		{
			puts("Expected identifier in struct definition!");
			goto error_cleanup;
		}
		start_index++;
		if(start_index >= tv->length)
		{
			puts("Unexpected end of struct definition!");
			goto error_cleanup;
		}
		
		StructDescriptorEntry entry;
		entry.type_descriptor = type_descriptor;
		entry.entry_name = name_token->name;
		entry.offset = struct_descriptor.size;
		entry.pointer_count = pointer_count;
		sdev_push(&struct_descriptor.entries, &entry);

		struct_descriptor.size += pointer_count > 0 ? 1 : type_descriptor->size;

		if(tv->data[start_index].type == TOKEN_TYPE_SEMICOLON) continue;

		if(tv->data[start_index].type != TOKEN_TYPE_SEMICOLON)
		{
			puts("Expected closing semicolon in struct definition!");
			goto error_cleanup;
		}
	}

	TypeDescriptor type_descriptor = {0};
	type_descriptor.primitive_type = PRIMITIVE_TYPE_STRUCT;
	type_descriptor.struct_descriptor = struct_descriptor;
	type_descriptor.type_name = identifier_token->name;
	type_descriptor.size = struct_descriptor.size;
	type_desc_vector_push(&g_tdv, &type_descriptor);
	*last_index = start_index;
	return true;

	error_cleanup:
	puts("Failed to compile struct");
	struct_descriptor_free(&struct_descriptor);
	return false;
}

bool compile_tokens(TokenVector *tv, int start_index, DirectiveStack *stack, ProgramVariableStack *local_var_stack,
					int *last_index)
{
	int token_vector_index = start_index;
	for (; token_vector_index < tv->length; token_vector_index++)
	{
		int paren_count = 0;
		Token *current_token = &tv->data[token_vector_index];
		TypeDescriptor *type_descriptor = get_type_by_name(&g_tdv, current_token);
		if (type_descriptor)
		{
			int pointer_count = 0;
			token_vector_index++;
			if (token_vector_index >= tv->length)
			{
				puts("Invalid variable definition.");
				*last_index = token_vector_index;
				return false;
			}
			for(; token_vector_index < tv->length && tv->data[token_vector_index].type == TOKEN_TYPE_STAR; token_vector_index++)
			{
				pointer_count++;
			}
			if (token_vector_index >= tv->length)
			{
				puts("Invalid variable definition.");
				*last_index = token_vector_index;
				return false;
			}
			current_token = &tv->data[token_vector_index];
			if (current_token->type != TOKEN_TYPE_IDENTIFIER)
			{
				puts("Invalid variable definition. Expected identifier.");
				*last_index = token_vector_index;
				return false;
			}
			Directive directive = {0};
			directive.token = current_token;
			directive.type = DIRECTIVE_VAR;
			directive.type_descriptor = type_descriptor;
			directive.pointer_count = pointer_count;
			directive_stack_push(stack, &directive);
			continue;
		}
		if (current_token->type == TOKEN_TYPE_SEMICOLON)
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

		if (current_token->type == TOKEN_TYPE_CLOSE_PAREN)
		{
			process_directive_stack(stack, 0, true, local_var_stack);
			continue;
		}
		if (current_token->type == TOKEN_TYPE_OPEN_PAREN)
		{
			Directive directive = {0};
			directive.type = DIRECTIVE_OPEN_PAREN;
			directive.type_descriptor= get_type_by_name(&g_tdv, &(Token){.type = TOKEN_TYPE_VOID});
			directive_stack_push(stack, &directive);
			continue;
		}

		DirectiveType directive_type = DIRECTIVE_INVALID;
		switch (current_token->type)
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
			if (stack->size == 0)
			{
				directive_type = DIRECTIVE_DEREF;
				break;
			}
			if (stack->size >= 1 &&
				stack->data[stack->size - 1].type != DIRECTIVE_VARIABLE &&
				stack->data[stack->size - 1].type != DIRECTIVE_INT)
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

		if (directive_precedence != 0)
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

		if (current_token->type == TOKEN_TYPE_IDENTIFIER)
		{
			int next_next_token_index = token_vector_index + 1;
			// This is a function call
			if (next_next_token_index < tv->length && tv->data[next_next_token_index].type == TOKEN_TYPE_OPEN_PAREN)
			{
				Directive directive = {0};
				directive.token = current_token;
				directive.type = DIRECTIVE_CALL;
				directive.type_descriptor= get_type_by_name(&g_tdv, &(Token){.type = TOKEN_TYPE_VOID});
				directive_stack_push(stack, &directive);
				continue;
			}

			// This is a variable
			ProgramVariable *pv = prog_var_stack_find(local_var_stack, current_token->name);
			if (!pv)
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
			directive.type_descriptor = pv->type_descriptor;
			directive.pointer_count = pv->pointer_count;
			directive_stack_push(stack, &directive);
			continue;
		}

		if (current_token->type == TOKEN_TYPE_INTEGER_LITERAL)
		{
			Directive directive = {0};
			directive.token = current_token;
			directive.type = DIRECTIVE_INT;
			directive.type_descriptor = get_type_by_name(&g_tdv, &(Token){.type = TOKEN_TYPE_I16});
			directive_stack_push(stack, &directive);
			continue;
		}

		Directive directive = {0};
		directive.token = current_token;
		directive.type = directive_type;
		directive.type_descriptor= get_type_by_name(&g_tdv, &(Token){.type = TOKEN_TYPE_VOID});
		directive_stack_push(stack, &directive);
		continue;
	}

	if (stack->size != 0)
	{
		puts("Failed to compile expression. Some directives could not be processed");
		*last_index = token_vector_index;
		return false;
	}
	*last_index = token_vector_index;
	return true;
}

int main(int argc, const char **argv)
{
	if (argc < 2)
	{
		puts("Filepath argument missing.");
		return 1;
	}

	FILE *file = fopen(argv[1], "r");
	if (!file)
	{
		printf("Failed to open file %s\n", argv[1]);
		return 1;
	}

	TokenVector tv = {0};
	token_vector_init(&tv, 10);
	tokenize_file(file, &tv);

	type_desc_vector_init(&g_tdv);

	int last_index = 0;
	bool result = compile_struct(&tv, last_index, &last_index);
	if(!result)
	{
		return 1;
	}
	last_index++;
	if(last_index >= tv.length)
	{
		return 0;
	}
	result = compile_struct(&tv, last_index, &last_index);
	if(!result)
	{
		return 1;
	}
	last_index++;
	if(last_index >= tv.length)
	{
		return 0;
	}

	DirectiveStack stack = {0};
	stack.data = malloc(sizeof(Directive) * 100);
	ProgramVariableStack local_var_stack = {0};
	local_var_stack.data = malloc(sizeof(ProgramVariable) * 100);

	printf("Count: %d\n", tv.length);
	token_vector_print(&tv);
	while (true)
	{
		compile_tokens(&tv, last_index, &stack, &local_var_stack, &last_index);
		last_index++;
		if (last_index >= tv.length)
			break;
	}
}