
#include <locale.h>
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
#define WELCOME  "Welcome to Type Fast"
#define END_MSG  "You reached the end of the game"
#define TIM_MSG  "Time: %.*f seconds"
#define TOT_MSG  "Number of words: %zd"
#define ACC_MSG  "Accuracy: %.*f"
#define HIT_MSG  "Hits: %d (%.*f)"
#define MIS_MSG  "Misses: %d (%.*f)"
#define KEY_MSG  "Press any key twice to exit"
// layout constants
//
// BOT_X/Y----------MID_X-----------------> TOP_X
//   |
//   |
//   |
//   |
//   |
// MID_Y
//   |
//   |
//   |
//   |
//   |
//   v
// TOP_Y
//
#define TOP_X       (cols - cols/10)
#define TOP_XA(msg) (cols - strlen(msg))
#define MID_XA(msg) (cols-strlen(msg))/2
#define BOT_X       (cols/10)
#define TOP_Y       (rows-2)
#define MID_Y       (rows/2)
#define BOT_Y       (1)
// macros
#define MS        (1000)
#define SIGNAL(x) pthread_cond_signal(x)
#define LOCK(x)   pthread_mutex_lock(x)
#define UNLOCK(x) pthread_mutex_unlock(x)
#define MUT       pthread_mutex_t
#define COND      pthread_cond_t
#define RED       (COLOR_PAIR(1))
#define GREEN     (COLOR_PAIR(2))
// declarations
void   init_ncurses();
void   parse_flags(int argc, char* argv[]);
void   error(char* msg, char* arg);
void   free_exit(int sig);
void*  score_tracker();
void*  input();
void*  print_words();
void*  xmalloc(size_t bytes);
char** read_dict(size_t* nbr_of_lines, char* filename);
