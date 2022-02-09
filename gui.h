#ifndef GUI
#define GUI

#include "audio.h"
#include "utils.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

//-------------------------------------

#define GRID_SIZE (16 * 2)
#define WIDTH (16 * 2)
#define HEIGHT (9 * 2)
#define MARGIN 2
#define NUM_TABS 4

//-------------------------------------
// misc
//-------------------------------------
typedef SDL_FRect rect_t;

void apply_widget_margin(rect_t *R) {
  R->x += MARGIN, R->y += MARGIN, R->w -= 2 * MARGIN, R->h -= 2 * MARGIN;
}

void apply_grid_size(rect_t *R) {
  R->x *= GRID_SIZE, R->y *= GRID_SIZE, R->w *= GRID_SIZE, R->h *= GRID_SIZE;
}

typedef struct {
  int r, g, b;
} color_t;

//-------------------------------------
// gui
//-------------------------------------
struct {
  SDL_Window *win;
  SDL_Renderer *ren;
  SDL_Texture *tex;
  SDL_Texture *font;
  struct {
    SDL_Joystick *joystick;
    float left_right[2], up_down[2];
  } joystick;
  int num_widgets;
  color_t fg, bg, ac;
  int w, h;
  int tab;
  void **widgets;
  void *focus;
  bool quit, clear;
} gui;
int gui_init();
int gui_cleanup();
void gui_start();
void gui_add(void *X);
void gui_events();

#define JOYSTICK_THRESHOLD 3200
#define JOYSTICK_MAX 32768

int screen_to_world_pos_x(int x) {
  return floor((float)x / gui.w * WIDTH * GRID_SIZE);
}
int screen_to_world_pos_y(int y) {
  return floor((float)y / gui.h * HEIGHT * GRID_SIZE);
}

//-------------------------------------
void color(int r, int g, int b) {
  SDL_SetRenderDrawColor(gui.ren, r, g, b, 255);
}

void color_background() { color(gui.bg.r, gui.bg.g, gui.bg.b); }
void color_foreground() { color(gui.fg.r, gui.fg.g, gui.fg.b); }
void color_accent() { color(gui.ac.r, gui.ac.g, gui.ac.b); }

//-------------------------------------
void draw_rect(rect_t *R) { SDL_RenderFillRectF(gui.ren, R); }

//-------------------------------------
void draw_pixel(int x, int y) { SDL_RenderDrawPoint(gui.ren, x, y); }

//-------------------------------------
void draw_circle(int cx, int cy, int r) {
  cx += r, cy += r;

  int d = (r * 2);
  int x = (r - 1);
  int y = 0;
  int tx = 1;
  int ty = 1;
  int err = (tx - d);
  SDL_Rect rect;

  while (x >= y) {
    rect = (SDL_Rect){cx, cy - y, x, y};
    SDL_RenderDrawRect(gui.ren, &rect);

    rect = (SDL_Rect){cx, cy, x, y};
    SDL_RenderDrawRect(gui.ren, &rect);

    rect = (SDL_Rect){cx - x, cy - y, x, y};
    SDL_RenderDrawRect(gui.ren, &rect);

    rect = (SDL_Rect){cx - x, cy, x, y};
    SDL_RenderDrawRect(gui.ren, &rect);

    rect = (SDL_Rect){cx, cy - x, y, x};
    SDL_RenderDrawRect(gui.ren, &rect);

    rect = (SDL_Rect){cx, cy, y, x};
    SDL_RenderDrawRect(gui.ren, &rect);

    rect = (SDL_Rect){cx - y, cy - x, y, x};
    SDL_RenderDrawRect(gui.ren, &rect);

    rect = (SDL_Rect){cx - y, cy, y, x};
    SDL_RenderDrawRect(gui.ren, &rect);

    if (err <= 0) {
      ++y;
      err += ty;
      ty += 2;
    }

    if (err > 0) {
      --x;
      tx += 2;
      err += (tx - d);
    }
  }
}

//-------------------------------------
void draw_char(char c, float x, float y) {
  int i = -1, j = 0;
  if ('a' <= c && c <= 'z') {
    i = c - 'a';
  } else if ('0' <= c && c <= '9') {
    i = c - '0';
    j = 1;
  } else if (c == '.') {
    i = 10;
    j = 1;
  }

  if (i == -1)
    return;

  SDL_Rect src = {i * 16, j * 16, 16, 16};
  SDL_Rect dest = {x * GRID_SIZE, (y - 0.25) * GRID_SIZE, GRID_SIZE * 0.5,
                   GRID_SIZE * 0.5};
  SDL_RenderCopy(gui.ren, gui.font, &src, &dest);
}

