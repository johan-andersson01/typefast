#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#define DEFAULT_FILE "dictionary.txt"
#define HELP_EXIT (33)
#define END_EXIT (0)

void error(char* msg, char* arg);
void score_tracker();
void print_help();
char** read_dict(size_t* nbr_of_lines, char* filename);
void input();
void print_words();
void free_exit(int sig);

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
size_t wordlist_entries = 0;
size_t malloced = 0;
int score = 0;
unsigned short speed = 9;
volatile unsigned short game_over = 0;
char next_input[100];

pthread_mutex_t game_over_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t input_lock     = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t wordlist_lock  = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t input_cond      = PTHREAD_COND_INITIALIZER;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        dict = read_dict(&dict_entries, DEFAULT_FILE);
    } else if (argc > 2) {
        error("Too many arguments. Use -h for help.", NULL);
    } else if (*argv[1] == '-' && *(argv[1]+1) == 'h') {
        print_help();
    } else {
        dict = read_dict(&dict_entries,  argv[1]);
    }
    wordlist = malloc(dict_entries*sizeof(char*));
    signal(SIGINT, free_exit);
    // init threads
    pthread_t threads[3]; 
    pthread_create(&threads[0], NULL, print_words, NULL);
    pthread_create(&threads[1], NULL, score_tracker, NULL);
    pthread_create(&threads[2], NULL, input, NULL);

    if (pthread_join(threads[0], NULL)) {
        fprintf(stderr, "Error joining thread %d\n", 0);
    }
    free_exit(END_EXIT);
}

void free_exit(int sig) {
    if (sig == END_EXIT) {
        printf("You reached the end of the game\nYour score is: %d\n", score);
    } else if (sig == SIGINT) {
        printf("Good bye. \nYour score is: %d\n", score);
    }
    for (size_t i = 0; i < malloced; i++) {
        free(dict[i]);
        dict[i] = NULL;
    }
    for (size_t i = 0; i < wordlist_entries; i++) {
        free(wordlist[i]);
        wordlist[i] = NULL;
    }
    free(dict);
    free(wordlist);
    dict = NULL;
    wordlist = NULL;
    exit(1);
}



char** read_dict(size_t* nbr_of_lines, char* filename) {
    FILE *f;
    malloced = 100;
    char** lines = malloc(malloced*sizeof(char*));
    f = fopen(filename, "r");
    if (f == NULL) {
        error("Couldn't open dictionary file:", filename);
    }
    size_t line = 0;
    while (!feof(f))
    {
        int line_pos = 0;
        int max_line_length = 2;
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
            *nbr_of_lines = ++line;
            while (line >= malloced) {
                malloced *= 2;
                lines = (char **)realloc(lines, malloced);
            }
        }
    }
    return lines;
}

void print_help() {
    printf("Welcome to Type Fast!\n");
    free_exit(HELP_EXIT);
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

void input(void* param) {
    char next[100];
    while (fgets(next, 100, stdin)) {
        lock(&input_lock);
        strcpy(next_input, next);
        pthread_cond_signal(&input_cond);
        unlock(&input_lock);
        printf("you entered: %s\n", next);
    }
}

void lock(void* mutex) {
    pthread_mutex_lock(mutex);
}

void unlock(void* mutex) {
    pthread_mutex_unlock(mutex);
}

void print_words(void* param) {
    for (size_t i = 0; i < dict_entries; i++) {
        sleep(10-speed);
        puts(dict[i]);
        fflush(stdout);
        lock(&wordlist_lock);
        wordlist[wordlist_entries] = malloc((1+strlen(dict[i]))*sizeof(char));
        strcpy(wordlist[wordlist_entries++], dict[i]);
        unlock(&wordlist_lock);
    }
    game_over = 1;
}

void score_tracker(void* param) {
    char next[100];
    while (!game_over) {
        lock(&input_lock);
        pthread_cond_wait(&input_cond, &input_lock);
        printf("in s_t: next_input = %s\n", next_input);
        strcpy(next, next_input);
        unlock(&input_lock);
        short match = 0;
        lock(&wordlist_lock);
        printf("in s_t: wordlist_entries = %zd\n", wordlist_entries);
        for (size_t i = 0; i < wordlist_entries; i++) {
            if (wordlist[i] != NULL && strcmp(next, wordlist[i]) == 0) {
                match = 1;
                free(wordlist[i]);
                wordlist[i] = NULL;
                break;
            }
        }
        unlock(&wordlist_lock);
        if (match) score++;
        else score--;
    }
}
