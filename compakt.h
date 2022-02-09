#ifndef COMPACT
#define COMPACT

#include "audio.h"
#include "gui.h"
#include "midi.h"
#include "utils.h"

//-------------------------------------
// seq
//-------------------------------------
typedef struct {
  void **X;
  union {
    bool value_b;
    float value_f;
  };
  int step;
  uint pos, size;
} seq_t;

//-------------------------------------
void seq_init(seq_t *S, int x, int y, uint size) {
  ZERO(S, seq_t);

  S->size = size;
  S->step = 1;
  S->pos = 0;
}

//-------------------------------------
void seq_init_buttons(seq_t *S, int x, int y, uint size) {
  seq_init(S, x, y, size);
  S->value_b = false;

  S->X = calloc(S->size, sizeof(button_t *));
  for (int i = 0; i < S->size; ++i) {
    button_t *B = button_new(0, 0, 1, 1);
    B->toggle = true;
    S->X[i] = B;
  }
  layout_line_(S->X, size, x, y, 0, false);
}

//-------------------------------------
void seq_init_sliders(seq_t *S, int x, int y, uint size) {
  seq_init(S, x, y, size);

  S->value_f = 0.0;

  S->X = calloc(S->size, sizeof(slider_t *));
  for (int i = 0; i < S->size; ++i) {
    slider_t *Q = slider_new(0, 0, 1, 4);
    slider_set_vert(Q, true);
    S->X[i] = Q;
  }
  layout_line_(S->X, size, x, y, 0, false);
}

//-------------------------------------
void seq_step(seq_t *S) {
  S->pos += S->step;
  if (S->pos >= S->size)
    S->pos = 0;
  else if (S->pos < 0)
    S->pos = S->size - 1;
}

//-------------------------------------
bool seq_step_b(seq_t *S) {
  button_t *B = S->X[S->pos];
  S->value_b = B->value;
  seq_step(S);
  return S->value_b;
}

//-------------------------------------
float seq_step_f(seq_t *S) {
  slider_t *Q = S->X[S->pos];
  S->value_f = Q->value;
  seq_step(S);
  return S->value_f;
}

//-------------------------------------
seq_t *seq_new_b(int x, int y, uint size) {
  seq_t *S = malloc(sizeof(seq_t));
  seq_init_buttons(S, x, y, size);
  return S;
}

//-------------------------------------
seq_t *seq_new_f(int x, int y, uint size) {
  seq_t *S = malloc(sizeof(seq_t));
  seq_init_sliders(S, x, y, size);
  return S;
}

//-------------------------------------
void seq_destroy(seq_t *S) { FREE(S->X); }

//-------------------------------------

#endif