//-------------------------------------
int draw_string(char *str, float x, float y) {
  int n = strlen(str);

  for (int i = 0; i < n; ++i)
    if (str[i] != '\0')
      draw_char(str[i], x + i * 0.5, y);

  return n;
}

//-------------------------------------
int draw_int(int v, float x, float y) {

  int n = 0, p = 1;
  while (p <= v && n < 9) {
    ++n;
    p *= 10;
  }
  p /= 10;
  for (int i = 0; i < n; ++i, p /= 10) {
    char c = (v / p) % 10 + '0';
    draw_char(c, x + i * 0.5, y);
  }

  return n;
}

//-------------------------------------
int draw_float(float v, float x, float y) {
  const char *fmt = "%.3f";

  int len = snprintf(NULL, 0, fmt, v) + 1;
  char *str = calloc(1, len);
  snprintf(str, len, fmt, v);
  str[len] = '\0';
  draw_string(str, x, y);
  free(str);

  return len;
}

//-------------------------------------
// widget
//-------------------------------------
typedef struct {
  void (*on_destroy)(void *X);
  void (*on_click)(void *X, int m, int x, int y);
  void (*on_key)(void *X, SDL_Keycode k);
  void (*on_drag)(void *X, int m, int x, int y);
  void (*on_draw)(void *X);
  char *name;
  int x, y, w, h, tab;
  bool dirty, outline;
} widget_t;
void widget_init(widget_t *W, int x, int y, int w, int h);
widget_t *widget_new(int x, int y, int w, int h);
void widget_draw(void *X, widget_t *W);
void widget_destroy(void *X);
rect_t widget_rect(widget_t *W);
void widget_name(void *X, char *name);

#define WIDGET_LOOP(...)                                                       \
  for (int i = 0; i < gui.num_widgets; ++i) {                                  \
    void *X = gui.widgets[i];                                                  \
    if (!X)                                                                    \
      continue;                                                                \
    widget_t *W = X;                                                           \
    __VA_ARGS__                                                                \
  }

//-------------------------------------
// layouts
//-------------------------------------
#define layout_line(x, y, gap, vert, ...)                                      \
  {                                                                            \
    void *X[] = {__VA_ARGS__};                                                 \
    layout_line_(X, LEN(X), x, y, gap, vert);                                  \
  }

void layout_line_(void *X[], int num, float x, float y, float gap, bool vert) {
  if (vert) {
    for (int i = 0; i < num; ++i) {
      widget_t *W = X[i];
      W->x = x, W->y = y;
      y += W->h + gap;
    }
  } else {
    for (int i = 0; i < num; ++i) {
      widget_t *W = X[i];
      W->x = x, W->y = y;
      x += W->w + gap;
    }
  }
}

//-------------------------------------
void layout_center(void *X) {
  widget_t *W = X;

  W->x = (WIDTH - W->w) * 0.5;
  W->y = (HEIGHT - W->h) * 0.5;
}

//-------------------------------------
// gui
//-------------------------------------
int gui_init() {
  memset(&gui, 0, sizeof(gui));

  if (SDL_Init(SDL_INIT_VIDEO) || (IMG_Init(IMG_INIT_PNG) == 0)) {
    printf("[error] unable to init SDL: %s\n", SDL_GetError());
    return -1;
  }

  // gui.fg = (color_t){0, 0, 0};
  // gui.bg = (color_t){255, 255, 255};

  gui.bg = (color_t){0, 0, 0};
  gui.fg = (color_t){255, 255, 255};
  gui.ac = (color_t){255, 0, 0};

  gui.w = GRID_SIZE * WIDTH, gui.h = GRID_SIZE * HEIGHT;
  gui.win =
      SDL_CreateWindow("compact", SDL_WINDOWPOS_CENTERED,
                       SDL_WINDOWPOS_CENTERED, gui.w, gui.h, SDL_WINDOW_SHOWN);
  if (!gui.win) {
    printf("[error] unable to create window: %s\n", SDL_GetError());
    return -1;
  }

  gui.ren = SDL_CreateRenderer(gui.win, -1, 0);
  if (!gui.ren) {
    printf("[error] unable to create renderer: %s\n", SDL_GetError());
    return -1;
  }

  gui.tex = SDL_CreateTexture(gui.ren, SDL_PIXELFORMAT_RGB24,
                              SDL_TEXTUREACCESS_TARGET, GRID_SIZE * WIDTH,
                              GRID_SIZE * HEIGHT);
  if (!gui.tex) {
    printf("[error] unable to create texture: %s\n", SDL_GetError());
    return -1;
  }

  {
    SDL_Surface *font_surf = IMG_Load("font.png");
    if (!font_surf) {
      printf("[error] unable to load font: %s\n", SDL_GetError());
      return -1;
    }
    gui.font = SDL_CreateTextureFromSurface(gui.ren, font_surf);
    if (!gui.font) {
      printf("[error] unable to create font texture: %s\n", SDL_GetError());
      return -1;
    }
    SDL_FreeSurface(font_surf);

    SDL_SetTextureColorMod(gui.font, gui.fg.r, gui.fg.g, gui.fg.b);
  }

  color_background();
  SDL_RenderClear(gui.ren);
  SDL_SetRenderTarget(gui.ren, gui.tex);
  SDL_RenderClear(gui.ren);
  SDL_SetRenderTarget(gui.ren, NULL);

  gui.num_widgets = 0, gui.widgets = NULL;
  gui.focus = NULL;
  gui.clear = true;
  gui.quit = false;

  return 0;
}

