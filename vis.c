#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <math.h>
#include <locale.h>
#include <wchar.h>
#include "vis.h"

// UTF-8 Unicode block characters for different fill levels
const char *block_chars[] = {
    " ",           // 0/4 filled - empty
    "\u2591",      // 1/4 filled - ░ Light shade
    "\u2592",      // 2/4 filled - ▒ Medium shade
    "\u2593",      // 3/4 filled - ▓ Dark shade
    "\u2588"       // 4/4 filled - █ Full block
};

// Global renderer instance
static Renderer *g_renderer = NULL;
static struct termios orig_termios;

void cleanup_terminal(void) {
    if (g_renderer) {
        printf(SHOW_CURSOR);
        printf(RESET_COLOR);
        printf(CLEAR_SCREEN);
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    }
}

void get_terminal_size(int *width, int *height) {
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    *width = w.ws_col;
    *height = w.ws_row;
}

int is_active_coordinate(Renderer *r, int x, int y) {
    if (!r->current_state || !r->current_state->positions) {
        return 0;
    }
    
    for (int i = 0; i < r->current_state->position_len; i++) {
        if (r->current_state->positions[i].coordinate.x == x && 
            r->current_state->positions[i].coordinate.y == y) {
            return 1;
        }
    }
    return 0;
}

int get_cell_value(Renderer *r, int x, int y) {
    if (x < 0 || y < 0 || x >= r->board_width || y >= r->board_height) {
        return 0;
    }
    return r->current_state->board[y][x];
}

void render_character_at_position(Renderer *r, int screen_x, int screen_y, int content_width, int content_height) {
    if (screen_x >= content_width || screen_y >= content_height) {
        return;
    }
    
    float cells_per_char = r->viewport.zoom;
    
    if (cells_per_char <= 1.0) {
        // Zoomed in: 1 character = 1 cell or less
        int world_x = r->viewport.x + screen_x;
        int world_y = r->viewport.y + (content_height - 1 - screen_y);
        
        int cell_value = get_cell_value(r, world_x, world_y);
        int is_active = is_active_coordinate(r, world_x, world_y);
        
        // Store as UTF-8 string
        strcpy(&r->screen_buffer[screen_y][screen_x * 8], cell_value ? "█" : " ");
        r->color_buffer[screen_y][screen_x] = is_active ? 'R' : 'N';
    } else {
        // Zoomed out: 1 character = multiple cells
        int cells_wide = (int)ceil(cells_per_char);
        int cells_high = (int)ceil(cells_per_char);
        
        int start_x = r->viewport.x + (int)(screen_x * cells_per_char);
        int start_y = r->viewport.y + (int)((content_height - 1 - screen_y) * cells_per_char);
        
        int filled_count = 0;
        int total_count = 0;
        int has_active = 0;
        
        for (int dy = 0; dy < cells_high; dy++) {
            for (int dx = 0; dx < cells_wide; dx++) {
                int world_x = start_x + dx;
                int world_y = start_y + dy;
                
                if (world_x >= 0 && world_y >= 0 && 
                    world_x < r->board_width && world_y < r->board_height) {
                    total_count++;
                    if (get_cell_value(r, world_x, world_y)) {
                        filled_count++;
                    }
                    if (is_active_coordinate(r, world_x, world_y)) {
                        has_active = 1;
                    }
                }
            }
        }
        
        int fill_level = 0;
        if (total_count > 0) {
            fill_level = (filled_count * 4) / total_count;
            if (fill_level > 4) fill_level = 4;
        }
        
        // Store UTF-8 block character
        const char *block_char = block_chars[fill_level];
        strcpy(&r->screen_buffer[screen_y][screen_x * 8], block_char);
        r->color_buffer[screen_y][screen_x] = has_active ? 'R' : 'N';
    }
}

