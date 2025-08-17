#ifndef VIS_H
#define VIS_H

#include <pthread.h>
#include "sim.h"

// ANSI escape codes
#define RESET_COLOR "\033[0m"
#define RED_BG "\033[41m"
#define GRAY_COLOR "\033[90m"
#define CLEAR_SCREEN "\033[2J"
#define HIDE_CURSOR "\033[?25l"
#define SHOW_CURSOR "\033[?25h"
#define MOVE_CURSOR(y,x) printf("\033[%d;%dH", (y), (x))

// Unicode box drawing characters
#define BOX_HORIZONTAL "─"
#define BOX_VERTICAL "│"
#define BOX_TOP_LEFT "┌"
#define BOX_TOP_RIGHT "┐"
#define BOX_BOTTOM_LEFT "└"
#define BOX_BOTTOM_RIGHT "┘"

// UTF-8 Unicode block characters for different fill levels
extern const char *block_chars[];

typedef struct {
    int x, y;           // Bottom-left coordinate
    float zoom;         // Cells per character (1.0 = 1:1, 2.0 = 4 cells per char, etc.)
    int width, height;  // Terminal dimensions
} Viewport;

typedef struct {
    State *current_state;
    int board_width, board_height;
    Viewport viewport;
    pthread_mutex_t state_lock;
    pthread_mutex_t render_lock;
    int should_exit;
    char **screen_buffer;  // Each position stores UTF-8 string
    char **color_buffer;
} Renderer;

// Function declarations
void cleanup_terminal(void);
void get_terminal_size(int *width, int *height);
int is_active_coordinate(Renderer *r, int x, int y);
int get_cell_value(Renderer *r, int x, int y);
void render_character_at_position(Renderer *r, int screen_x, int screen_y, int content_width, int content_height);
void render_frame(Renderer *r);
void signal_handler(int sig);
void setup_terminal(void);
void *input_thread(void *arg);
Renderer* create_renderer(int board_width, int board_height);
void start_renderer(Renderer *r);
void* start_render_thread(void *arg);
void update_state(Renderer *r, State *new_state);
void destroy_renderer(Renderer *r);

// Test functions
State* create_test_state(int width, int height);
void free_test_state(State *state, int height);
int main_example();

#endif // VIS_H