//-------------------------------------
int gui_init_joystick(int id) {
  if (id == -1) {
    SDL_InitSubSystem(SDL_INIT_JOYSTICK);

    if (SDL_NumJoysticks()) {
      printf("[gui] found %i joysticks\n", SDL_NumJoysticks());

      SDL_JoystickEventState(SDL_ENABLE);
      SDL_Joystick *joystick = NULL;
      for (int i = 0; i < SDL_NumJoysticks(); i++) {
        joystick = SDL_JoystickOpen(i);
        printf("joystick %i, %s\n", i, SDL_JoystickName(joystick));
        SDL_JoystickClose(joystick);
      }
      SDL_JoystickEventState(SDL_DISABLE);
    } else
      printf("[gui] no joysticks found!\n");

    SDL_QuitSubSystem(SDL_INIT_JOYSTICK);

    return 0;
  } else {
    SDL_InitSubSystem(SDL_INIT_JOYSTICK);

    if (SDL_NumJoysticks() == 0 || id >= SDL_NumJoysticks()) {
      printf("[gui error] num joysticks %i, id given %i\n", SDL_NumJoysticks(),
             id);
      SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
      return -1;
    }

    SDL_JoystickEventState(SDL_ENABLE);
    gui.joystick.joystick = SDL_JoystickOpen(id);
    return 0;
  }
}

//-------------------------------------
void gui_add(void *X) {
  gui.num_widgets++;

  if (gui.widgets)
    gui.widgets = realloc(gui.widgets, gui.num_widgets * sizeof(void *));
  else
    gui.widgets = calloc(gui.num_widgets, sizeof(void *));

  gui.widgets[gui.num_widgets - 1] = X;
}

//-------------------------------------
void gui_tab(int tab) {
  WIDGET_LOOP({ W->dirty = true; })
  gui.tab = CLIP(tab, 0, NUM_TABS);
  gui.clear = true;
}

