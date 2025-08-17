#include <stdio.h>
#include <unistd.h>
#include <locale.h>
#include <pthread.h>
#include "sim.h"
#include "langton.h"
#include "vis.h"

int main_text() {
    Coordinate board_size = {8000, 8000};
    Position start = {{4000, 4000}, UP};

    State st = new_state(board_size, &start, 1);
    if (register_langton(&st)) exit(EXIT_FAILURE);

    for (int i = 0; i < 6; ++i) {
        printf("Position: (%d, %d) <%s>\n",
            st.positions[0].coordinate.x, st.positions[0].coordinate.y,
            dir_str(st.positions[0].direction));
        advance_state(&st);
    }

    return 0;
}

int main_vis() {
    setlocale(LC_ALL, "");
    Coordinate board_size = {8000, 8000};
    Position starts[2] = { {{3950, 3950}, DOWN}, {{4050, 4050}, UP} };

    State st = new_state(board_size, starts, 2);
    if (register_langton(&st)) exit(EXIT_FAILURE);
    Renderer *r = create_renderer(board_size.x, board_size.y);
    if (!r) {
        perror("Create renderer");
        exit(EXIT_FAILURE);
    }

    update_state(r, &st);

    pthread_t renderer;
    pthread_create(&renderer, NULL, start_render_thread, r);

    while (!r->should_exit) {
        usleep(500);
        advance_state(&st);
        update_state(r, &st);
    }

    return 0;
}

int main() {
    main_vis();
    return 0;
}