#include "typefast.h"
int rows, cols;
int cur_row = 0;
int max_line_len;
struct timespec start, finish;
char** dict; // unprinted words
size_t dict_entries = 0;
char** dict_printed;
volatile size_t dict_printed_entries = 0;
size_t malloced_lines = 0;
int score = 0;
unsigned int wpm = 0;
unsigned int hits = 0;
unsigned int misses = 0;
unsigned short speed = 9;
unsigned short shuffle = 0;
volatile unsigned short game_over = 0;
char* next_input;

MUT input_lock          = PTHREAD_MUTEX_INITIALIZER;
MUT dict_printed_lock   = PTHREAD_MUTEX_INITIALIZER;
COND input_flag         = PTHREAD_COND_INITIALIZER;

int main(int argc, char* argv[]) {
    signal(SIGINT, free_exit);
    clock_gettime(CLOCK_MONOTONIC, &start); // used by shuffle_dict
    parse_options(argc, argv);
    init_ncurses();
    next_input = xmalloc((max_line_len+1)*sizeof(char));
    dict_printed = xmalloc(dict_entries*sizeof(char*));
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

void parse_options(int argc, char* argv[]) {
    short file_specified = 0;
    unsigned short i = 1;
    while (i < argc && *argv[i] == '-') {
        switch(*(argv[i]+1)) {
            case 'h':
                free_exit(HELP_EXIT);
                i++;
                break;
            case 's':
                shuffle = 1;
                i++;
                break;
            case 'v':
                if (isdigit(*(argv[i+1]))) {
                    speed = atoi(argv[++i]);
                    if (speed < 1) {
                        error("Bad input: minimum speed is 1\n", NULL);
                    }
                } else {
                    error("Bad input: invalid speed parameter", NULL);
                }
                i++;
                break;
            case 'f':
                file_specified = 1;
                dict = read_dict(&dict_entries,argv[++i]);
                break;
            default:
                break;
        }
    }
    if (!file_specified) {
        dict = read_dict(&dict_entries, DEF_FILE);
    }
}

void init_ncurses() {
    initscr();
    start_color();
    use_default_colors();
    init_pair(1, COLOR_WHITE, COLOR_RED);
    init_pair(2, COLOR_WHITE, COLOR_BLUE);
    init_pair(3, COLOR_WHITE, COLOR_YELLOW);
    init_pair(4, COLOR_YELLOW, -1);
    cbreak();
    noecho();
    curs_set(0);
    getmaxyx(stdscr, rows, cols);
    refresh();
}

void free_exit(int sig) {
    game_over = 1;
    if (sig == END_EXIT || sig == SIGINT) {
        clock_gettime(CLOCK_MONOTONIC, &finish);
        float elapsed = (finish.tv_sec - start.tv_sec);
        elapsed += (finish.tv_nsec - start.tv_nsec) / 1000000000.0;
        float accuracy = 0;
        float hitratio = 0;
        float missratio = 0;
        if (hits+misses > 0) {
            accuracy = ((float) hits)/(hits+misses);
            hitratio = ((float) hits)/dict_printed_entries;
            missratio = ((float) misses)/dict_printed_entries;
        }
        clear();
        
        mvprintw(MID_Y,   MID_XA(END_MSG), END_MSG);
        mvprintw(MID_Y+1, MID_XA(TIM_MSG), TIM_MSG, 2, elapsed);
        mvprintw(MID_Y+2, MID_XA(TOT_MSG), TOT_MSG, dict_printed_entries);
        mvprintw(MID_Y+2, MID_XA(WPM_MSG), WPM_MSG, wpm);
        mvprintw(MID_Y+3, MID_XA(ACC_MSG), ACC_MSG, 2, accuracy);
        mvprintw(MID_Y+4, MID_XA(MIS_MSG), MIS_MSG, misses, 2, missratio);
        mvprintw(MID_Y+5, MID_XA(HIT_MSG), HIT_MSG, hits, 2, hitratio);
        mvprintw(MID_Y+6, MID_XA(KEY_MSG), KEY_MSG);
        refresh();
        getch();
        endwin();
    } else if (sig == HELP_EXIT) {
        puts(HELP_MSG);
        exit(1);
    }
    for (size_t i = 0; i < malloced_lines; i++) {
        free(dict[i]);
    }
    for (size_t i = 0; i < dict_printed_entries; i++) {
        free(dict_printed[i]);
    }
    free(next_input);
    free(dict);
    free(dict_printed);
    next_input = NULL;
    dict = NULL;
    dict_printed = NULL;
    exit(1);
}

void* xmalloc(size_t bytes) {
    void* p = malloc(bytes);
    if (p != NULL) {
        return p;
    }
    error("xmalloc failed", NULL);
}

void* xrealloc(void* p, size_t bytes) {
    p = realloc(p, bytes);
    if (p != NULL) {
        return p;
    }
    error("xrealloc failed", NULL);
}

char** read_dict(size_t* nbr_of_lines, char* filename) {
    FILE *f;
    malloced_lines = 101;
    char** lines = xmalloc(malloced_lines*sizeof(char*));
    f = fopen(filename, "r");
    if (f == NULL) {
        error("Couldn't open dictionary file:", filename);
    }
    size_t line = 0;
    max_line_len = 6;
    while (!feof(f))
    {
        while (line >= malloced_lines) {
            malloced_lines *= 2;
            lines = xrealloc(lines, malloced_lines*sizeof(char*));
        }
        int line_pos = 0;
        lines[line] = xmalloc(max_line_len * sizeof(char));
        char ch = 0;
        while (ch != '\n' && !feof(f)) {
            ch = fgetc(f);
            while (line_pos >= max_line_len - 1) {
                max_line_len *= 2;
                lines[line] = xrealloc(lines[line],max_line_len*sizeof(char));
            }
            lines[line][line_pos++] = ch;
        }
        lines[line][line_pos-1] = '\0';
        *nbr_of_lines = ++line;
    }
    fclose(f);
    if (shuffle) {
        shuffle_dict(lines, *nbr_of_lines);
    }
    return lines;
}

void shuffle_dict(char** array, size_t n)
{
    if (n > 1) {
        size_t i;
        srand(start.tv_sec % INT_MAX);
        for (i = 0; i < n - 1; i++) {
          size_t j = rand() % (n - 1);
          char* temp  = array[j];
          array[j] = array[i];
          array[i] = temp;
        }
    }
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
    move(prev_y, prev_x);
    for (short i = 0; i < strlen; i++) {
        str[i] = 0;
    }
}

void backspace(int y, int x, short str_len) {
    mvdelch(y,x+str_len);
    refresh();
}


void* input(void* param) {
    char next[max_line_len];
    unsigned short pos = 0;
    while (!game_over) {
        char c = getch();
        if (c == '\n' || c == ' ') {
            next[pos++] = '\0';
            LOCK(&input_lock);
            strcpy(next_input, next);
            SIGNAL(&input_flag);
            UNLOCK(&input_lock);
            erase_row(rows-1, next, pos);
            pos = 0;
        } else if (c == KEY_BACKSPACE || c == KEY_DC || c == 127) {
            if (pos > 0) {
                pos--;
            }
            next[pos] = '\0';
            backspace(MAX_Y + 1, MIN_X, strlen(next));
        } else {
            if (pos < max_line_len) {
                next[pos++] = c;
            }
            mvprintw(MAX_Y + 1, MIN_X, "%s", next);
            refresh();
        }
    }
    return NULL;
}

void* print_words(void* param) {
    size_t printed = dict_entries;
    short prev_strlen = 20;
    for (size_t i = 0; i < dict_entries; i++) {
        cur_row = i % MAX_Y + 1;
        usleep(SEC(60.0/speed));
        LOCK(&dict_printed_lock);
        if (i > cur_row) {
            for (size_t k = 0; k < i; k++) {
                if (k % MAX_Y + 1 == cur_row) {
                    free(dict_printed[k]);
                    dict_printed[k] = NULL;
                }
            }
        }
        dict_printed[dict_printed_entries] = xmalloc((1+strlen(dict[i]))*sizeof(char));
        strcpy(dict_printed[dict_printed_entries++], dict[i]);
        UNLOCK(&dict_printed_lock);
        erase_row(cur_row, NULL, 0);
        if (game_over) {
            return NULL;
        } else {
            if (printed > dict_entries/10) {
                attron(YELLOW);
            } else {
                attron(RED);
            }
            mvprintw(MIN_Y-1, MIN_X, "Words left: %d ", --printed);
            if (printed > dict_entries/10) {
                attroff(YELLOW);
            } else {
                attron(RED);
            }
        }
        mvprintw(cur_row, MIN_X, dict[i]);
        refresh();
    }
    game_over = 1;
    SIGNAL(&input_flag);
    return NULL;
}

void* score_tracker(void* param) {
    char next[max_line_len];
    volatile unsigned int matches = 0;
    while ( !game_over  && matches != dict_entries) {
        LOCK(&input_lock);
        pthread_cond_wait(&input_flag, &input_lock);
        if (game_over) {
            break;
        }
        strcpy(next, next_input);
        UNLOCK(&input_lock);
        short match = 0;
        short match_row = 0;
        LOCK(&dict_printed_lock);
        for (size_t i = 0; i < dict_printed_entries; i++) {
            if (dict_printed[i] != NULL && strcmp(next, dict_printed[i]) == 0) {
                match = 1;
                matches++;
                match_row = i % MAX_Y +1;
                free(dict_printed[i]);
                dict_printed[i] = NULL;
                dict_entries--;
                break;
            }
        }
        UNLOCK(&dict_printed_lock);

        clock_gettime(CLOCK_MONOTONIC, &finish);
        float elapsed = (finish.tv_sec - start.tv_sec);

        wpm = (int) hits/(elapsed/60);

        int x = MAX_XA("WPM: XXX | Score:XXXX");
        if (match) {
                hits++;
                attron(BLUE);
                mvprintw(match_row, MIN_X, next);
                mvprintw(MIN_Y-1, x, "WPM: %d | Score: %d", wpm, ++score);
                attroff(BLUE);
        } else {
                misses++;
                attron(RED);
                mvprintw(MAX_Y + 1, MAX_XA(next), next);
                mvprintw(MIN_Y-1, x, "WPM: %d | Score: %d", wpm, --score);
                attroff((RED));
        }
    }
    game_over = 1;
    return NULL;
}