//-------------------------------------
void gui_events() {
  SDL_Event evt;

  while (SDL_PollEvent(&evt)) {
    switch (evt.type) {

      // system
    case SDL_QUIT:
      gui.quit = true;
      break;

    case SDL_WINDOWEVENT: {
      switch (evt.window.event) {
      case SDL_WINDOWEVENT_RESIZED:
        gui.w = evt.window.data1, gui.h = evt.window.data2;
        break;

      case SDL_WINDOWEVENT_LEAVE:
        if (gui.focus)
          gui.focus = NULL;
        break;

      default:
        break;
      }
    } break;

      // keyboard
    case SDL_KEYDOWN: {
      SDL_Keycode k = evt.key.keysym.sym;
      if (k == 'q') {
        gui.quit = true;
      } else if ('1' <= k && k <= '9') {
        gui_tab(k - '1');
      }
    } break;

    // mouse
    case SDL_MOUSEBUTTONDOWN: {
      int m = evt.button.button;
      int mx = screen_to_world_pos_x(evt.button.x);
      int my = screen_to_world_pos_y(evt.button.y);

      gui.focus = NULL;

      WIDGET_LOOP({
        if (W->tab != gui.tab)
          continue;

        rect_t r = widget_rect(W);

        if (contains(r.x, r.y, r.w, r.h, mx, my)) {
          if (W->on_click) {
            W->on_click(gui.widgets[i], m, mx - r.x, my - r.y);
            gui.focus = gui.widgets[i];
          }
          break;
        }
      })
    } break;

    case SDL_MOUSEMOTION: {
      if (gui.focus) {
        widget_t *W = gui.focus;

        if (W->on_drag && W->tab == gui.tab) {
          int m = evt.motion.state;
          int mx = screen_to_world_pos_x(evt.motion.x) - W->x * GRID_SIZE;
          int my = screen_to_world_pos_y(evt.motion.y) - W->y * GRID_SIZE;
          W->on_drag(gui.focus, m, mx, my);
        }
      }
    } break;

    case SDL_MOUSEBUTTONUP: {
      if (gui.focus)
        gui.focus = NULL;
    } break;

      // joystick
    case SDL_JOYAXISMOTION: {
      float value = 0;
      if (evt.jaxis.value < -JOYSTICK_THRESHOLD ||
          evt.jaxis.value > JOYSTICK_THRESHOLD)
        value = scale(evt.jaxis.value, -JOYSTICK_MAX, JOYSTICK_MAX, -1, 1);

      if (evt.jaxis.axis == 0)
        gui.joystick.left_right[0] = value;
      else if (evt.jaxis.axis == 1)
        gui.joystick.up_down[0] = value;
      else if (evt.jaxis.axis == 3)
        gui.joystick.left_right[1] = value;
      else if (evt.jaxis.axis == 4)
        gui.joystick.up_down[1] = value;

    } break;

      // end
    }
  }
}

//-------------------------------------
extern void gui_callback();

//-------------------------------------
void gui_grid() {
  color_accent();
  for (int i = 0; i < WIDTH; ++i)
    SDL_RenderDrawLine(gui.ren, i * GRID_SIZE, 0, i * GRID_SIZE,
                       HEIGHT * GRID_SIZE);
  for (int i = 0; i < HEIGHT; ++i)
    SDL_RenderDrawLine(gui.ren, 0, i * GRID_SIZE, WIDTH * GRID_SIZE,
                       i * GRID_SIZE);
}

//-------------------------------------
void gui_start() {
  while (!gui.quit) {
    SDL_SetRenderTarget(gui.ren, gui.tex);

    gui_events();

    if (gui.clear) {
      color_background();
      SDL_RenderClear(gui.ren);
      gui.clear = false;

      color_accent();
      draw_int(gui.tab + 1, WIDTH - 1, 1);
      color_background();
    }

    WIDGET_LOOP({
      if (W->tab == gui.tab)
        widget_draw(X, W);
    });

    gui_callback();

    SDL_SetRenderTarget(gui.ren, NULL);

    color_background();
    SDL_RenderClear(gui.ren);

    SDL_RenderCopy(gui.ren, gui.tex, NULL, NULL);

    SDL_RenderPresent(gui.ren);
    SDL_Delay((float)1000 / 60);
  }
}

//-------------------------------------
int gui_cleanup() {
  SDL_DestroyTexture(gui.font);
  SDL_DestroyTexture(gui.tex);
  SDL_DestroyRenderer(gui.ren);
  SDL_DestroyWindow(gui.win);

  if (gui.joystick.joystick)
    SDL_JoystickClose(gui.joystick.joystick);

  if (gui.widgets) {
    WIDGET_LOOP({ widget_destroy(X); });
    free(gui.widgets);
  }

  IMG_Quit();
  SDL_Quit();
  return 0;
}

//-------------------------------------
// widget
//-------------------------------------
void widget_init(widget_t *W, int x, int y, int w, int h) {
  memset(W, 0, sizeof(widget_t));

  W->x = x, W->y = y, W->w = w, W->h = h;
  W->dirty = true, W->outline = true;
}

//-------------------------------------
widget_t *widget_new(int x, int y, int w, int h) {
  widget_t *W = calloc(1, sizeof(widget_t));
  widget_init(W, x, y, w, h);
  return W;
}

//-------------------------------------
void widget_draw(void *X, widget_t *W) {
  if (W->dirty) {
    rect_t r = widget_rect(W);

    if (W->outline) {
      color_foreground();
      draw_rect(&r);

      apply_widget_margin(&r);
      color_background();
      draw_rect(&r);
    } else {
      color_background();
      draw_rect(&r);
    }

    if (W->name)
      draw_string(W->name, W->x, W->y - 0.5);

    W->dirty = false;
    if (W->on_draw)
      W->on_draw(X);
  }
}

