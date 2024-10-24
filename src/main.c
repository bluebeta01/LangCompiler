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
	TypeDescriptor type_descriptor1;
	const char *entry_name;
	int offset;
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
	TypeDescriptor type_descriptor1;
	int address; // Stack pointer relative address
	int scope;	 // What scope this var is in
} ProgramVariable;

typedef struct
{
	ProgramVariable *data;
	int length;
	int stack_size;
	int scope_counter;
} ProgramVariableStack;

typedef enum NodeType
{
	NODE_CONSTANT,
	NODE_VARIABLE,
	NODE_ADD,
	NODE_SUB,
	NODE_MUL,
	NODE_REF,
	NODE_DEREF,
	NODE_DECLARE_VAR,
	NODE_COMMA,
	NODE_ASSIGN,
	NODE_SUBSCRIPT,
	NODE_END_OF_EXP,
	NODE_CALL
} NodeType;

typedef struct Node Node;
struct Node
{
	NodeType type;
	int precedence;
	Node *left;
	Node *right;
	int paren_count;
	Token *token;
	int location;
	int address;
	TypeDescriptor type_descriptor1;
	int deref_count;
};

int node_precedence(NodeType type)
{
	switch(type)
	{
		case NODE_END_OF_EXP:
		return -1;

		case NODE_ASSIGN:
		return 0;

		case NODE_ADD:
		case NODE_SUB:
		return 1;

		case NODE_MUL:
		return 2;

		case NODE_REF:
		case NODE_DEREF:
		return 3;

		case NODE_SUBSCRIPT:
		return 4;

		case NODE_CONSTANT:
		case NODE_VARIABLE:
		return 99;
	}

	return 0;
}

typedef struct
{
	Node *root_node;
	Node *active_node;
	int paren_counter;
} ExpressionParserCtx;

void parser_close_paren(ExpressionParserCtx *ctx)
{
	ctx->paren_counter++;
	if(!ctx->root_node) return;
	Node *new_active = NULL;
	Node *old_active = NULL;
	Node *iter = ctx->root_node;
	while(iter)
	{
		if(iter->paren_count > 0)
		{
			new_active = old_active;
			old_active = iter;
		}

		if (iter == ctx->active_node)
			break;

		iter = iter->right;
	}
	
	if (old_active == NULL)
	{
		ctx->active_node = NULL;
		return;
	}

	if(old_active->paren_count <= ctx->paren_counter)
	{
		ctx->active_node = new_active;
		ctx->paren_counter -= old_active->paren_count;
	}
}

void parser_append_node(ExpressionParserCtx *ctx, Node *node)
{
	if(!ctx->root_node)
	{
		ctx->root_node = node;
		if (node->paren_count > 0)
			ctx->active_node = node;
		return;
	}

	Node *replaced_node = ctx->root_node;
	Node *parent = replaced_node;
	if (ctx->active_node)
	{
		replaced_node = ctx->active_node;
		
		Node *p = NULL;
		Node *i = ctx->root_node;
		while(i && i != replaced_node)
		{
			p = i;
			i = i->right;
		}

		if(p && i == replaced_node)
			parent = p;
	}
	while(true)
	{
		if(!replaced_node->right)
			break;
		if(replaced_node->precedence >= node->precedence)
			break;
		if(replaced_node->paren_count > 0 && replaced_node != ctx->active_node)
			break;
		if (replaced_node->paren_count > 0 && ctx->paren_counter > 0)
			break;

		parent = replaced_node;
		replaced_node = replaced_node->right;
	}

	if(node->paren_count > 0 && !(replaced_node->paren_count > 0 && ctx->paren_counter > 0))
	{
		replaced_node->right = node;
		if(node->paren_count > 0) ctx->active_node = node;
		return;
	}

	if(!replaced_node->right && node->precedence > replaced_node->precedence && !(replaced_node->paren_count > 0 && replaced_node != ctx->active_node) && !(replaced_node->paren_count > 0 && ctx->paren_counter > 0))
	{
		replaced_node->right = node;
		if(node->paren_count > 0) ctx->active_node = node;
		return;
	}

	if(replaced_node == ctx->active_node)
	{
		node->paren_count += replaced_node->paren_count - ctx->paren_counter;
		replaced_node->paren_count -= replaced_node->paren_count - ctx->paren_counter;
		ctx->paren_counter = 0;
	}

	//Rotate
	Node **slot = &node->left;
	if(node->type == NODE_REF || node->type == NODE_DEREF)
		slot = &node->right;
	if(replaced_node == ctx->root_node)
	{
		if(ctx->root_node == ctx->active_node)
		{
			ctx->active_node = node;
		}
		*slot = replaced_node;
		ctx->root_node = node;
		if (node->paren_count > 0)
			ctx->active_node = node;
		return;
	}
	if(replaced_node == parent)
	{
		puts("Invalid expression");
		return;
	}
	*slot = replaced_node;
	parent->right = node;
	if(node->paren_count > 0)
		ctx->active_node = node;
}


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

