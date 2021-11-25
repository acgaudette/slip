#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>

const char escape  = '$';
const char vec_beg = '[';
const char vec_end = ']';
const char eol_def = ';';

/* test code

$ dot up + [1 2 3] [4 5 6]
$ + cam.pos * app cam.rot fwd * * dt axis config.speed
$ mix pos_last cam.pos'3 * dt config.damp
$ mix 2. zero -.5
$ * * a' b c
$ [ sin cos 1 ]
$ sin cos 1 2
$ 0 : 3, + 1 a,
$ + 5 (2*2)
$ *q ~rot axis-angle "" -..2theta[5]%%
$ one'4
$ mix'4 a b 1
$ +'2 c d

*/

static char eol;

__attribute__((noreturn))
static void panic()
{
	printf("Exit failure\n");
	fflush(stdout);
	abort();
}

#define ARG_MAX 4
typedef struct {
	enum sym {
		  SYM_NONE
		, SYM_FN
		, SYM_CONST
		, SYM_MACRO
		, SYM_VAR
		, SYM_LIT
		, SYM_VEC
		, SYM_VEC_TAIL
	} type;

	int builtin;
	union {
		char op;
		char *in;
	};

	union {
		size_t n;
		size_t returns;
	};

	union {
		char *real;
		void *elem[4];
		struct {
			int poly;
			union {
				char *out;
				struct {
					const char *out_1;
					const char *out_2;
					const char *out_3;
					const char *out_4;
				};
			};

			size_t n_int;
			size_t n_param;
			size_t params[ARG_MAX];
			void *args[ARG_MAX];
		};
	};
} sym;

static const char *sym_type_str(enum sym type)
{
	switch (type) {
	case SYM_NONE:
		return "SYM_NONE";
	case SYM_FN:
		return "SYM_FN";
	case SYM_CONST:
		return "SYM_CONST";
	case SYM_MACRO:
		return "SYM_MACRO";
	case SYM_VAR:
		return "SYM_VAR";
	case SYM_LIT:
		return "SYM_LIT";
	case SYM_VEC:
		return "SYM_VEC";
	case SYM_VEC_TAIL:
		return "SYM_VEC_TAIL";
	default:
		panic();
	}
}

static void sym_print(sym sym)
{
	if (sym.builtin)
		fprintf(stderr, "%s \"%c\"", sym_type_str(sym.type), sym.op);
	else if (sym.in)
		fprintf(stderr, "%s \"%s\"", sym_type_str(sym.type), sym.in);
	else
		fprintf(stderr, "%s", sym_type_str(sym.type));

	switch (sym.type) {
	case SYM_FN:
		fprintf(stderr, " (");
		for (size_t i = 0; i < sym.n_param; ++i) {
			if (sym.params[i])
				fprintf(stderr, " %lu", sym.params[i]);
			else
				fprintf(stderr, " generic");
		}
		fprintf(stderr, " )");
		if (sym.n)
			fprintf(stderr, " returns type:%lu", sym.n);
		else
			fprintf(stderr, " returns generic");
		break;
	case SYM_CONST:
	case SYM_MACRO:
		if (sym.n)
			fprintf(stderr, " type:%lu", sym.n);
		else
			fprintf(stderr, " generic");
		break;
	case SYM_VAR:
		if (sym.n)
			fprintf(stderr, " annotated type:%lu", sym.n);
		else
			fprintf(stderr, " unknown type");
		break;
	case SYM_LIT:
		fprintf(stderr, " %s", sym.real);
		break;
	default:
		break;
	}
}

#define SLIP_GENERIC 0
static sym key[] = {
	#include "slip.binds"
};

typedef struct {
	enum token {
		  TOK_NONE
		, TOK_REAL
		, TOK_IDEN
		, TOK_OP
		, TOK_VEC_BEG
		, TOK_VEC_END
		, TOK_EOL
	} type;

	const char *beg, *end;
	size_t len, n;
	char *str;

	union {
		double real;
		char op;
	};
} token;