//-------------------------------------
rect_t widget_rect(widget_t *W) {
  return (rect_t){W->x * GRID_SIZE, W->y * GRID_SIZE, W->w * GRID_SIZE,
                  W->h * GRID_SIZE};
}

//-------------------------------------
void widget_name(void *X, char *name) {
  widget_t *W = X;
  W->name = name, W->dirty = true;
}

//-------------------------------------
void widget_destroy(void *X) {
  widget_t *W = X;

  if (W->on_destroy)
    W->on_destroy(X);

  FREE(X);
}

//-------------------------------------
// label
//-------------------------------------
typedef struct {
  widget_t W;
  char *value;
  bool must_free;
} label_t;
typedef label_t *label_p;

void label_init(label_t *L, int x, int y);
label_t *label_new(int x, int y);
void label_draw(void *X);
void label_set(label_t *L, char *value);
void label_set_float(label_t *L, float f);
void label_destroy(void *X);

//-------------------------------------
void label_init(label_t *L, int x, int y) {
  ZERO(L, label_t);
  widget_init(&L->W, x, y, 0, 1);

  L->W.outline = false;
  L->W.on_draw = label_draw;
  L->W.on_destroy = label_destroy;
}

//-------------------------------------
label_t *label_new(int x, int y) {
  label_t *L = calloc(1, sizeof(label_t));
  label_init(L, x, y);

  gui_add(L);
  return L;
}

//-------------------------------------
void label_draw(void *X) {
  label_t *L = X;

  if (L->value)
    draw_string(L->value, L->W.x, L->W.y + 0.5);
}

//-------------------------------------
void label_set(label_t *L, char *value) {
  L->value = value, L->W.dirty = true;

  size_t len = strlen(value);
  L->W.w = len;
}

//-------------------------------------
void label_set_float(label_t *L, float v) {
  if (L->must_free)
    FREE(L->value);

  const char *fmt = "%.3f";

  int len = snprintf(NULL, 0, fmt, v) + 1;
  L->value = calloc(1, len);
  snprintf(L->value, len, fmt, v);
  L->value[len] = '\0';

  L->W.dirty = true;
  L->W.w = len;
  L->must_free = true;
}

//-------------------------------------
void label_set_int(label_t *L, int v) {
  if (L->must_free)
    FREE(L->value);

  const char *fmt = "%i";

  int len = snprintf(NULL, 0, fmt, v) + 1;
  L->value = calloc(1, len);
  snprintf(L->value, len, fmt, v);
  L->value[len] = '\0';

  L->W.dirty = true;
  L->W.w = len;
  L->must_free = true;
}

//-------------------------------------
void label_destroy(void *X) {
  label_t *L = X;

  if (L->must_free)
    FREE(L->value);
}

//-------------------------------------
// button
//-------------------------------------
typedef struct {
  widget_t W;
  void (*on_click)(bool value);
  int flash;
  bool toggle, value;
} button_t;
typedef button_t *button_p;

void button_init(button_t *B, int x, int y, int w, int h);
button_t *button_new(int x, int y, int w, int h);
void button_draw(void *X);
void button_click(void *X, int m, int x, int y);
void button_set(button_t *B, bool value);
void button_flash(button_t *B);

#define BUTTON_FLASH 5

//-------------------------------------
void button_init(button_t *B, int x, int y, int w, int h) {
  ZERO(B, button_t);
  widget_init(&B->W, x, y, w, h);

  B->W.on_draw = button_draw;
  B->W.on_click = button_click;
}

//-------------------------------------
button_t *button_new(int x, int y, int w, int h) {
  button_t *B = calloc(1, sizeof(button_t));
  button_init(B, x, y, w, h);

  gui_add(B);
  return B;
}

//-------------------------------------
void button_draw(void *X) {
  button_t *B = X;

  if ((B->toggle && B->value) || B->flash) {
    widget_t *W = &B->W;
    rect_t r = widget_rect(W);
    apply_widget_margin(&r);
    color_accent();

    if (B->flash) {
      B->flash--;
      B->W.dirty = true;
    }

    draw_rect(&r);
  }
}

//-------------------------------------
void button_set(button_t *B, bool value) {
  B->value = value;
  B->W.dirty = true;
}

//-------------------------------------
void button_flash(button_t *B) {
  B->flash = BUTTON_FLASH;
  B->W.dirty = true;
}

//-------------------------------------
void button_click(void *X, int m, int x, int y) {
  button_t *B = X;

  if (B->toggle)
    button_set(B, !B->value);
  else
    button_flash(B);

  if (B->on_click)
    B->on_click(B->value);
}

