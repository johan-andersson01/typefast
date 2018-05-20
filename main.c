#include "typefast.h"
int row, col;
int cur_row = 0;
int max_line_len;
struct timespec start, finish;
char** dict; // unprinted words
size_t dict_entries = 0;
char** wordlist;
volatile size_t wordlist_entries = 0;
size_t malloced_lines = 0;
int score = 0;
unsigned int hits = 0;
unsigned int misses = 0;
unsigned short speed = 9;
volatile unsigned short game_over = 0;
char* next_input;

MUT input_lock     = PTHREAD_MUTEX_INITIALIZER;
MUT wordlist_lock  = PTHREAD_MUTEX_INITIALIZER;
COND input_cond    = PTHREAD_COND_INITIALIZER;

int main(int argc, char* argv[]) {
    signal(SIGINT, free_exit);
    if (argc < 2) {
        dict = read_dict(&dict_entries, DEF_FILE);
    } else if (argc > 2) {
        error("Too many arguments. Use -h for help.", NULL);
    } else if (*argv[1] == '-' && *(argv[1]+1) == 'h') {
        free_exit(HELP_EXIT);
    } else {
        dict = read_dict(&dict_entries,  argv[1]);
    }
    next_input = xmalloc((max_line_len+1)*sizeof(char));
    initscr();
    start_color();
    init_pair(1, COLOR_WHITE, COLOR_RED);
    init_pair(2, COLOR_WHITE, COLOR_GREEN);
    cbreak();
    noecho();
    curs_set(0);
    getmaxyx(stdscr, row, col);
    mvprintw(row/2, (col-strlen(WELCOME))/2, "%s", WELCOME);
    refresh();
    wordlist = xmalloc(dict_entries*sizeof(char*));
    // init threads
    pthread_t threads[3]; 
    clock_gettime(CLOCK_MONOTONIC, &start);
    pthread_create(&threads[0], NULL, print_words, NULL);
    pthread_create(&threads[1], NULL, score_tracker, NULL);
    pthread_create(&threads[2], NULL, input, NULL);
    if (pthread_join(threads[1], NULL)) {
        fprintf(stderr, "Error joining thread %d\n", 1);
    }
    free_exit(END_EXIT);
}

void free_exit(int sig) {
    game_over = 1;
    if (sig == END_EXIT || sig == SIGINT) {
        clock_gettime(CLOCK_MONOTONIC, &finish);
        float elapsed = (finish.tv_sec - start.tv_sec);
        elapsed += (finish.tv_nsec - start.tv_nsec) / 1000000000.0;
        float accuracy = 0;
        if (hits+misses > 0) {
            accuracy = ((float) hits)/(hits+misses);
        }
        clear();
        mvprintw(MID_Y,   MID_X(END_MSG), END_MSG);
        mvprintw(MID_Y+1, MID_X(TOT_MSG), TOT_MSG, wordlist_entries);
        mvprintw(MID_Y+2, MID_X(TIM_MSG), TIM_MSG, 2, elapsed);
        mvprintw(MID_Y+3, MID_X(ACC_MSG), ACC_MSG, 2, accuracy);
        mvprintw(MID_Y+4, MID_X(MIS_MSG), MIS_MSG, misses);
        mvprintw(MID_Y+5, MID_X(HIT_MSG), HIT_MSG, hits);
        mvprintw(MID_Y+6, MID_X(KEY_MSG), KEY_MSG);
        refresh();
        getch();
        endwin();
    } else if (sig == HELP_EXIT) {
        puts("You typed -h to get help!");
        exit(1);
    }
    for (size_t i = 0; i < malloced_lines; i++) {
        free(dict[i]);
        dict[i] = NULL;
    }
    for (size_t i = 0; i < wordlist_entries; i++) {
        free(wordlist[i]);
        wordlist[i] = NULL;
    }
    free(next_input);
    free(dict);
    free(wordlist);
    next_input = NULL;
    dict = NULL;
    wordlist = NULL;
    exit(1);
}

void* xmalloc(size_t bytes) {
    void* p = malloc(bytes);
    if (p != NULL) {
        return p;
    }
    error("xmalloc failed", NULL);
    exit(1);
}

char** read_dict(size_t* nbr_of_lines, char* filename) {
    FILE *f;
    malloced_lines = 100;
    char** lines = xmalloc(malloced_lines*sizeof(char*));
    f = fopen(filename, "r");
    if (f == NULL) {
        error("Couldn't open dictionary file:", filename);
    }
    size_t line = 0;
    while (!feof(f))
    {
        int line_pos = 0;
        int malloced_length= 2;
        lines[line] = xmalloc(malloced_length * sizeof(char));
        char ch = ' ';
        while (ch != '\n' && !feof(f)) {
            ch = fgetc(f);
            while (line_pos >= malloced_length - 2) {
                malloced_length *= 2;
                lines[line] = (char *)realloc(lines[line],malloced_length);
            }
            lines[line][line_pos++] = ch;
        }
        lines[line][line_pos-1] = '\0'; // strip \n
        if (!feof(f)) {
            *nbr_of_lines = ++line;
            while (line >= malloced_lines) {
                malloced_lines *= 2;
                lines = (char **)realloc(lines, malloced_lines);
            }
        }
        if (line_pos > max_line_len) {
            max_line_len = line_pos;
        }
    }
    return lines;
}

void error(char* msg, char* arg) {
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
            mvprintw(TOP_Y + 1, BOT_X, "%s", next);
            refresh();
        } else {
            next[pos++] = '\0';
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
        cur_row = i % TOP_Y + 1;
        sleep(1);
        LOCK(&wordlist_lock);
        if (i > cur_row) {
            for (size_t k = 0; k < i; k++) {
                if (k % TOP_Y + 1 == cur_row) {
                    free(wordlist[k]);
                    wordlist[k] = NULL;
                }
            }
        }
        wordlist[wordlist_entries] = xmalloc((1+strlen(dict[i]))*sizeof(char));
        strcpy(wordlist[wordlist_entries++], dict[i]);
        /* free(dict[i]); */
        /* dict[i] = NULL; */
        UNLOCK(&wordlist_lock);
        erase_row(cur_row, NULL, 0);
        if (game_over) {
            return;
        }
        mvprintw(cur_row, BOT_X, dict[i]);
        refresh();
    }
    game_over = 1;
    SIGNAL(&input_cond);
}

void score_tracker(void* param) {
    char next[max_line_len];
    volatile unsigned int matches = 0;
    while ( !game_over  && matches != dict_entries) {
        LOCK(&input_lock);
        pthread_cond_wait(&input_cond, &input_lock);
        if (game_over) {
            break;
        }
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
                match_row = i % TOP_Y +1;
                line_len = strlen(wordlist[i]);
                free(wordlist[i]);
                wordlist[i] = NULL;
                break;
            }
        }
        UNLOCK(&wordlist_lock);

        if (match) {
                hits++;
                attron(COLOR_PAIR(2));
                mvprintw(match_row, BOT_X + max_line_len , next);
                mvprintw(TOP_Y+1, TOP_X("Score:XXXXX"), "Score: %d", ++score);
                attroff(COLOR_PAIR(2));
        } else {
                misses++;
                attron(COLOR_PAIR(1));
                mvprintw(TOP_Y + 1, TOP_X("Score:XXXXX") - strlen(next) - 2, next);
                mvprintw(TOP_Y + 1, TOP_X("Score:XXXXX"), "Score: %d", --score);
                attroff(COLOR_PAIR(1));
        }
    }
    game_over = 1;
}