void render_frame(Renderer *r) {
    pthread_mutex_lock(&r->state_lock);
    pthread_mutex_lock(&r->render_lock);
    
    int content_width = r->viewport.width - 2;  // Account for box borders
    int content_height = r->viewport.height - 4; // Account for box borders and status lines
    
    // Clear screen buffer - each position can hold up to 8 bytes for UTF-8
    for (int y = 0; y < content_height; y++) {
        for (int x = 0; x < content_width; x++) {
            strcpy(&r->screen_buffer[y][x * 8], " ");
            r->color_buffer[y][x] = 'N';
        }
    }
    
    // Render each character position within the content area
    for (int y = 0; y < content_height; y++) {
        for (int x = 0; x < content_width; x++) {
            render_character_at_position(r, x, y, content_width, content_height);
        }
    }
    
    // Draw the box and content
    MOVE_CURSOR(1, 1);
    
    // Top border
    printf(BOX_TOP_LEFT);
    for (int x = 0; x < content_width; x++) {
        printf(BOX_HORIZONTAL);
    }
    printf(BOX_TOP_RIGHT "\n");
    
    // Content with side borders
    for (int y = 0; y < content_height; y++) {
        printf(BOX_VERTICAL);
        for (int x = 0; x < content_width; x++) {
            char *char_str = &r->screen_buffer[y][x * 8];
            if (r->color_buffer[y][x] == 'R') {
                printf(RED_BG "%s" RESET_COLOR, char_str);
            } else {
                printf("%s", char_str);
            }
        }
        printf(BOX_VERTICAL);
        if (y < content_height - 1) {
            printf("\n");
        }
    }
    
    // Bottom border
    printf("\n" BOX_BOTTOM_LEFT);
    for (int x = 0; x < content_width; x++) {
        printf(BOX_HORIZONTAL);
    }
    printf(BOX_BOTTOM_RIGHT "\n");
    
    // Status line
    MOVE_CURSOR(r->viewport.height - 1, 1);
    printf("\033[K"); // Clear from cursor to end of line
    printf("Pos: (%d,%d)", r->viewport.x, r->viewport.y);
    MOVE_CURSOR(r->viewport.height - 1, r->viewport.width - 15);
    printf("\033[K"); // Clear from cursor to end of line
    printf("Zoom: %.2fx\n", r->viewport.zoom);
    
    // Controls line in gray - centered
    MOVE_CURSOR(r->viewport.height, 1);
    printf("\033[K"); // Clear the entire line
    const char *controls_text = "WASD:Pan +/-:Zoom C:Center Q:Quit";
    int controls_len = strlen(controls_text);
    int padding = (r->viewport.width - controls_len) / 2;
    if (padding > 0) {
        printf("%*s", padding, ""); // Print padding spaces
    }
    printf(GRAY_COLOR "%s" RESET_COLOR, controls_text);
    
    fflush(stdout);
    
    pthread_mutex_unlock(&r->render_lock);
    pthread_mutex_unlock(&r->state_lock);
}

void signal_handler(int sig) {
    if (sig == SIGWINCH) {
        // Handle window resize
        if (g_renderer) {
            pthread_mutex_lock(&g_renderer->render_lock);
            
            // Store old dimensions before getting new ones
            int old_content_height = g_renderer->viewport.height - 4;
            
            // Get new terminal size
            get_terminal_size(&g_renderer->viewport.width, &g_renderer->viewport.height);
            
            // Free old buffers if they exist
            if (g_renderer->screen_buffer) {
                // Use the stored old content height, ensuring we don't go below 0
                if (old_content_height > 0) {
                    for (int i = 0; i < old_content_height; i++) {
                        if (g_renderer->screen_buffer[i]) {
                            free(g_renderer->screen_buffer[i]);
                        }
                        if (g_renderer->color_buffer[i]) {
                            free(g_renderer->color_buffer[i]);
                        }
                    }
                }
                free(g_renderer->screen_buffer);
                free(g_renderer->color_buffer);
                g_renderer->screen_buffer = NULL;
                g_renderer->color_buffer = NULL;
            }
            
            // Allocate new buffers
            int content_width = g_renderer->viewport.width - 2;
            int content_height = g_renderer->viewport.height - 4;
            
            // Ensure we don't allocate negative or zero sized buffers
            if (content_width > 0 && content_height > 0) {
                g_renderer->screen_buffer = malloc(content_height * sizeof(char*));
                g_renderer->color_buffer = malloc(content_height * sizeof(char*));
                
                for (int i = 0; i < content_height; i++) {
                    g_renderer->screen_buffer[i] = calloc(content_width * 8 + 1, sizeof(char));
                    g_renderer->color_buffer[i] = malloc(content_width + 1);
                    g_renderer->color_buffer[i][content_width] = '\0';
                }
            }
            
            pthread_mutex_unlock(&g_renderer->render_lock);
            
            // Clear screen and re-render only if we have valid buffers
            printf(CLEAR_SCREEN);
            if (content_width > 0 && content_height > 0) {
                render_frame(g_renderer);
            }
        }
        return;
    }
    
    if (g_renderer) {
        g_renderer->should_exit = 1;
    }
    cleanup_terminal();
    exit(0);
}