//-------------------------------------
// slider
//-------------------------------------
typedef struct {
  widget_t W;
  void (*on_change)(void *X, float value);
  bool vert;
  float value;
} slider_t;
typedef slider_t *slider_p;

slider_t *slider_new(int x, int y, int w, int h);
void slider_draw(void *X);
void slider_click(void *X, int m, int x, int y);
void slider_drag(void *X, int m, int x, int y);
void slider_set(slider_t *S, float value);
void slider_set_vert(slider_t *S, bool vert);

//-------------------------------------
slider_t *slider_new(int x, int y, int w, int h) {
  slider_t *S = calloc(1, sizeof(slider_t));
  widget_init(&S->W, x, y, w, h);

  S->W.on_draw = slider_draw;
  S->W.on_click = slider_click;
  S->W.on_drag = slider_drag;

  gui_add(S);
  return S;
}

//-------------------------------------
void slider_draw(void *X) {
  slider_t *S = X;
  widget_t *W = &S->W;

  rect_t r = widget_rect(W);
  apply_widget_margin(&r);

  color_accent();

  if (S->vert) {
    int h = (int)floor(r.h * S->value);
    r.y += r.h - h;
    r.h = h;
    draw_rect(&r);
  } else {
    int w = (int)floor(r.w * S->value);
    r.w = w;
    draw_rect(&r);
  }
}

//-------------------------------------
void slider_click(void *X, int m, int x, int y) {
  slider_t *S = X;

  if (m == 1) {
    if (S->vert)
      slider_set(S, 1 - (float)y / (S->W.h * GRID_SIZE));
    else
      slider_set(S, (float)x / (S->W.w * GRID_SIZE));
  } else if (m == 2)
    slider_set(S, frand(0.0, 1.0));
}

//-------------------------------------
void slider_drag(void *X, int m, int x, int y) { slider_click(X, m, x, y); }

//-------------------------------------
void slider_set(slider_t *S, float value) {
  if (S->value != value) {
    S->value = CLIP(value, 0, 1);
    S->W.dirty = true;
    if (S->on_change)
      S->on_change(S, S->value);
  }
}

//-------------------------------------
void slider_set_vert(slider_t *S, bool vert) {
  if (S->vert != vert) {
    S->vert = vert;
    S->W.dirty = true;
  }
}

//-------------------------------------
// multislider
//-------------------------------------
typedef struct {
  widget_t W;
  void (*on_change)(void *X, int id, float value);
  bool vert;
  int size;
  float *value;
} multislider_t;
typedef multislider_t *multislider_p;

multislider_t *multislider_new(int x, int y, int w, int h);
void multislider_destroy(void *X);
void multislider_draw(void *X);
void multislider_click(void *X, int m, int x, int y);
void multislider_drag(void *X, int m, int x, int y);
void multislider_resize(multislider_t *M, int size);
float multislider_get(multislider_t *M, int id);
void multislider_set(multislider_t *M, int id, float value);
void multislider_set_all(multislider_t *M, int size, float *array);
void multislider_set_vert(multislider_t *M, bool vert);

//-------------------------------------
multislider_t *multislider_new(int x, int y, int w, int h) {
  multislider_t *M = calloc(1, sizeof(multislider_t));
  widget_init(&M->W, x, y, w, h);

  M->W.on_click = multislider_click;
  M->W.on_drag = multislider_drag;
  M->W.on_draw = multislider_draw;
  M->W.on_destroy = multislider_destroy;

  gui_add(M);
  return M;
}

//-------------------------------------
void multislider_destroy(void *X) {
  multislider_t *M = X;
  FREE(M->value);
}

//-------------------------------------
void multislider_resize(multislider_t *M, int size) {
  FREE(M->value);

  if (size <= 0) {
    M->size = 0;
    return;
  }

  M->size = size;
  M->value = calloc(M->size, sizeof(float));
  M->W.dirty = true;
}

//-------------------------------------
void multislider_draw(void *X) {
  multislider_t *M = X;

  if (!M->value)
    return;

  rect_t r = widget_rect(&M->W);
  apply_widget_margin(&r);

  color_accent();

  if (M->vert) {
    float uy = (float)r.h / M->size;
    for (int i = 0; i < M->size; ++i) {
      int w = (int)floor(r.w * M->value[i]);
      rect_t s = {r.x, r.y + uy * i, w, uy};
      draw_rect(&s);
    }
  } else {
    float ux = (float)r.w / M->size;
    for (int i = 0; i < M->size; ++i) {
      float h = (float)r.h * M->value[i];
      rect_t s = {r.x + ux * i, r.y + r.h - h, ux, h};
      draw_rect(&s);
    }
  }
}

