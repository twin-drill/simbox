#include "sim.h"

const Coordinate left = {-1, 0};
const Coordinate right = {1, 0};

int langton_condition(State *st, int pos_index) {
    return 1;
}

void langton_exec(State *st, int pos_index) {
    int x = st->positions[pos_index].coordinate.x, y = st->positions[pos_index].coordinate.y;
    if (st->board[y][x]) {
        st->board[y][x] = 0;
        move(&(st->positions[pos_index]), left);
    } else {
        st->board[y][x] = 1;
        move(&(st->positions[pos_index]), right);
    }
}

const Behavior langton = { langton_condition, langton_exec };

int register_langton(State *st) {
    int initial_rules = st->rule_len;
    add_rules(st, langton);
    if (st->rule_len != initial_rules + 1) return -1;
    return 0;
}