static const char *token_type_str(enum token type)
{
	switch (type) {
	case TOK_NONE:
		return "TOK_NONE";
	case TOK_REAL:
		return "TOK_REAL";
	case TOK_IDEN:
		return "TOK_IDEN";
	case TOK_OP:
		return "TOK_OP";
	case TOK_VEC_BEG:
		return "TOK_VEC_BEG";
	case TOK_VEC_END:
		return "TOK_VEC_END";
	case TOK_EOL:
		return "TOK_EOL";
	default:
		panic();
	}
}

static void token_print(token token)
{
	fprintf(
		stderr,
		"%s \"%.*s\"",
		token_type_str(token.type),
		(int)(token.end - token.beg),
		token.beg
	);

	switch (token.type) {
	case TOK_REAL:
		fprintf(stderr, " %f", token.real);
		break;
	case TOK_IDEN:
		fprintf(stderr, " <%s>", token.str);
		break;
	default:
		break;
	}
}

static int is_unary(const char c)
{
	switch (c) {
	case '-':
	case '+':
	case '~':
		return 1;
	default:
		return 0;
	}
}

static int is_punct(const char c)
{
	switch (c) {
	case ',':
	case ':':
	case ';':
		return 1;
	default:
		return 0;
	}
}

static token lex(const char **in)
{
	assert(**in != escape);
	token token = {};

	switch (**in) {
	case escape:
		fprintf(stderr, "bad char: equals escape (%c)\n", escape);
		panic();
	case vec_beg:
		token.type = TOK_VEC_BEG;
		token.beg = (*in)++;
		token.end = *in;
		return token;
	case vec_end:
		token.type = TOK_VEC_END;
		token.beg = (*in)++;
		token.end = *in;
		return token;
	case ',':
	case ':':
	case ';':
		token.type = TOK_EOL;
		token.op  = **in;
		token.beg = (*in)++;
		token.end =  *in;
		return token;
	default:
		break;
	}

	size_t n_dot = 0;

	switch (**in) {
	case '`':
	case '~':
	case '+':
	case '-':
	case '_':
	case '*':
	case '/':
	case '!':
	case '@':
	case '#':
	case '$':
	case '%':
	case '^':
	case '&':
	case '?':
	case ':':
	case ';':
	case ',':
	case '.':
	case '<':
	case '>':
	case '[':
	case ']':
	case '(':
	case ')':
	case '{':
	case '}':
	case '"':
	case '\'': {
		char next = *(*in + 1);
		if (isspace(next) || '\'' == next) {
			token.type = TOK_OP;
			token.op  = **in;
			token.beg = (*in)++;

			if (**in == '\'') {
				++(*in);

				int n = 1;
				if (isdigit(**in)) {
					n = atoi(*in);
					assert(n);
					++(*in);
				}

				token.n = n;
			}

			token.end =  *in;
			return token;
		} else if ('.' == **in) {
			++n_dot;
			if (isdigit(next))
				goto literal;
		} else if (is_unary(**in)) {
			if (isdigit(next))
				goto literal;
			else if ('.' == next) {
				next = *(*in + 2);
				if (isdigit(next))
					goto literal;
			}
		}
	}
	case 'a':
	case 'b':
	case 'c':
	case 'd':
	case 'e':
	case 'f':
	case 'g':
	case 'h':
	case 'i':
	case 'j':
	case 'k':
	case 'l':
	case 'm':
	case 'n':
	case 'o':
	case 'p':
	case 'q':
	case 'r':
	case 's':
	case 't':
	case 'u':
	case 'v':
	case 'w':
	case 'x':
	case 'y':
	case 'z':
	case 'A':
	case 'B':
	case 'C':
	case 'D':
	case 'E':
	case 'F':
	case 'G':
	case 'H':
	case 'I':
	case 'J':
	case 'K':
	case 'L':
	case 'M':
	case 'N':
	case 'O':
	case 'P':
	case 'Q':
	case 'R':
	case 'S':
	case 'T':
	case 'U':
	case 'V':
	case 'W':
	case 'X':
	case 'Y':
	case 'Z':
		token.type = TOK_IDEN;
		token.beg = (*in)++;

		size_t neg = 0;
		while (!isspace(**in)) {
			if (is_punct(**in))
				break;
			if (**in == '\'') {
				++(*in);
				++neg;

				int n = 1;
				if (isdigit(**in)) {
					n = atoi(*in);
					assert(n);
					++(*in);
					++neg;
				}

				token.n = n;
				break;
			}

			++(*in);
		}
		token.end = *in;

		token.len = token.end - token.beg;
		assert(token.len >= neg);
		token.len -= neg;

		token.str = malloc(token.len + 1);
		assert(token.str);
		strncpy(token.str, token.beg, token.len);
		token.str[token.len] = 0;
		return token;
	literal :
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
		token.type = TOK_REAL;
		token.beg = (*in)++;

		while (isdigit(**in) || **in == '.') {
			n_dot += '.' == **in;
			assert(n_dot <= 1);
			++(*in);
		}

		token.end = *in;

		errno = 0;
		token.real = strtod(token.beg, NULL);
		if (errno) {
			perror("strtod");
			panic();
		}

		token.len = token.end - token.beg;
		token.str = malloc(token.len + 1);
		assert(token.str);
		strncpy(token.str, token.beg, token.len);
		return token;
	case '\t':
	case '\v':
	case '\f':
	case '\r':
	case ' ':
		++*in;
		return lex(in);
	case 0:
	case '\n':
		token.type = TOK_EOL;
		token.op  = **in;
		token.beg = (*in)++;
		token.end =  *in;
		return token;
	default:
		fprintf(stderr, "bad char: \"%c\" (%u)\n", **in, **in);
		panic();
	}
}