void setup_terminal(void) {
    struct termios raw;
    
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(cleanup_terminal);
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGWINCH, signal_handler); // Handle window resize
    
    raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;
    
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    printf(HIDE_CURSOR);
    printf(CLEAR_SCREEN);
}

void *input_thread(void *arg) {
    Renderer *r = (Renderer *)arg;
    char c;
    
    while (!r->should_exit) {
        if (read(STDIN_FILENO, &c, 1) == 1) {
            pthread_mutex_lock(&r->render_lock);
            
            int pan_speed = (int)fmax(1, r->viewport.zoom);
            
            switch (c) {
                case 'w': case 'k': // Up
                    r->viewport.y += pan_speed;
                    break;
                case 's': case 'j': // Down
                    r->viewport.y -= pan_speed;
                    break;
                case 'a': case 'h': // Left
                    r->viewport.x -= pan_speed;
                    break;
                case 'd': case 'l': // Right
                    r->viewport.x += pan_speed;
                    break;
                case '=': case '+': // Zoom in
                    r->viewport.zoom *= 0.8f;
                    if (r->viewport.zoom < 0.5f) r->viewport.zoom = 0.5f;
                    break;
                case '-': case '_': // Zoom out
                    r->viewport.zoom *= 1.25f;
                    if (r->viewport.zoom > 50.0f) r->viewport.zoom = 50.0f;
                    break;
                case 'c': // Center view on active coordinates centroid
                    if (r->current_state && r->current_state->positions && r->current_state->position_len > 0) {
                        // Calculate centroid of active coordinates
                        int sum_x = 0, sum_y = 0;
                        for (int i = 0; i < r->current_state->position_len; i++) {
                            sum_x += r->current_state->positions[i].coordinate.x;
                            sum_y += r->current_state->positions[i].coordinate.y;
                        }
                        int centroid_x = sum_x / r->current_state->position_len;
                        int centroid_y = sum_y / r->current_state->position_len;
                        
                        // Center viewport on centroid
                        int content_width = r->viewport.width - 2;
                        int content_height = r->viewport.height - 4;
                        
                        // To center the centroid in the visible area:
                        // centroid_x = viewport.x + content_width/2
                        // centroid_y = viewport.y + content_height/2 - 1  (due to coordinate mapping)
                        r->viewport.x = centroid_x - (content_width / 2);
                        r->viewport.y = centroid_y - (content_height / 2) + 1;
                        
                    } else {
                        // Fallback to board center if no active coordinates
                        int content_width = r->viewport.width - 2;
                        int content_height = r->viewport.height - 4;
                        r->viewport.x = r->board_width / 2 - (content_width / 2);
                        r->viewport.y = r->board_height / 2 - (content_height / 2) + 1;
                    }
                    break;
                case 'q': // Quit
                    r->should_exit = 1;
                    pthread_mutex_unlock(&r->render_lock);
                    return NULL;
            }
            
            pthread_mutex_unlock(&r->render_lock);
            render_frame(r);
        }
        usleep(10000); // 10ms
    }
    
    return NULL;
}

Renderer* create_renderer(int board_width, int board_height) {
    Renderer *r = malloc(sizeof(Renderer));
    if (!r) return NULL;
    
    r->board_width = board_width;
    r->board_height = board_height;
    r->current_state = NULL;
    r->should_exit = 0;
    
    get_terminal_size(&r->viewport.width, &r->viewport.height);
    r->viewport.x = 0;
    r->viewport.y = 0;
    r->viewport.zoom = 1.0f;
    
    // Allocate screen buffers - need extra space for UTF-8 characters
    // Account for box borders reducing content area
    int content_width = r->viewport.width - 2;
    int content_height = r->viewport.height - 4;
    
    r->screen_buffer = malloc(content_height * sizeof(char*));
    r->color_buffer = malloc(content_height * sizeof(char*));
    
    for (int i = 0; i < content_height; i++) {
        // Allocate 8 bytes per character position to handle UTF-8 (max 4 bytes + null terminator, with padding)
        r->screen_buffer[i] = calloc(content_width * 8 + 1, sizeof(char));
        r->color_buffer[i] = malloc(content_width + 1);
        r->color_buffer[i][content_width] = '\0';
    }
    
    pthread_mutex_init(&r->state_lock, NULL);
    pthread_mutex_init(&r->render_lock, NULL);
    
    return r;
}

