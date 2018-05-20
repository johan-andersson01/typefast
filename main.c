#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <curses.h>
#include <signal.h>
#define SIGNAL(x) pthread_cond_signal(x)
#define LOCK(x) pthread_mutex_lock(x)
#define UNLOCK(x) pthread_mutex_unlock(x)
#define DEFAULT_FILE "dictionary.txt"
#define HELP_EXIT (33)
#define END_EXIT (0)
#define L_BORDER (col/10)
void error(char* msg, char* arg);
void score_tracker();
void print_help();
char** read_dict(size_t* nbr_of_lines, char* filename);
void input();
void print_words();
void free_exit(int sig);
int row, col;
int cur_row = 0;
int max_line_len= 0;

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
volatile size_t wordlist_entries = 0;
size_t malloced = 0;
int score = 0;
unsigned short speed = 9;
volatile unsigned short game_over = 0;
char next_input[100];

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
    initscr();
    start_color();
    init_pair(1, COLOR_WHITE, COLOR_RED);
    init_pair(2, COLOR_WHITE, COLOR_GREEN);
    cbreak();
    noecho();
    curs_set(0);
    getmaxyx(stdscr, row, col);
    char msg[] = ("Welcome to Type Fast...\n");
    mvprintw(row/2, (col-strlen(msg))/2, "%s", msg);
    refresh();
    wordlist = malloc(dict_entries*sizeof(char*));
    signal(SIGINT, free_exit);
    // init threads
    pthread_t threads[3]; 
    pthread_create(&threads[0], NULL, print_words, NULL);
    pthread_create(&threads[1], NULL, score_tracker, NULL);
    pthread_create(&threads[2], NULL, input, NULL);
    if (pthread_join(threads[1], NULL)) {
        fprintf(stderr, "Error joining thread %d\n", 1);
    }
    free_exit(END_EXIT);
}

void free_exit(int sig) {
    clear();
    if (sig == END_EXIT) {
        mvprintw(row/2, (col-strlen("You reached the end of the game"))/2,"You reached the end of the game");
    }
    for (size_t i = 0; i < malloced; i++) {
        free(dict[i]);
        dict[i] = NULL;
    }
    for (size_t i = 0; i < wordlist_entries; i++) {
        free(wordlist[i]);
        wordlist[i] = NULL;
    }
    refresh();
    sleep(2);
    endwin();
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
        int malloced_length= 2;
        lines[line] = malloc(malloced_length * sizeof(char));
        char ch = ' ';
        while (ch != '\n' && !feof(f)) {
            ch = fgetc(f);
            while (line_pos >= malloced_length - 2) {
                malloced_length *= 2;
                lines[line] = (char *)realloc(lines[line],malloced_length);
            }
            lines[line][line_pos++] = ch;
        }
        lines[line][line_pos-1] = '\0';
        if (!feof(f)) {
            *nbr_of_lines = ++line;
            while (line >= malloced) {
                malloced *= 2;
                lines = (char **)realloc(lines, malloced);
            }
        }
        if (line_pos > max_line_len) {
            max_line_len = line_pos;
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

void erase_row(int row, char* str, short strlen) {
    int prev_x, prev_y;
    getyx(stdscr,prev_y,prev_x);
    move(row,0);
    clrtoeol();
    refresh();
    for (short i = 0; i < strlen; i++) {
        str[i] = 0;
    }
    move(prev_y, prev_x);
}

void input(void* param) {
    char next[max_line_len];
    unsigned short pos = 0;
    while (!game_over) {
        char c = getch();
        if (c != '\n') {
            next[pos++] = c;
            mvprintw(row-1, L_BORDER, "%s", next);
            refresh();
        } else {
            next[pos++] = 0;
            LOCK(&input_lock);
            strcpy(next_input, next);
            SIGNAL(&input_cond);
            UNLOCK(&input_lock);
            erase_row(row-1, next, pos);
            pos = 0;
        }
    }
}

void print_words(void* param) {
    for (size_t i = 0; i < dict_entries; i++) {
        sleep(10-speed);
        LOCK(&wordlist_lock);
        wordlist[wordlist_entries] = malloc((1+strlen(dict[i]))*sizeof(char));
        strcpy(wordlist[wordlist_entries++], dict[i]);
        UNLOCK(&wordlist_lock);
        mvprintw(++cur_row, L_BORDER, dict[i]);
        refresh();
    }
}

void score_tracker(void* param) {
    char next[max_line_len];
    volatile unsigned int matches = 0;
    while (matches != dict_entries) {
        LOCK(&input_lock);
        pthread_cond_wait(&input_cond, &input_lock);
        strcpy(next, next_input);
        UNLOCK(&input_lock);
        short match = 0;
        short match_row = 0;
        short line_len = 0;
        LOCK(&wordlist_lock);
        for (size_t i = 0; i < wordlist_entries; i++) {
            if (wordlist[i] != NULL && strcmp(next, wordlist[i]) == 0) {
                match = 1;
                matches++;
                match_row = i+1;
                line_len = strlen(wordlist[i]);
                free(wordlist[i]);
                wordlist[i] = NULL;
                break;
            }
        }
        UNLOCK(&wordlist_lock);

        if (match) {
                attron(COLOR_PAIR(2));
                mvprintw(match_row, L_BORDER + max_line_len , next);
                mvprintw(row - 2, col-(strlen("Score:    ")), "Score: %d", ++score);
                attroff(COLOR_PAIR(2));
        } else {
                attron(COLOR_PAIR(1));
                mvprintw(row/2+1, col/2-(strlen(next)), next);
                mvprintw(row - 2, col-(strlen("Score:    ")), "Score: %d", --score);
                attroff(COLOR_PAIR(1));
        }
    }
    game_over = 1;
}