static sym *sym_find_op(const token token)
{
	for (size_t i = 0; i < sizeof(key) / sizeof(sym); ++i) {
		sym *sym = key + i;
		switch (sym->type) {
		case SYM_FN:
			if (!sym->builtin)
				continue;
			break;
		case SYM_CONST:
		case SYM_MACRO:
			continue;
		default:
			panic();
		}

		if (sym->op != token.op)
			continue;
		return sym;
	}

	return NULL;
}

static sym *sym_find_iden(const token token)
{
	for (size_t i = 0; i < sizeof(key) / sizeof(sym); ++i) {
		sym *sym = key + i;
		switch (sym->type) {
		case SYM_FN:
			if (sym->builtin)
				continue;
			break;
		case SYM_CONST:
		case SYM_MACRO:
			break;
		default:
			panic();
		}

		if (0 != strncmp(sym->in, token.str, token.len))
			continue;
		if (token.len != strlen(sym->in))
			continue;
		return sym;
	}

	return NULL;
}

static void err(const token token)
{
	fprintf(stderr, "symbol unrecognized:\n\t");
	token_print(token);
	printf("\n");
	panic();
}

static sym symbolize(const char **in)
{
	token token = lex(in);
#ifdef SL_DUMP_TOK
	token_print(token);
	fprintf(stderr, "\n");
#endif
	sym result = {};
	sym *src;

	switch (token.type) {
	case TOK_OP:
		src = sym_find_op(token);
		if (!src)
			err(token);
		result = *src;
		result.n = result.n ?: token.n;
		result.n_int = result.n_int ?: token.n;
		return result;
	case TOK_IDEN:
		src = sym_find_iden(token);
		if (!src) {
			result.type = SYM_VAR;
			result.in = result.out = token.str;
			result.n = token.n;
			return result;
		}

		result = *src;
		result.n = result.n ?: token.n;
		result.n_int = result.n_int ?: token.n;
		return result;
	case TOK_REAL:
		result.type = SYM_LIT;
		result.real = token.str;
		result.n = 1;
		return result;
	case TOK_VEC_BEG:
		result.type = SYM_VEC;
		return result;
	case TOK_VEC_END:
		result.type = SYM_VEC_TAIL;
		return result;
	case TOK_EOL:
		eol = eol ?: token.op;
		return result;
	default:
		panic();
	}
}