//-------------------------------------
void multislider_click(void *X, int m, int x, int y) {
  multislider_t *M = X;

  if (!M->value || M->size == 0)
    return;

  if (M->vert) {
    int id = (float)y / (M->W.h * GRID_SIZE) * M->size;
    float value = (float)x / (M->W.w * GRID_SIZE);
    multislider_set(M, id, value);
  } else {
    int id = (float)x / (M->W.w * GRID_SIZE) * M->size;
    float value = 1.0 - (float)y / (M->W.h * GRID_SIZE);
    multislider_set(M, id, value);
  }
}

//-------------------------------------
void multislider_drag(void *X, int m, int x, int y) {
  multislider_click(X, m, x, y);
}

//-------------------------------------
float multislider_get(multislider_t *M, int id) {
  if (0 <= id && id < M->size && M->value)
    return M->value[id];
  else
    return -1.0;
}

//-------------------------------------
void multislider_set(multislider_t *M, int id, float value) {
  if (0 <= id && id < M->size && M->value) {
    M->value[id] = CLIP(value, 0, 1);
    M->W.dirty = true;
  }
}

//-------------------------------------
void multislider_set_vert(multislider_t *M, bool vert) {
  if (M->vert != vert) {
    M->vert = vert;
    M->W.dirty = true;
  }
}

//-------------------------------------
void multislider_set_all(multislider_t *M, int size, float array[]) {
  if (!size)
    return;

  if (M->size != size)
    multislider_resize(M, size);

  for (int i = 0; i < size; ++i)
    M->value[i] = array[i];

  M->W.dirty = true;
}

//-------------------------------------
// led
//-------------------------------------
typedef struct {
  widget_t W;
  int value, dur;
} led_t;
typedef led_t *led_p;

#define LED_FLASH 10

led_t *led_new(int x, int y, int w, int h);
void led_draw(void *X);
void led_click(void *X, int m, int x, int y);
void led_trigger(led_t *L);

//-------------------------------------
led_t *led_new(int x, int y, int w, int h) {
  led_t *L = calloc(1, sizeof(led_t));
  widget_init(&L->W, x, y, w, h);

  L->dur = LED_FLASH;
  L->W.on_draw = led_draw;
  L->W.on_click = led_click;

  gui_add(L);
  return L;
}

//-------------------------------------
void led_draw(void *X) {
  led_t *L = X;

  if (L->value) {
    float p = (float)L->value / LED_FLASH;
    rect_t r = widget_rect(&L->W);
    apply_widget_margin(&r);
    color_accent();
    r.x += (1 - p) * 0.5 * r.w;
    r.y += (1 - p) * 0.5 * r.h;
    r.w *= p, r.h *= p;
    draw_rect(&r);

    L->value--;
    L->W.dirty = true;
  }
}

//-------------------------------------
void led_trigger(led_t *L) {
  L->value = L->dur;
  L->W.dirty = true;
}

//-------------------------------------
void led_click(void *X, int m, int x, int y) { led_trigger(X); }

//-------------------------------------
// array
//-------------------------------------
typedef struct {
  widget_t W;
  uint len, chans;
  float *data, min, max;
  struct {
    float start, end;
  } * ranges;
  uint num_ranges;
  bool dynamic, edit;
} array_t;
typedef array_t *array_p;

array_t *array_new(int x, int y, int w, int h);
void array_destroy(void *X);
void array_draw(void *X);
void array_click(void *X, int m, int x, int y);
void array_set(array_t *A, uint len, uint chans, float *data, float min,
               float max);
void array_set_buf(array_t *A, buffer_t *buf);
void array_set_range(array_t *A, float start, float end);
void array_set_ranges(array_t *A, uint id, float start, float end);
void array_set_num_ranges(array_t *A, uint num);
float array_get(int id, int chan);

//-------------------------------------
array_t *array_new(int x, int y, int w, int h) {
  array_t *A = calloc(1, sizeof(array_t));
  widget_init(&A->W, x, y, w, h);

  A->W.on_destroy = array_destroy;
  A->W.on_draw = array_draw;
  A->W.on_click = array_click;
  A->W.on_drag = array_click;
  A->min = 0, A->max = 1;

  gui_add(A);
  return A;
}