//Gets the width of a type descriptor's primitive type
int type_descriptor_primitive_width(TypeDescriptor *desc)
{
	switch(desc->primitive_type)
	{
		case PRIMITIVE_TYPE_I16:
		case PRIMITIVE_TYPE_U16:
		return 1;

		case PRIMITIVE_TYPE_STRUCT:
		return desc->struct_descriptor.size;
	}

	return 0;
}

//Creates a type descriptor from tokens and returns the next index in tv
int type_descriptor_from_tokens(TokenVector *tv, int start_index, TypeDescriptor *desc)
{
	desc->pointer_count = 0;
	if (tv->data[start_index].type == TOKEN_TYPE_U16)
	{
		desc->primitive_type = PRIMITIVE_TYPE_U16;
		desc->type_name = "u16";
		desc->size = 1;
		goto count_pointers;
	}
	if (tv->data[start_index].type == TOKEN_TYPE_I16)
	{
		desc->primitive_type = PRIMITIVE_TYPE_I16;
		desc->type_name = "i16";
		desc->size = 1;
		goto count_pointers;
	}
	if (tv->data[start_index].type == TOKEN_TYPE_VOID)
	{
		desc->primitive_type = PRIMITIVE_TYPE_VOID;
		desc->type_name = "void";
		desc->size = 0;
		goto count_pointers;
	}
	if (tv->data[start_index].type == TOKEN_TYPE_IDENTIFIER)
	{
		TypeDescriptor *found = NULL;
		for(int i = 0; i < g_tdv.length; i++)
		{
			if(!strcmp(tv->data[start_index].name, g_tdv.data[i].type_name))
			{
				found = &g_tdv.data[i];
				break;
			}
		}
		if(!found)
			return start_index;

		*desc = *found;
		goto count_pointers;
	}
	return start_index;

	count_pointers:
	start_index++;
	for(; start_index < tv->length; start_index++)
	{
		if(tv->data[start_index].type == TOKEN_TYPE_STAR)
		{
			desc->pointer_count++;
			desc->size = 1;
			continue;
		}
		break;
	}

	return start_index;
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

		TypeDescriptor type_descriptor;
		int new_index = type_descriptor_from_tokens(tv, start_index, &type_descriptor);
		if(new_index == start_index)
		{
			puts("Expected type name in struct definition!");
			goto error_cleanup;
		}
		if(new_index >= tv->length)
		{
			puts("Unexpected end of struct definition!");
			goto error_cleanup;
		}
		start_index = new_index;

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
		entry.type_descriptor1 = type_descriptor;
		entry.entry_name = name_token->name;
		entry.offset = struct_descriptor.size;
		sdev_push(&struct_descriptor.entries, &entry);

		struct_descriptor.size += entry.type_descriptor1.pointer_count > 0 ? 1 : entry.type_descriptor1.size;

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

typedef struct 
{
	int stack_size;
	ProgramVariableStack prog_var_stack;
} CompilerContext;

void load_node_to_register(Node *node, CompilerContext *ctx)
{
	if(node->location == 1)
	{
		puts("pop r0");
		ctx->stack_size--;
		goto deref;
	}

	if(node->type == NODE_CONSTANT)
	{
		printf("mhi HI(#%d)\n", node->token->int_literal);
		printf("ori LO(#%d)\n", node->token->int_literal);
		goto deref;
	}

	if(node->type == NODE_VARIABLE)
	{
		printf("mhi HI(#%d)\n", ctx->stack_size - node->address);
		printf("ori LO(#%d)\n", ctx->stack_size - node->address);
		puts("add r0, sp");
		if(node->deref_count >= 0)
			puts("ldr r0, r0");
		goto deref;
	}

	deref:
	for(int i = 0; i < node->deref_count; i++)
	{
		puts("ldr r0, r0");
	}
}

int node_width(Node *node)
{
	if(node->type_descriptor1.pointer_count > 0)
		return 1;

	if(node->type_descriptor1.primitive_type == PRIMITIVE_TYPE_STRUCT)
	{
		return node->type_descriptor1.struct_descriptor.size;
	}

	return 1;
}

//Copies the value of the source node to the address of the dst node
void copy_node_to_address(Node *dst, Node *src, CompilerContext *ctx)
{
	load_node_to_register(src, ctx);
	puts("mov r1, r0");
	printf("mhi HI(#%d)\n", ctx->stack_size - dst->address);
	printf("ori LO(#%d)\n", ctx->stack_size - dst->address);
	puts("add r0, sp");
	puts("str r0, r1");
}

void emit_expression_asm(Node *node, CompilerContext *ctx)
{
	if (node->left)
		emit_expression_asm(node->left, ctx);
	if (node->right)
		emit_expression_asm(node->right, ctx);
	if(node->type == NODE_VARIABLE || node->type == NODE_CONSTANT || node->type == NODE_DECLARE_VAR)
		return;

	if(node->type == NODE_END_OF_EXP)
	{
		if(node->left && node->left->type == NODE_DECLARE_VAR)
		{
			ProgramVariable pv = {0};
			pv.address = ctx->stack_size;
			pv.token = node->left->token;
			pv.type_descriptor1 = node->left->type_descriptor1;
			prog_var_stack_push(&ctx->prog_var_stack, &pv);
		
			ctx->stack_size += node->left->type_descriptor1.size;
			puts("movi #0");
			for(int i = 0; i < node->left->type_descriptor1.size; i++)
			{
				puts("push r0");
			}

			return;
		}

		return;
	}

	if(!node->right)
	{
		puts("Operator requires rvalue");
		return;
	}

	if(node->type == NODE_REF)
	{
		*node = *node->right;
		node->deref_count--;
		return;
	}

	if(node->type == NODE_DEREF)
	{
		*node = *node->right;
		node->deref_count++;
		return;
	}

	if(!node->left || !node->right)
	{
		puts("Operator requires lvalue and rvalue");
		return;
	}

	if(node->type == NODE_ASSIGN)
	{
		if(node->left->type == NODE_DECLARE_VAR)
		{
			load_node_to_register(node->right, ctx);
			puts("push r0");
			node->location = 1;

			ProgramVariable pv = {0};
			pv.address = ctx->stack_size;
			pv.token = node->left->token;
			pv.type_descriptor1 = node->left->type_descriptor1;
			prog_var_stack_push(&ctx->prog_var_stack, &pv);

			ctx->stack_size++;
			return;
		}

		if(node->right->type_descriptor1.pointer_count > node->right->deref_count || (node->right->type_descriptor1.pointer_count == node->right->deref_count && type_descriptor_primitive_width(&node->right->type_descriptor1) == 1))
		{
			load_node_to_register(node->right, ctx);
			puts("mov r1, r0");
			node->left->deref_count--;
			load_node_to_register(node->left, ctx);
			puts("str r0, r1");
			return;
		}

		return;
	}

	if(node->type == NODE_SUBSCRIPT)
	{
		node->type_descriptor1 = node->left->type_descriptor1;

		load_node_to_register(node->right, ctx);
		puts("mov r1, r0");
		printf("mhi HI(#%d)\n", node->left->type_descriptor1.size);
		printf("ori LO(#%d)\n", node->left->type_descriptor1.size);
		puts("mul r1, r0");
		load_node_to_register(node->left, ctx);
		puts("add r0, r1");
		puts("push r0");
		node->location = 1;
		node->deref_count++;
		ctx->stack_size++;
		return;
	}

	if(node->type == NODE_ADD)
	{
		//TODO: Do some sane type checking here
		int pointer_count = node->left->type_descriptor1.pointer_count + node->right->type_descriptor1.pointer_count;
		node->type_descriptor1 = node->left->type_descriptor1;
		node->type_descriptor1.pointer_count = pointer_count;

		load_node_to_register(node->right, ctx);
		puts("mov r1, r0");
		load_node_to_register(node->left, ctx);
		puts("add r0, r1");
		puts("push r0");
		node->location = 1;
		ctx->stack_size++;
		return;
	}
}

bool compile_tokens_nodes(TokenVector *tv, int start_index, ExpressionParserCtx *parser_ctx, CompilerContext *compiler_ctx,
					int *last_index)
{
	int paren_count = 0;
	int token_vector_index = start_index;
	for (; token_vector_index < tv->length; token_vector_index++)
	{
		Token *current_token = &tv->data[token_vector_index];
		TypeDescriptor type_descriptor;
		int new_index = type_descriptor_from_tokens(tv, token_vector_index, &type_descriptor);
		if (new_index > token_vector_index && new_index < tv->length)
		{
			Node *node = malloc(sizeof(Node));
			*node = (Node)
			{
				.token = &tv->data[new_index],
				.type = NODE_DECLARE_VAR,
				.type_descriptor1 = type_descriptor,
				.paren_count = paren_count
			};
			node->precedence = node_precedence(node->type);
			parser_append_node(parser_ctx, node);
			paren_count = 0;
			token_vector_index = new_index;
			continue;
		}
		if (current_token->type == TOKEN_TYPE_SEMICOLON)
		{
			Node *node = malloc(sizeof(Node));
			*node = (Node)
			{
				.token = &tv->data[token_vector_index],
				.type = NODE_END_OF_EXP,
				.paren_count = paren_count
			};
			node->precedence = node_precedence(node->type);
			parser_append_node(parser_ctx, node);
			paren_count = 0;
			*last_index = token_vector_index;
			return true;
		}
		if (current_token->type == TOKEN_TYPE_CLOSE_PAREN || current_token->type == TOKEN_TYPE_CLOSE_BRACKET)
		{
			parser_close_paren(parser_ctx);
			continue;
		}
		if (current_token->type == TOKEN_TYPE_OPEN_PAREN)
		{
			paren_count++;
			continue;
		}
		if(current_token->type == TOKEN_TYPE_OPEN_BRACKET)
		{
			Node *node = malloc(sizeof(Node));
			*node = (Node)
			{
				.token = current_token,
				.type = NODE_SUBSCRIPT,
				.paren_count = paren_count
			};
			node->precedence = node_precedence(node->type);
			paren_count++;
			parser_append_node(parser_ctx, node);
			continue;
		}
		if(current_token->type == TOKEN_TYPE_PLUS)
		{
			Node *node = malloc(sizeof(Node));
			*node = (Node)
			{
				.token = current_token,
				.type = NODE_ADD,
				.paren_count = paren_count
			};
			node->precedence = node_precedence(node->type);
			parser_append_node(parser_ctx, node);
			paren_count = 0;
			continue;
		}
		if(current_token->type == TOKEN_TYPE_MINUS)
		{
			Node *node = malloc(sizeof(Node));
			*node = (Node)
			{
				.token = current_token,
				.type = NODE_SUB,
				.paren_count = paren_count
			};
			node->precedence = node_precedence(node->type);
			parser_append_node(parser_ctx, node);
			paren_count = 0;
			continue;
		}
		if(current_token->type == TOKEN_TYPE_STAR)
		{
			Node *node = malloc(sizeof(Node));
			*node = (Node)
			{
				.token = current_token,
				.type = NODE_DEREF,
				.paren_count = paren_count
			};
			node->precedence = node_precedence(node->type);
			parser_append_node(parser_ctx, node);
			paren_count = 0;
			continue;
		}
		if(current_token->type == TOKEN_TYPE_AMP)
		{
			Node *node = malloc(sizeof(Node));
			*node = (Node)
			{
				.token = current_token,
				.type = NODE_REF,
				.paren_count = paren_count
			};
			node->precedence = node_precedence(node->type);
			parser_append_node(parser_ctx, node);
			paren_count = 0;
			continue;
		}
		if(current_token->type == TOKEN_TYPE_COMMA)
		{
			Node *node = malloc(sizeof(Node));
			*node = (Node)
			{
				.token = current_token,
				.type = NODE_COMMA,
				.paren_count = paren_count
			};
			node->precedence = node_precedence(node->type);
			parser_append_node(parser_ctx, node);
			paren_count = 0;
			continue;
		}
		if(current_token->type == TOKEN_TYPE_INTEGER_LITERAL)
		{
			Node *node = malloc(sizeof(Node));
			*node = (Node)
			{
				.token = current_token,
				.type = NODE_CONSTANT,
				.paren_count = paren_count,
				.type_descriptor1 = (TypeDescriptor)
				{
					.primitive_type = PRIMITIVE_TYPE_I16,
					.size = 1
				}
			};
			node->precedence = node_precedence(node->type);
			parser_append_node(parser_ctx, node);
			paren_count = 0;
			continue;
		}
		if(current_token->type == TOKEN_TYPE_EQUALS)
		{
			Node *node = malloc(sizeof(Node));
			*node = (Node)
			{
				.token = current_token,
				.type = NODE_ASSIGN,
				.paren_count = paren_count
			};
			node->precedence = node_precedence(node->type);
			parser_append_node(parser_ctx, node);
			paren_count = 0;
			continue;
		}
		if(current_token->type == TOKEN_TYPE_IDENTIFIER)
		{
			ProgramVariable *pv = prog_var_stack_find(&compiler_ctx->prog_var_stack, current_token->name);
			if (!pv)
			{
				puts("Could not find variable.");
				*last_index = token_vector_index;
				return false;
			}

			Node *node = malloc(sizeof(Node));
			*node = (Node)
			{
				.token = current_token,
				.type = NODE_VARIABLE,
				.address = pv->address,
				.type_descriptor1 = pv->type_descriptor1,
				.paren_count = paren_count
			};
			node->precedence = node_precedence(node->type);
			parser_append_node(parser_ctx, node);
			paren_count = 0;

			continue;
		}

		puts("Invalid operator");
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

	printf("Count: %d\n", tv.length);
	token_vector_print(&tv);

	CompilerContext compiler_ctx = {0};
	compiler_ctx.prog_var_stack.data = malloc(sizeof(ProgramVariable) * 100);

	while (true)
	{
		//compile_tokens(&tv, last_index, &stack, &local_var_stack, &last_index);
		ExpressionParserCtx ctx = {0};
		bool result = compile_tokens_nodes(&tv, last_index, &ctx, &compiler_ctx, &last_index);
		if (!result)
			break;
		
		emit_expression_asm(ctx.root_node, &compiler_ctx);
		last_index++;
		if (last_index >= tv.length)
			break;
	}
}