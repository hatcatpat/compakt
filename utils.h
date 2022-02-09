#ifndef UTILS
#define UTILS

#define _GNU_SOURCE
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

//-------------------------------------
typedef unsigned int uint;

#define PI 3.14159265358979323846
#define TAU (2.0 * PI)
#define SQRT_2 1.4142135623730951
#define EULER 2.71828182846

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define CLIP(v, a, b) ((v) < (a) ? (a) : (v) > (b) ? (b) : (v))
#define SQR(a) ((a) * (a))
#define BIT(a) (1 << a)
#define ABS(a) ((a < 0 ? -(a) : (a)))
#define LEN(a) (sizeof((a)) / sizeof((a)[0]))

#define FREE(x)                                                                \
  if (x) {                                                                     \
    free(x);                                                                   \
    x = NULL;                                                                  \
  }

#define ZERO(x, t) memset(x, 0, sizeof(t));
#define NEW(t) calloc(1, sizeof(t));

#define loop(var, num) for (int var = 0; var < num; ++var)

//-------------------------------------
float nearest(float v, float r) { return r * floorf(v / r); }

int irand(int min, int max) { return rand() % (max - min) + min; }

float frand(float min, float max) {
  return ((float)rand() / (float)RAND_MAX) * (max - min) + min;
}
float norm_rand() { return frand(0.0, 1.0); }
float bi_rand() { return frand(-1.0, 1.0); }

float scale(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

bool chance(float prob) { return frand(0, 1) < prob; }

float clip_scale(float x, float in_min, float in_hi, float out_min,
                 float out_max) {
  return CLIP(scale(x, in_min, in_hi, out_min, out_max), out_min, out_max);
}

float bi2norm(float b) { return (b + 1.0) * 0.5; }
float norm2bi(float n) { return n * 2.0 - 1.0; }

float scale_norm(float n, float min, float max) {
  return n * (max - min) + min;
}
float scale_bi(float b, float min, float max) {
  return scale_norm(bi2norm(b), min, max);
}

bool contains(float x1, float y1, float w, float h, float x2, float y2) {
  return x1 <= x2 && x2 <= x1 + w && y1 <= y2 && y2 <= y1 + h;
}

void delay(int ms) { usleep(ms); }

//-------------------------------------
typedef struct {
  float radius, angle;
} polar_t;
polar_t make_polar(float radius, float angle) {
  return (polar_t){radius, angle};
}

typedef struct {
  float x, y;
} carte_t;
carte_t make_carte(float x, float y) { return (carte_t){x, y}; }

carte_t polar2carte(polar_t P) {
  return make_carte(P.radius * cos(P.angle), P.radius * sin(P.angle));
}

polar_t carte2polar(carte_t C) {
  return make_polar(sqrt(SQR(C.x) + SQR(C.y)), atan2(C.y, C.x));
}

//-------------------------------------
extern int gui_init();
extern void gui_start();
extern int gui_cleanup();

extern int audio_init();
extern void audio_start();
extern void audio_stop();
extern int audio_cleanup();

//-------------------------------------
int init() {
  srand(time(NULL));

  if (gui_init())
    return -1;

  if (audio_init())
    return -1;

  return 0;
}

//-------------------------------------
void start() {
  audio_start();
  gui_start();
  audio_stop();
}

//-------------------------------------
int cleanup() {
  int result = 0;

  if (gui_cleanup())
    result = -1;

  if (audio_cleanup())
    result = -1;

  return result;
}

#endif