//-------------------------------------
void array_destroy(void *X) {
  array_t *A = X;
  FREE(A->ranges);
}

//-------------------------------------
void array_draw(void *X) {
  array_t *A = X;

  if (!A->data)
    return;

  rect_t r = widget_rect(&A->W);
  apply_widget_margin(&r);

  color_foreground();
  if (A->ranges) {
    for (int i = 0; i < A->num_ranges; ++i) {
      rect_t s = {
          r.x + A->ranges[i].start * r.w,
          r.y,
          r.w * (A->ranges[i].end - A->ranges[i].start),
          r.h,
      };
      draw_rect(&s);
    }
  }

  color_accent();
  int len = MIN(A->len, r.w);
  float ux = (float)r.w / len;
  float u = (float)A->len / len;

  if (A->chans == 1) {
    for (int i = 0; i < len; ++i) {
      int I = (float)i / len * A->len;
      float v = clip_scale(A->data[I], A->min, A->max, 0, 1);

      rect_t s = {
          r.x + i * ux,
          r.h + r.y - (r.h * v),
          ux,
          r.h * v,
      };

      draw_rect(&s);
    }
  } else {
    float h = (float)r.h / A->chans;

    for (int c = 0; c < A->chans; ++c) {
      color_accent();

      for (int i = 0; i < len; ++i) {
        int I = i * u * A->chans + c;
        float v = clip_scale(A->data[I], A->min, A->max, 0, 1);

        rect_t s = {
            r.x + i * ux,
            (h * (c + 1)) + r.y - (h * v),
            ux,
            h * v,
        };

        draw_rect(&s);
      }

      if (c > 0) {
        rect_t s = {r.x, r.y + h * c, r.w, 1};
        color_foreground();
        draw_rect(&s);
      }
    }
  }

  if (A->dynamic)
    A->W.dirty = true;
}

//-------------------------------------
void array_click(void *X, int m, int x, int y) {
  array_t *A = X;

  if (!A->data)
    return;

  x = CLIP(x, 0, A->W.w * GRID_SIZE);

  if (m == 1) {
    if (A->edit) {
      if (A->chans == 1) {
        int id = (float)x / (A->W.w * GRID_SIZE) * A->len;
        float value = 1.0 - (float)y / (A->W.h * GRID_SIZE);

        if (m == 1)
          A->data[id] = scale(value, 0, 1, A->min, A->max);
        else
          A->data[id] = frand(A->min, A->max);
      } else {
        int id = (float)x / (A->W.w * GRID_SIZE) * A->len * A->chans + 0;
        float value = 1.0 - (float)y / (A->W.h * GRID_SIZE);

        A->data[id] = scale(value, 0, 1, A->min, A->max);
      }
    }
  } else if (m == 3 || m == 4) {
    if (A->ranges) {
      float p = (float)x / (A->W.w * GRID_SIZE);

      if (A->num_ranges == 1) {
        float start = A->ranges[0].start;
        float end = A->ranges[0].end;

        if (ABS(start - p) < ABS(end - p))
          array_set_range(A, p, end);
        else
          array_set_range(A, start, p);
      } else {
      }
    }
  }

  A->W.dirty = true;
}

//-------------------------------------
void array_set(array_t *A, uint len, uint chans, float *data, float min,
               float max) {
  if (!data || len == 0 || chans == 0)
    return;

  A->len = len;
  A->chans = chans;
  A->data = data;
  A->min = min;
  A->max = max;
}

//-------------------------------------
void array_set_buf(array_t *A, buffer_t *buf) {
  if (!buf->data)
    return;

  array_set(A, buf->len, buf->chans, buf->data, -1, 1);
}

//-------------------------------------
void array_set_ranges(array_t *A, uint id, float start, float end) {
  if (id >= A->num_ranges)
    return;

  A->ranges[id].start = MIN(start, end), A->ranges[id].end = MAX(start, end);
  A->W.dirty = true;
}

//-------------------------------------
void array_set_range(array_t *A, float start, float end) {
  if (!A->ranges)
    array_set_num_ranges(A, 1);

  array_set_ranges(A, 0, start, end);
}

//-------------------------------------
void array_set_num_ranges(array_t *A, uint num) {
  if (A->ranges)
    A->ranges = realloc(A->ranges, num * 2 * sizeof(float));
  else
    A->ranges = calloc(num * 2, sizeof(float));

  A->num_ranges = num;
  A->W.dirty = true;
}

#endif
