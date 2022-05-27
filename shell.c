#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <linux/limits.h>
#include <signal.h>

#define NAME "shell"
#define VERSION "1"

#define ERR_ALLOC_ER "allocation error\n"
#define TOK_DELIM " \t\r\n\a"

#define RL_BUFSIZE 1024
#define TOK_BUFSIZE 64
#define GH_BUFSIZE 64
#define REPL_BUFSIZE 128

char *read_line(void);
char **split_line(char *line);
void main_loop(void);

int launch(char **args);
int execute(char **args);
int launch(char **args);

int num_builtins();
int builtin_cd(char **args);
int builtin_help(char **args);
int builtin_exit(char **args);
void ctrl_c_handler(int signal);

char cwd[PATH_MAX];

// List of the names of builtin commands
char *builtin_str[] = {
    "cd",
    "help",
    "exit"
};

// List of the builtin functions
int(*builtin_func[]) (char **) = {
    &builtin_cd,
    &builtin_help,
    &builtin_exit
};

int main(int argc, char *argv []) {
    // SIGINT is the signal sent when the user clicks ctrl c
    // Call c ctrl_c_handler when signal SIGINT is sent
    signal(SIGINT, ctrl_c_handler);
    main_loop();
    return 0;
}

void main_loop(void) {
    char *line;
    char **args;
    int status;
    char $[4] = " # \0";
    // If not root
    if (geteuid() != 0) {
        $[0] = ' ';
        $[1] = '$';
        $[2] = ' ';
        $[3] = '\0';
    }
    do {
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            strcat(cwd, $);
            printf("%s", cwd);
            line = read_line();
        }
        args = split_line(line);
        status = execute(args);
        free(line);
        free(args);
    } while (status);
}

char *read_line(void) {
    int bufsize = RL_BUFSIZE;
    int position = 0;
    char *buffer = malloc(sizeof(char) * bufsize);
    // The reason c in of tye int is because you can check end of file. You cannot do that with char
    int c;

    if (!buffer) {
        // If failed to allocate memory with malloc
        fprintf(stderr, ERR_ALLOC_ER);
        exit(EXIT_FAILURE);
    }   
    while (1) {
        c = getchar();
        if (c == EOF || c == '\n') {
            // If we are at end of file or newline; add null char (\0) and return the string
            buffer[position] = '\0';
            return buffer;
        }
        else {
            // Else; add the char to the buffer
            buffer[position] = c;
        }
        position++;

        // If the buffer size is reached
        if (position >= bufsize) {
            // Make the buffer size bigger
            bufsize += RL_BUFSIZE;
            // Reallocate the buffer with the new buffer size
            buffer = realloc(buffer, bufsize);
            // Check if the reallocation was successful
            if (!buffer) {
                fprintf(stderr, ERR_ALLOC_ER);
                exit(EXIT_FAILURE);
            }
        }
    }
}

char **split_line(char *line) {
    // Splits a line into an array of tokens. "one two three" -> ["one", "two", "three"]
    int bufsize = TOK_BUFSIZE;
    int position = 0;
    // The array that will be returned
    char **tokens = malloc(bufsize * sizeof(char*));
    char *token;
    
    if (!tokens) {
        // If failed to allocate memory with malloc
        fprintf(stderr, ERR_ALLOC_ER);
        exit(EXIT_FAILURE);
    }   
    // Split the line
    token = strtok(line, TOK_DELIM);
    while (token != NULL) {
        // Add the token to the tokens array
        tokens[position] = token;
        position++;

        // If the buffer size is reached
        if (position >= bufsize) {
            // Expand the buffer sizes
            bufsize += TOK_BUFSIZE;
            // Reallocate the array
            tokens = realloc(tokens, bufsize * sizeof(char*));
            // Check for allocation errors
            if (!tokens) {
                fprintf(stderr, ERR_ALLOC_ER);
                exit(EXIT_FAILURE);
            }
        }

        token = strtok(NULL, TOK_DELIM);
    }
    // Null terminate
    tokens[position] = NULL;
    return tokens;
}

int builtin_cd(char **args) {
    if (args[1] == NULL) {
        // If no argument after cd
        char err[50] = NAME;
        strcat(err, ": expected argument to \"cd\"\n");
        fprintf(stderr, err);
    }

    else {
        if (chdir(args[1]) != 0) {
            perror(NAME);
        }
    }
    return 1;
}

int builtin_help(char **args) {
    printf("%s%s%s%s", NAME, " version ", VERSION, "\n");
    return 1;
}

int builtin_exit(char **args) {
    return 0;
}

int num_builtins() {
    // Returns the amount of builtin commands
    return sizeof(builtin_str) / sizeof(char *);
}

int execute(char **args) {
    int i;
    if (args[0] == NULL) {
        // If command is empty
        return 1;
    }
    
    for (i = 0; i < num_builtins(); i++) {
        if (strcmp(args[0], builtin_str[i]) == 0) {
            // Strcmp returns 0 if 2 strings match
            // So if first arg matches a builtin
            // Call the builtin with args
            return (*builtin_func[i])(args);
        }
    }
    // Run a regular process
    return launch(args);
}

int launch(char **args) {
    pid_t pid; 
    pid_t wpid;
    int status; 
    pid = fork();
    if (pid == 0) {
        // Child process
        if (execvp(args[0], args) == -1) {
            // If execvp returns -1; print error
            perror(NAME);
        }
        exit(EXIT_FAILURE);
    }
    else if (pid < 0) {
        // If fork() had an error
        perror(NAME);
    }
    else {
      // Parent process
      do {
          wpid = waitpid(pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }   
    return 1;
}

void ctrl_c_handler(int _signal) {
    // Just print the prompt if ctrl c was clicked
    printf("\n%s", cwd);
}