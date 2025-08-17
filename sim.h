#pragma once
#include <stdlib.h>

typedef struct Coordinate Coordinate;
typedef enum Direction Direction;
typedef struct Position Position;
typedef struct Behavior Behavior;
typedef struct State State;

struct Coordinate {
    int x, y;
};

enum Direction {
    UP, RIGHT, DOWN, LEFT
};

struct Position {
    Coordinate coordinate;
    Direction direction;
};

struct Behavior {
    int (*condition)(State*, int);
    void (*execution)(State*, int);
};

struct State {
    int **board;
    int rule_len, position_len, iteration;
    Position *positions;
    Behavior *rules;
};

// https://codeberg.org/NRK/slashtmp/src/branch/master/misc/safe_va_func.c
#define add_rules(S, ...) add_rules_core( \
    S, (const Behavior[]){ __VA_ARGS__ }, \
    sizeof(Behavior[]){ __VA_ARGS__ } / sizeof(Behavior) \
)

void add_rules_core(State *state, const Behavior *b, size_t n);

char* dir_str(Direction d);
State new_state(Coordinate size, Position *start, int num_positions);
void advance_state(State *state);
void move(Position *pos, Coordinate vector);
