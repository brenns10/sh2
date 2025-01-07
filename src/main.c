/*
 * main.c: core of sh2 shell
 */

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

		if (c == EOF) {
			exit(EXIT_SUCCESS);
		} else if (c == '\n') {
			sc_cb_trim(&cb);
			return cb.buf;
		} else {
			sc_cb_append(&cb, (char) c);
		}
	}
}

#define SH2_TOK_DELIM   " \t\r\n\a"

static char **split_line(char *line)
{
	struct sc_array arr;
	sc_arr_init(&arr, char *, 16);
	char *token;

	token = strtok(line, SH2_TOK_DELIM);
	while (token != NULL) {
		sc_arr_append(&arr, char *, token);
		token = strtok(NULL, SH2_TOK_DELIM);
	}
	sc_arr_append(&arr, char *, NULL);
	return arr.arr;
}

int main(int argc, char **argv)
{
	char *line;
	char **args;
	int status;

	do {
		printf("> ");
		line = read_line();
		args = split_line(line);
		status = run_command(args);

		free(line);
		free(args);
	} while (status);

	return EXIT_SUCCESS;
}