static void check_arg(sym *fn, size_t i, sym *arg)
{
	size_t *n_param = fn->params + i;

	if (!*n_param && fn->n_int)
		*n_param = fn->n_int;

	if (*n_param && arg->n) {
		if (arg->n != *n_param) {
			fprintf(
				stderr,
				"argument \"%s\" type: v%lu\n"
				"does not match function \"%s\" "
				"parameter %lu type: v%lu\n",
				arg->builtin ? "builtin" : arg->in,
				arg->n,
				fn->builtin ? "builtin" : fn->in,
				i + 1,
				*n_param
			);
			panic();
		}
	}

	else if (*n_param && !arg->n) {
		arg->n = *n_param;
	}

	else if (!*n_param && arg->n) {
		assert(!fn->n_int);
		*n_param = arg->n;
		fn->n_int = arg->n;

		if (!fn->n)
			fn->n = fn->n_int;
	}
}

static void parse_sym(sym *node, const char **in)
{
	int infer = 0;
	switch (node->type) {
	case SYM_FN:
		for (size_t i = 0; i < node->n_param; ++i) {
			if (node->params[i])
				continue;
			infer = 1;
			break;
		}

		for (size_t i = 0; i < node->n_param; ++i) {
			node->args[i] = malloc(sizeof(sym));
			sym *arg = node->args[i];
			assert(arg);

			*arg = symbolize(in);
#ifdef SL_DUMP_SYM
			sym_print(*arg);
			fprintf(stderr, "\n");
#endif
			parse_sym(arg, in);
			check_arg(node, i, arg);
			// node->n_int may still be zero at this point
		}

		if (!node->n_int && infer) {
			if (node->builtin)
				fprintf(
					stderr,
					"type of function \"%c\" "
					"could not be inferred\n",
					node->op
				);
			else
				fprintf(
					stderr,
					"type of function \"%s\" "
					"could not be inferred\n",
					node->in
				);
			panic();
		}

		for (size_t i = 0; i < node->n_param; ++i) {
			sym *arg = node->args[i];
			assert(arg);

			if (!arg->n) {
				arg->n = node->n_int;
				node->params[i] = node->n_int;
			}
		}

		break;
	case SYM_VEC: {
		size_t i = 0;
		do {
			if (i >= 4) {
				fprintf(
					stderr,
					"maximum vector length exceeded\n"
				);
			}

			node->elem[i] = malloc(sizeof(sym));
			sym *elem = node->elem[i];
			assert(elem);
			*elem = symbolize(in);
#ifdef SL_DUMP_SYM
			sym_print(*elem);
			fprintf(stderr, "\n");
#endif
			if (!elem->type || elem->type == SYM_VEC_TAIL)
				break;
			parse_sym(elem, in);

			if (!elem->n) {
				fprintf(
					stderr,
					"vector element %lu length unknown\n",
					i + 1
				);
				panic();
			}

			i += elem->n;
		} while (1);

		node->n = i;
		break;
	}
	case SYM_CONST:
	case SYM_MACRO:
	case SYM_VAR:
	case SYM_LIT:
	case SYM_NONE:
		return;
	default:
		panic();
	}
}

