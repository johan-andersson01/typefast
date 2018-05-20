#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <curses.h>
#include <signal.h>
#include <time.h>

// status codes
#define HELP_EXIT (33)
#define END_EXIT  (0)
// string constants
#define DEF_FILE "dictionary.txt"
#define WELCOME "Welcome to Type Fast"
#define END_MSG "You reached the end of the game"
#define TIM_MSG "Time: %.*f seconds"
#define ACC_MSG "Accuracy: %.*f"
#define HIT_MSG "Hits: %d"
#define MIS_MSG "Misses: %d\n"
// layout constants
#define TOP_X (col - col/10)
#define TOP_X(msg) (col - strlen(msg))
#define MID_X(msg) (col-strlen(msg))/2
#define BOT_X (col/10)
#define TOP_Y (row-2)
#define MID_Y (row/2)
#define BOT_Y (1)
// function macros
#define SIGNAL(x) pthread_cond_signal(x)
#define LOCK(x) pthread_mutex_lock(x)
#define UNLOCK(x) pthread_mutex_unlock(x)
#define MUT pthread_mutex_t
#define COND pthread_cond_t

// declarations
void error(char* msg, char* arg);
void score_tracker();
void print_help();
char** read_dict(size_t* nbr_of_lines, char* filename);
void input();
void print_words();
void free_exit(int sig);
