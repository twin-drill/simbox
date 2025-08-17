#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include "sim.h"

char* dir_str(Direction d) {
    switch (d) {
        case UP:
            return "UP";
        case DOWN:
            return "DOWN";
        case LEFT:
            return "LEFT";
        case RIGHT:
            return "RIGHT";
    }
}

void add_rules_core(State *state, const Behavior *b, size_t n) {
    Behavior *newmem = realloc(state->rules, (state->rule_len + n) * sizeof(Behavior));
    if (newmem == NULL) {
        perror("add_rules_core realloc");
        return;
    }
    
    state->rules = newmem;
    int i = state->rule_len;
    const Behavior *c;
    for (c = b; c < b + n; ++c, ++i) {
        state->rules[i] = *c;
    }
    printf("Added %zu rules\n", n);
    state->rule_len += n;
}

State new_state(Coordinate size, Position *starts, int num_pos) {
    // init board
    // todo check mem
    int **board = calloc(size.y, sizeof(int*));
    for (int i = 0; i < size.y; ++i) board[i] = calloc(size.x, sizeof(int));
    
    State st;
    st.board = board;
    st.positions = starts;
    st.position_len = num_pos;
    st.rules = NULL;
    st.rule_len = 0;
    st.iteration = 0;
    return st;
}

void advance_state(State *state) {
    for (int i = 0; i < state->rule_len; ++i) {
        for (int j = 0; j < state->position_len; ++j) {
            if (state->rules[i].condition(state, j)) {
                state->rules[i].execution(state, j);
            }
        }
    }
    ++state->iteration;
}

void move(Position *pos, Coordinate vector) {
    int t;
    switch (pos->direction) {
        case UP:
            break;
        case DOWN:
            vector.x = -vector.x;
            vector.y = -vector.y;
            break;
        case RIGHT:
            t = vector.y;
            vector.y = -vector.x;
            vector.x = t;
            break;
        case LEFT:
            t = -vector.y;
            vector.y = vector.x;
            vector.x =t;
            break;
    }

    switch (pos->direction) {
        case UP:
        case DOWN:
            if (vector.x > 0) {
                pos->direction = RIGHT;
            } else if (vector.x < 0) {
                pos->direction = LEFT;
            }
            break;
        case LEFT:
        case RIGHT:
            if (vector.y > 0) {
                pos->direction = UP;
            } else if (vector.y < 0) {
                pos->direction = DOWN;
            }
            break;
    }

    pos->coordinate.x += vector.x;
    pos->coordinate.y += vector.y;
}
