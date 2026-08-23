#include <stdio.h>
#include <stdlib.h>

/* If we are compiling in windows, compile these functions*/
#ifdef _WIN32
#include <string.h>

#define SIZE 2048

static char buffer[SIZE];

/*Fake readline*/

char* readline(char* prompt) {
    fputs(prompt, stdout);
    fgets(buffer, SIZE, stdin);

    char* cpy = malloc(strlen(buffer)+1);
    stcpy(cpy, buffer);
    cpy[strlen[cpy] -1] = '\0';
    return cpy;
}

void add_history(char* unused) {}

#else
#include <editline/readline.h>
#endif



int main(int argc, char** argv) {
    puts("Lipsy version 0.0.0.0.1");
    puts("Press CTRL+c to exit\n");

    while(1) {
        char* input = readline("lipsy> ");

        add_history(input);

        printf("No you're a %s\n", input);

        free(input);
    }
    return 0;
}
