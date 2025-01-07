/*
 * main.c: core of sh2 shell
 */

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "sc-collections.h"

static int builtin_cd(char **args);
static int builtin_help(char **args);
static int builtin_exit(char **args);

typedef int(command_f)(char **);

const char *const builtin_str[] = { "cd", "help", "exit" };
command_f *const builtin_func[] = { &builtin_cd, &builtin_help, &builtin_exit };

static int sh2_num_builtins()
{
	return sizeof(builtin_str) / sizeof(char *);
}

/*
  Builtin function implementations.
*/
static int builtin_cd(char **args)
{
	if (args[1] == NULL) {
		fprintf(stderr, "lsh: expected argument to \"cd\"\n");
	} else {
		if (chdir(args[1]) != 0) {
			perror("lsh");
		}
	}
	return 1;
}

static int builtin_help(char **args)
{
	int i;
	printf("sh2: a basic shell\n");
	printf("Built-in commands:\n");

	for (i = 0; i < sh2_num_builtins(); i++) {
		printf("  %s\n", builtin_str[i]);
	}
	return 1;
}

static int builtin_exit(char **args)
{
	return 0;
}

static char *find_binary(char *command)
{
	if (strchr(command, '/'))
		return strdup(command);

	const int cmdlen = strlen(command);
	const char *path = getenv("PATH");
	if (!path)
		return NULL;

	while (path) {
		const char *path_next = strchr(path, ':');
		int pathlen;
		if (path_next) {
			pathlen = path_next - path;
			path_next++;
		} else {
			pathlen = strlen(path);
		}
		int len = pathlen + cmdlen + 1;
		char *binpath = malloc(len + 1);
		memcpy(binpath, path, pathlen);
		binpath[pathlen] = '/';
		memcpy(binpath + pathlen + 1, command, cmdlen);
		binpath[len] = '\0';

		if (access(binpath, X_OK) == 0)
			return binpath;

		free(binpath);
		path = path_next;
	}
	return NULL;
}

static int run_process(char **args)
{
	pid_t pid;
	int status;

	char *binary = find_binary(args[0]);
	if (!binary) {
		fprintf(stderr, "%s: command not found\n", args[0]);
		return 1;
	}

	pid = fork();
	if (pid == 0) {
		// Child process
		if (execv(binary, args) == -1) {
			perror("lsh");
		}
		exit(EXIT_FAILURE);
	} else if (pid < 0) {
		// Error forking
		perror("lsh");
	} else {
		// Parent process
		do {
			waitpid(pid, &status, WUNTRACED);
		} while (!WIFEXITED(status) && !WIFSIGNALED(status));
	}
	free(binary);

	return 1;
}

static int run_command(char **args)
{
	int i;

	if (args[0] == NULL) {
		// An empty command was entered.
		return 1;
	}

	for (i = 0; i < sh2_num_builtins(); i++) {
		if (strcmp(args[0], builtin_str[i]) == 0) {
			return (*builtin_func[i])(args);
		}
	}

	return run_process(args);
}

static char *read_line(void)
{
	struct sc_charbuf cb;
	sc_cb_init(&cb, 128);

	while (1) {
		int c = getchar();
		if (c == EOF)
			exit(EXIT_SUCCESS);
		sc_cb_append(&cb, (char) c);
		if (c == '\n') {
			sc_cb_trim(&cb);
			return cb.buf;
		}
	}
}

struct lexer {
	struct sc_array arr;
	struct sc_charbuf tok;
	enum {
		START,
		UNQUOTED,
		UNQUOTED_ESCAPE,
		SINGLEQ,
		DOUBLEQ,
		DOUBLEQ_ESCAPE,
	} state;
};

static void lexer_init(struct lexer *lex)
{
	sc_arr_init(&lex->arr, char *, 16);
	sc_cb_init(&lex->tok, 32);
	lex->state = START;
}

static void lexer_emit(struct lexer *lex)
{
	sc_cb_trim(&lex->tok);
	sc_arr_append(&lex->arr, char *, lex->tok.buf);
	sc_cb_init(&lex->tok, 32);
}

static bool split_line(struct lexer *lex, char *line)
{
	/* Line continuation via backslash */
	if (lex->arr.len) {
		if (lex->state == UNQUOTED_ESCAPE)
			lex->state = UNQUOTED;
		else if (lex->state == DOUBLEQ_ESCAPE)
			lex->state = DOUBLEQ;
	}
	for (int i = 0; line[i]; i++) {
		switch (lex->state) {
		case START:
		case UNQUOTED:
			if (line[i] == '\'') {
				lex->state = SINGLEQ;
			} else if (line[i] == '"') {
				lex->state = DOUBLEQ;
			} else if (line[i] == '\\') {
				lex->state = UNQUOTED_ESCAPE;
			} else if (isspace(line[i])) {
				if (lex->state == UNQUOTED)
					lexer_emit(lex);
				lex->state = START;
			} else {
				sc_cb_append(&lex->tok, line[i]);
				lex->state = UNQUOTED;
			}
			break;
		case UNQUOTED_ESCAPE:
			sc_cb_append(&lex->tok, line[i]);
			lex->state = UNQUOTED;
			break;
		case SINGLEQ:
			if (line[i] == '\'')
				lex->state = UNQUOTED;
			else
				sc_cb_append(&lex->tok, line[i]);
			break;
		case DOUBLEQ:
			if (line[i] == '"')
				lex->state = UNQUOTED;
			else if (line[i] == '\\')
				lex->state = DOUBLEQ_ESCAPE;
			else
				sc_cb_append(&lex->tok, line[i]);
			break;
		case DOUBLEQ_ESCAPE:
			sc_cb_append(&lex->tok, line[i]);
			lex->state = DOUBLEQ;
			break;
		}
	}
	assert(lex->state != UNQUOTED);
	return !(lex->state == UNQUOTED || lex->state == START);
}

static void lexer_destroy(struct lexer *lex)
{
	sc_arr_destroy(&lex->arr);
	sc_cb_destroy(&lex->tok);
}

static char **get_args()
{
	struct lexer lex;
	lexer_init(&lex);
	bool again = false;
	do {
		printf("%c ", again ? '>' : '$');
		char *line = read_line();
		again = split_line(&lex, line);
		free(line);
	} while (again || !lex.arr.len);

	char **value = lex.arr.arr;
	lex.arr.arr = NULL;
	lexer_destroy(&lex);
	return value;
}

static void free_args(char **args)
{
	for (int i = 0; args[i]; i++) {
		free(args[i]);
	}
	free(args);
}

int main(int argc, char **argv)
{
	char **args;
	int status;

	do {
		args = get_args();
		status = run_command(args);
		free_args(args);
	} while (status);

	return EXIT_SUCCESS;
}
