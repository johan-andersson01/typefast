#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#define DEFAULT_FILE "dictionary.txt"

void error();
void score_tracker();
void print_help();
char** read_dict();
void input();
void print_words();

// global vars:
// wordlist (printed words)
// dictionary (not printed yet)
// score
// input_queue
// mutex for wordlist (accessed by score_tracker and print_words
//
char** dict; // unprinted words
size_t dict_entries = 0;
char** wordlist;
unsigned short speed = 5;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        dict = read_dict(&dict_entries, DEFAULT_FILE);
        print_dict(dict, dict_entries);
    } else if (argc > 2) {
        error("Too many arguments. Use -h for help.", NULL);
    } else if (argv[1] == '-' && *argv[1]+1 == 'h') {
        print_help();
    } else {
        dict = read_dict(&dict_entries,  argv[1]);
        print_dict(dict, dict_entries);
    }
    print_words();
    // start thread printing out words
    // strart thread keeping track of score
    // start thread accepting user input
    // start thread matching user input and printed words
    free(dict);
    dict = NULL;
}

char** read_dict(size_t* nbr_of_lines, char* filename) {
    FILE *f;
    size_t max_lines = 20;
    char** lines = calloc(max_lines, sizeof(char*));
    f = fopen(filename, "r");
    if (f == NULL) {
        error("Couldn't open dictionary file:", filename);
    }
    size_t line = 0;
    while (!feof(f))
    {
        int line_pos = 0;
        int max_line_length = 2;
       // printf("trying to malloc at line %zx. max_lines = %zx\n", line, max_lines);
        lines[line] = malloc(max_line_length * sizeof(char));
        char ch = ' ';
        while (ch != '\n' && !feof(f)) {
            ch = fgetc(f);
            while (line_pos >= max_line_length - 2) {
                max_line_length *= 2;
                lines[line] = (char *)realloc(lines[line], max_line_length);
            }
            lines[line][line_pos++] = ch;
        }
        lines[line][line_pos] = '\0';
        if (!feof(f)) {
            //fputs(lines[line], stdout);
            //printf("line: %zx, maxline: %zx\n", line, max_lines);
            fflush(stdout);
            *nbr_of_lines = ++line;
            while (line >= max_lines) {
                max_lines *= 2;
                lines = (char **)realloc(lines, max_lines);
            }
        }
    }
    wordlist = malloc(line*sizeof(char*));
   return lines;
}

void print_dict(char** dict, size_t nbr_of_entries) {
    printf("print_dict: %zd entries\n", nbr_of_entries);
    for (size_t i = 0; i < nbr_of_entries; i++) {
        fputs(dict[i], stdout);
    }
}

void print_help() {
    printf("Welcome to Type Fast!\n");
}
void error(char* msg, char* arg) {
    printf("Boop beep! ");
    if (arg == NULL) {
        puts(msg);
    } else {
        printf(msg);
        printf(" %s\n", arg);
    }
    exit(1);
}

void input() {
    // close on Ctrl-c
    // listen for input
    // if input save it to input queue, change mutex s.t. score_tracker validates it
}

void print_words() {
    size_t next = 0;
    for (size_t i = 0; i < dict_entries; i++) {
        puts(dict[i]);
        fflush(stdout);
        wordlist[next] = malloc(strlen(dict[i])*sizeof(char));
        strcpy(wordlist[next], dict[i]);
        dict[i] = NULL;
        sleep(10-speed);
    }
}

void score_tracker() {
    // gives + points if input matches existing
    // gives - points if input doesn't match
}



