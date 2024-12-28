/*
 * main.c: core of sh2 shell
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

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

static int run_process(char **args)
{
	pid_t pid;
	int status;

	pid = fork();
	if (pid == 0) {
		// Child process
		if (execvp(args[0], args) == -1) {
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
#ifdef SH2_USE_STD_GETLINE
	char *line = NULL;
	ssize_t bufsize = 0; // have getline allocate a buffer for us
	if (getline(&line, &bufsize, stdin) == -1) {
		if (feof(stdin)) {
			exit(EXIT_SUCCESS); // We received an EOF
		} else {
			perror("lsh: getline\n");
			exit(EXIT_FAILURE);
		}
	}
	return line;
#else
#define SH2_RL_BUFSIZE 1024
	int bufsize = SH2_RL_BUFSIZE;
	int position = 0;
	char *buffer = malloc(sizeof(char) * bufsize);
	int c;

	if (!buffer) {
		fprintf(stderr, "lsh: allocation error\n");
		exit(EXIT_FAILURE);
	}

	while (1) {
		// Read a character
		c = getchar();

		if (c == EOF) {
			exit(EXIT_SUCCESS);
		} else if (c == '\n') {
			buffer[position] = '\0';
			return buffer;
		} else {
			buffer[position] = c;
		}
		position++;

		// If we have exceeded the buffer, reallocate.
		if (position >= bufsize) {
			bufsize += SH2_RL_BUFSIZE;
			buffer = realloc(buffer, bufsize);
			if (!buffer) {
				fprintf(stderr, "lsh: allocation error\n");
				exit(EXIT_FAILURE);
			}
		}
	}
#endif
}

#define SH2_TOK_BUFSIZE 64
#define SH2_TOK_DELIM   " \t\r\n\a"

static char **split_line(char *line)
{
	int bufsize = SH2_TOK_BUFSIZE, position = 0;
	char **tokens = malloc(bufsize * sizeof(char *));
	char *token, **tokens_backup;

	if (!tokens) {
		fprintf(stderr, "lsh: allocation error\n");
		exit(EXIT_FAILURE);
	}

	token = strtok(line, SH2_TOK_DELIM);
	while (token != NULL) {
		tokens[position] = token;
		position++;

		if (position >= bufsize) {
			bufsize += SH2_TOK_BUFSIZE;
			tokens_backup = tokens;
			tokens = realloc(tokens, bufsize * sizeof(char *));
			if (!tokens) {
				free(tokens_backup);
				fprintf(stderr, "lsh: allocation error\n");
				exit(EXIT_FAILURE);
			}
		}

		token = strtok(NULL, SH2_TOK_DELIM);
	}
	tokens[position] = NULL;
	return tokens;
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