static const sym *translate(const sym *node)
{
	const sym *tail = node;
	switch (node->type) {
	case SYM_FN:
		switch (node->n_int) {
		case 0:
			assert(!node->poly);
			printf("%s", node->out);
			break;
		case 1:
			if (!node->builtin) {
				assert(node->out_1);
				printf("%s", node->out_1);
			}
			break;
		case 2:
			assert(node->out_2);
			printf("%s", node->out_2);
			break;
		case 3:
			assert(node->out_3);
			printf("%s", node->out_3);
			break;
		case 4:
			assert(node->out_4);
			printf("%s", node->out_4);
			break;
		}

		printf("(");

		if (node->builtin) {
			switch (node->n_param) {
			case 1:
				if (node->n_int == 1)
					printf("%s", node->out_1);
				break;
			case 2:
				break;
			default:
				panic();
			}
		}

		for (size_t i = 0; i < node->n_param; ++i) {
			sym *arg = (sym*)node->args[i];
			assert(arg);
			translate(arg);

			if (!i && node->builtin && node->n_int == 1) {
				if (2 == node->n_param) {
					printf(" %s ", node->out_1);
					continue;
				}
			}

			if (i < node->n_param - 1)
				printf(", ");
		}

		printf(")");
		break;
	case SYM_CONST:
		if (!node->poly) {
			printf("%s", node->out);
			break;
		}

		switch (node->n) {
		case 0:
			fprintf(
				stderr,
				"cannot infer type of constant \"%s\"\n",
				node->in
			);
			panic();
		case 1:
			assert(node->out_1);
			printf("%s", node->out_1);
			break;
		case 2:
			assert(node->out_2);
			printf("%s", node->out_2);
			break;
		case 3:
			assert(node->out_3);
			printf("%s", node->out_3);
			break;
		case 4:
			assert(node->out_4);
			printf("%s", node->out_4);
			break;
		}
		break;
	case SYM_MACRO:
		assert(!node->poly);
		assert(node->n);
		assert(node->out);
		printf("%s", node->out);
		break;
	case SYM_VAR:
		printf("%s", node->out);
		break;
	case SYM_LIT:
		printf("%s", node->real);
		break;
	case SYM_VEC:
		assert(node->n);
		sym *fst = (sym*)node->elem[0];
		assert(fst);

		if (fst->n == node->n) {
			translate(fst);
			break;
		}

		switch (node->n) {
		case 2:
			printf("(ff) { ");
			break;
		case 3:
			printf("(v3) { ");
			break;
		case 4:
			printf("(v4) { ");
			break;
		default:
			panic();
		}

		for (size_t i = 0; i < node->n;) {
			sym *elem = (sym*)node->elem[i];
			assert(elem);
			translate(elem);
#ifndef SL_TRAILING_COMMA
			if (i < node->n - 1)
#endif
			printf(", ");
			i += elem->n;
			assert(1 == elem->n); // Temporary
		}

		printf(" }");
		break;
	case SYM_NONE:
	default:
		panic();
	}

	return tail;
}

static void parse(const char *in, const size_t n_line, const size_t n_col)
{
	eol = 0;
	const char *start = in;
	fprintf(stderr, "parse %lu:%lu\n\t%s", n_line, n_col, in);

	sym root = { .type = SYM_VEC };
#ifdef SL_DUMP_SYM
	sym_print(root);
	fprintf(stderr, "\n");
#endif
	parse_sym(&root, &in);
	const sym *tail = translate(&root);

	const char *rem = in;
	sym next = symbolize(&in);

	if (next.type != SYM_NONE) {
		switch (eol) {
		case 0:
			break;
		case ':':
			if (1) printf(" %c ", eol);
			else
		default:
			printf("%c ", eol);
			return parse(rem, n_line, n_col + (rem - start));
		}

		if (tail->builtin)
			fprintf(
				stderr,
				"\"%c\" is not a recognized function\n",
				tail->op
			);
		else
			fprintf(
				stderr,
				"\"%s\" is not a recognized function\n",
				tail->in
			);
		panic();
	}

	switch (eol) {
	case 0:
	case '\n':
		printf("%c", eol_def);
		break;
	case ':':
		printf(" ");
	default:
		printf("%c", eol);
	}

#ifdef SL_PARSE_ONLY
	printf("\n");
#endif
}

int main(int argc, char **argv)
{
	const char *path = *(argv + 1) ?: "main.c";
	fprintf(stderr, "compile \"%s\"\n", path);

	FILE *file = fopen(path, "r");
	if (!file) {
		perror("fopen");
		panic();
	}

	size_t len = 0;
	ssize_t n;
	char *line;

	size_t n_line = 0;
	while (-1 != (n = getline(&line, &len, file))) {
		int string = 0;
		int lit = 0;

		++n_line;
		for (char *c = line; *c != '\n'; ++c) {
			switch (*c) {
			case '"':
				string = !string;
				break;
			case '\'':
				lit = !lit;
				break;
			default:
				break;
			}

			if (*c != escape || (string | lit)) {
#ifndef SL_PARSE_ONLY
				printf("%c", *c);
#endif
				continue;
			}

			parse(c + 1, n_line, (c + 1) - line);
			break;
		}
#ifndef SL_PARSE_ONLY
		printf("\n");
#endif
	}

	free(line);
	fclose(file);

	fprintf(stderr, "OK\n");
}