void start_renderer(Renderer *r) {
    if (!r) return;
    
    g_renderer = r;
    setup_terminal();
    
    pthread_t input_tid;
    pthread_create(&input_tid, NULL, input_thread, r);
    
    // Initial render
    render_frame(r);
    
    // Main rendering loop
    while (!r->should_exit) {
        usleep(16666); // ~60 FPS
        // Render again if needed (for animations, etc.)
    }
    
    pthread_join(input_tid, NULL);
}

void* start_render_thread(void *arg) {
    Renderer *r = (Renderer*)(arg);
    start_renderer(r);
    return NULL;
}

void update_state(Renderer *r, State *new_state) {
    if (!r) return;
    
    pthread_mutex_lock(&r->state_lock);
    
    r->current_state = new_state;
    
    pthread_mutex_unlock(&r->state_lock);
    
    render_frame(r);
}

void destroy_renderer(Renderer *r) {
    if (!r) return;
    
    r->should_exit = 1;
    
    pthread_mutex_destroy(&r->state_lock);
    pthread_mutex_destroy(&r->render_lock);
    
    // Calculate content height safely
    int content_height = r->viewport.height - 4;
    if (content_height > 0 && r->screen_buffer && r->color_buffer) {
        for (int i = 0; i < content_height; i++) {
            if (r->screen_buffer[i]) {
                free(r->screen_buffer[i]);
            }
            if (r->color_buffer[i]) {
                free(r->color_buffer[i]);
            }
        }
    }
    
    if (r->screen_buffer) {
        free(r->screen_buffer);
    }
    if (r->color_buffer) {
        free(r->color_buffer);
    }
    
    free(r);
    
    cleanup_terminal();
}

// Test functions
State* create_test_state(int width, int height) {
    State *state = malloc(sizeof(State));
    state->board = malloc(height * sizeof(int*));
    
    for (int y = 0; y < height; y++) {
        state->board[y] = malloc(width * sizeof(int));
        for (int x = 0; x < width; x++) {
            // Create a simple pattern for testing
            state->board[y][x] = ((x + y) % 3 == 0) ? 1 : 0;
        }
    }
    
    // Create multiple positions for testing
    state->position_len = 3;
    state->positions = malloc(state->position_len * sizeof(Position));
    
    state->positions[0].coordinate.x = 10;
    state->positions[0].coordinate.y = 10;
    state->positions[0].direction = RIGHT;
    
    state->positions[1].coordinate.x = 15;
    state->positions[1].coordinate.y = 15;
    state->positions[1].direction = UP;
    
    state->positions[2].coordinate.x = 20;
    state->positions[2].coordinate.y = 20;
    state->positions[2].direction = LEFT;
    
    state->iteration = 0;
    state->rule_len = 0;
    state->rules = NULL;
    
    return state;
}

void free_test_state(State *state, int height) {
    for (int y = 0; y < height; y++) {
        free(state->board[y]);
    }
    free(state->board);
    if (state->positions) {
        free(state->positions);
    }
    free(state);
}

int main_example() {
    // Set locale for UTF-8 support
    setlocale(LC_ALL, "");
    
    const int BOARD_WIDTH = 100;
    const int BOARD_HEIGHT = 100;
    
    // Create test state
    State *test_state = create_test_state(BOARD_WIDTH, BOARD_HEIGHT);
    
    // Create and start renderer
    Renderer *renderer = create_renderer(BOARD_WIDTH, BOARD_HEIGHT);
    if (!renderer) {
        printf("Failed to create renderer\n");
        return 1;
    }
    
    // Update with initial state
    update_state(renderer, test_state);
    
    printf("TUI Controls:\n");
    printf("WASD/HJKL: Pan view\n");
    printf("+/-: Zoom in/out\n");
    printf("C: Center view\n");
    printf("Q: Quit\n");
    printf("Press any key to start...\n");
    getchar();
    
    // Start the renderer (this blocks until quit)
    start_renderer(renderer);
    
    // Cleanup
    free_test_state(test_state, BOARD_HEIGHT);
    destroy_renderer(renderer);
    
    return 0;
}