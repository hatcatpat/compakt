#ifndef AUDIO
#define AUDIO

#include "utils.h"

#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

#include <jack/jack.h>

#include <fftw3.h>

//-------------------------------------
// audio
//-------------------------------------
struct {
  jack_client_t *client;
  jack_port_t *port_in[2], *port_out[2];
  const float *buf_in[2];
  float *buf_out[2];
  double rate;
  int frames, pos;
} audio;
int audio_init();
int audio_cleanup();
void audio_start();
void audio_stop();

extern void audio_callback();

static int jack_callback(jack_nframes_t frames, void *arg);
static void jack_shutdown(void *arg) { audio_cleanup(); }

#define audio_loop for (audio.pos = 0; audio.pos < audio.frames; ++audio.pos)

//-------------------------------------
// sample
//-------------------------------------
typedef struct {
  float value[2];
} sample_t;

#define sample_loop for (uint c = 0; c < 2; ++c)

//-------------------------------------
// audio
//-------------------------------------
static int jack_callback(jack_nframes_t frames, void *arg) {
  static uint c = 0;
  for (c = 0; c < 2; ++c) {
    audio.buf_in[c] = jack_port_get_buffer(audio.port_in[c], frames);
    audio.buf_out[c] = jack_port_get_buffer(audio.port_out[c], frames);
    memset(audio.buf_out[c], 0, frames * sizeof(float));
  }

  audio.frames = frames;
  audio_callback();

  return 0;
}

//-------------------------------------
int audio_init() {
  memset(&audio, 0, sizeof(audio));

  const char *client_name = "compact";
  const char *server_name = NULL;
  jack_options_t options = JackNullOption;
  jack_status_t status;

  audio.client = jack_client_open(client_name, options, &status, server_name);

  if (!audio.client) {
    printf("jack_client_open() failed, status = 0x%2.0x\n", status);
    if (status & JackServerFailed)
      printf("Unable to connect to JACK server\n");

    return -1;
  }

  if (status & JackServerStarted)
    printf("JACK server started\n");

  if (status & JackNameNotUnique) {
    client_name = jack_get_client_name(audio.client);
    printf("unique name `%s' assigned\n", client_name);
  }

  jack_set_process_callback(audio.client, jack_callback, 0);

  jack_on_shutdown(audio.client, jack_shutdown, 0);

  for (int c = 0; c < 2; ++c) {
    audio.port_in[c] =
        jack_port_register(audio.client, c == 0 ? "in_left" : "in_right",
                           JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);

    audio.port_out[c] =
        jack_port_register(audio.client, c == 0 ? "out_left" : "out_right",
                           JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

    if (!audio.port_in[c] || !audio.port_out[c]) {
      printf("no more JACK ports available\n");
      return -1;
    }
  }

  audio.rate = (double)jack_get_sample_rate(audio.client);

  printf("init audio jack: successful!\n");
  return 0;
}

//-------------------------------------
int audio_cleanup() {
  jack_client_close(audio.client);
  return 0;
}

//-------------------------------------
void audio_start() {
  if (jack_activate(audio.client))
    return;

  const char **ports;

  // input
  {
    ports = jack_get_ports(audio.client, NULL, NULL,
                           JackPortIsPhysical | JackPortIsOutput);
    if (!ports) {
      printf("no physical capture ports\n");
      return;
    }

    for (int c = 0; c < 2; ++c)
      if (jack_connect(audio.client, ports[c],
                       jack_port_name(audio.port_in[c])))
        printf("cannot connect input port %i\n", c);

    free(ports);
  }

  // output
  {
    ports = jack_get_ports(audio.client, NULL, NULL,
                           JackPortIsPhysical | JackPortIsInput);
    if (!ports) {
      printf("no physical playback ports\n");
      return;
    }

    for (int c = 0; c < 2; ++c)
      if (jack_connect(audio.client, jack_port_name(audio.port_out[c]),
                       ports[c]))
        printf("cannot connect output port %i\n", c);

    free(ports);
  }
}

//-------------------------------------
void audio_stop() {
  for (int c = 0; c < 2; ++c) {
    jack_port_disconnect(audio.client, audio.port_in[c]);
    jack_port_disconnect(audio.client, audio.port_in[c]);
  }
}

//-------------------------------------
// misc
//-------------------------------------
sample_t sample_zero = {0, 0};

sample_t make_sample1(float v) { return (sample_t){v, v}; }
sample_t make_sample(float l, float r) { return (sample_t){l, r}; }

#define DEFINE_SAMPLE_BINOP(op, name)                                          \
  sample_t sample_##name(sample_t a, sample_t b) {                             \
    return (sample_t){a.value[0] op b.value[0], a.value[1] op b.value[1]};     \
  }                                                                            \
  sample_t sample_##name##_s(sample_t a, float b) {                            \
    return (sample_t){a.value[0] op b, a.value[1] op b};                       \
  }

DEFINE_SAMPLE_BINOP(+, add)
DEFINE_SAMPLE_BINOP(-, sub)
DEFINE_SAMPLE_BINOP(*, mul)
DEFINE_SAMPLE_BINOP(/, div)

#undef DEFINE_SAMPLE_BINOP

//-------------------------------------
sample_t sample_clip(sample_t a, float min, float max) {
  sample_loop a.value[c] = CLIP(a.value[c], min, max);
  return a;
}

//-------------------------------------
sample_t sample_wrap(sample_t a, float min, float max) {
  sample_loop {
    if (a.value[c] < min)
      a.value[c] = max - fmod(min - a.value[c], max - min);
    else if (a.value[c] > max)
      a.value[c] = min + fmod(a.value[c] - max, max - min);
  }
  return a;
}

//-------------------------------------
sample_t sample_lincomb(sample_t a, float x, sample_t b, float y) {
  sample_loop a.value[c] = a.value[c] * x + b.value[c] * y;
  return a;
}

//-------------------------------------
sample_t effect_overdrive(sample_t input, float amt) {
  return sample_clip(sample_mul_s(input, amt), -1, 1);
}

//-------------------------------------
sample_t effect_fold(sample_t input, float amt) {
  sample_loop {
    if (input.value[c] > amt)
      input.value[c] = amt - (input.value[c] - amt);
    else if (input.value[c] < -1.0)
      input.value[c] = -(amt - (input.value[c] - amt));
  }

  return input;
}

//-------------------------------------
sample_t effect_bit(sample_t input, int bit) {
  sample_loop { input.value[c] = nearest(input.value[c], (float)1.0 / bit); }
  return input;
}

//-------------------------------------
bool audio_every(int t) { return audio.pos % t == 0; }

//-------------------------------------
void audio_out(sample_t s) {
  sample_loop audio.buf_out[c][audio.pos] += s.value[c];
}

//-------------------------------------
void audio_set(sample_t s) {
  sample_loop audio.buf_out[c][audio.pos] = s.value[c];
}

//-------------------------------------
sample_t audio_get() {
  sample_t s = sample_zero;
  sample_loop s.value[c] = audio.buf_out[c][audio.pos];
  return s;
}

//-------------------------------------
sample_t audio_in() {
  sample_t s = {0, 0};
  sample_loop s.value[c] = audio.buf_in[c][audio.pos];
  return s;
}

//-------------------------------------
int sec2samp(float sec) { return (int)floorf(sec * audio.rate); }
int samp2sec(int samp) { return (float)(samp / audio.rate); }

//-------------------------------------
// gate
//-------------------------------------
typedef struct {
  bool value;
  float prev, threshold;
} gate_t;
typedef gate_t *gate_p;

//-------------------------------------
void gate_init(gate_t *G) { ZERO(G, gate_t); }

//-------------------------------------
gate_t *gate_new() {
  gate_t *G = malloc(sizeof(gate_t));
  gate_init(G);
  return G;
}

//-------------------------------------
bool gate_update(gate_t *G, float input) {
  G->value = G->prev < G->threshold && input >= G->threshold;
  G->prev = input;

  return G->value;
}

//-------------------------------------
// oscil
//-------------------------------------
typedef struct {
  sample_t value;
  float freq, phase;
} oscil_t;
typedef oscil_t *oscil_p;
void oscil_init(oscil_t *O);
sample_t oscil_update(oscil_t *O);

//-------------------------------------
void oscil_init(oscil_t *O) {
  ZERO(O, oscil_t);
  O->freq = 440;
}

//-------------------------------------
oscil_t *oscil_new() {
  oscil_t *O = NEW(oscil_t);
  oscil_init(O);
  return O;
}

//-------------------------------------
sample_t oscil_update(oscil_t *O) {
  O->value.value[0] = O->value.value[1] = sin(O->phase * PI);
  O->phase += O->freq / audio.rate;
  while (O->phase > TAU)
    O->phase -= TAU;

  return O->value;
}

//-------------------------------------
// buffer
//-------------------------------------
typedef struct {
  float *data;
  uint len, size, rate;
  uint chans;
} buffer_t;
typedef buffer_t *buffer_p;

void buffer_init(buffer_t *B, uint len, uint chans);
void buffer_destroy(buffer_t *B);
void buffer_load(buffer_t *B, const char *filename);
float buffer_read1(buffer_t *B, int pos, uint chan);
sample_t buffer_read(buffer_t *B, int pos);
void buffer_write(buffer_t *B, int pos, sample_t in);
float buffer_rate_scale(buffer_t *B, float rate);

//-------------------------------------
void buffer_destroy(buffer_t *B) { FREE(B->data); }

//-------------------------------------
void buffer_init(buffer_t *B, uint len, uint chans) {
  ZERO(B, buffer_t);

  B->rate = audio.rate;
  B->len = len, B->chans = chans, B->size = B->len * B->chans;
  B->data = calloc(B->size, sizeof(float));
}

//-------------------------------------
buffer_t *buffer_new() {
  buffer_t *B = malloc(sizeof(buffer_t));
  ZERO(B, buffer_t);
  return B;
}

//-------------------------------------
void buffer_load(buffer_t *B, const char *filename) {
  FREE(B->data);
  ZERO(B, buffer_t);

  drwav wav;
  drwav_uint64 len;
  uint chans, rate;
  B->data = drwav_open_file_and_read_pcm_frames_f32(filename, &chans, &rate,
                                                    &len, NULL);

  B->rate = rate;
  B->chans = chans;
  B->len = len;
  B->size = B->len * B->chans;
}

//-------------------------------------
float buffer_read1(buffer_t *B, int pos, uint chan) {
  if (!B->data)
    return 0;

  pos = CLIP(pos, 0, B->len - 1);
  return B->data[pos * B->chans + chan];
}

//-------------------------------------
sample_t buffer_read(buffer_t *B, int pos) {
  if (!B->data)
    return sample_zero;

  pos = CLIP(pos, 0, B->len - 1);

  if (B->chans == 1)
    return make_sample1(B->data[pos]);
  else
    return make_sample(B->data[pos * B->chans + 0],
                       B->data[pos * B->chans + 1]);
}

//-------------------------------------
void buffer_write(buffer_t *B, int pos, sample_t in) {
  if (!B->data)
    return;

  pos = CLIP(pos, 0, B->len - 1);

  if (B->chans == 1)
    B->data[pos] = in.value[0] + in.value[1];
  else
    sample_loop { B->data[pos * B->chans + c] = in.value[c]; }
}

//-------------------------------------
float buffer_rate_scale(buffer_t *B, float rate) {
  return rate * (B->rate / audio.rate);
}

//-------------------------------------
// fft
//-------------------------------------
#define FFT_SIZE 1024
#define FFT_HALF_SIZE (FFT_SIZE / 2 + 1)

typedef struct {
  fftw_plan r2c[2], c2r[2];
  double *in[2];
  fftw_complex *out[2];
  int pos;
} fft_t;
typedef fft_t *fft_p;

#define fft_loop for (int f = 0; f < FFT_HALF_SIZE; ++f)

//-------------------------------------
void fft_init(fft_t *F) {
  ZERO(F, fft_t);

  sample_loop {
    F->in[c] = fftw_alloc_real(FFT_SIZE);
    F->out[c] = fftw_alloc_complex(FFT_HALF_SIZE);
    F->r2c[c] = fftw_plan_dft_r2c_1d(FFT_SIZE, F->in[c], F->out[c], 0);
    F->c2r[c] = fftw_plan_dft_c2r_1d(FFT_SIZE, F->out[c], F->in[c], 0);
  }
}

//-------------------------------------
fft_t *fft_new() {
  fft_t *F = NEW(fft_t);
  fft_init(F);
  return F;
}

//-------------------------------------
void fft_update(fft_t *F, sample_t in) {
  sample_loop {
    if (!F->in[c])
      continue;

    F->in[c][F->pos] = in.value[c];
  }

  F->pos++;
  if (F->pos >= FFT_SIZE)
    F->pos = 0;
}

//-------------------------------------
void fft_r2c(fft_t *F) {
  sample_loop {
    if (!F->r2c[c])
      continue;

    fftw_execute(F->r2c[c]);
  }
}

//-------------------------------------
void fft_c2r(fft_t *F) {
  sample_loop {
    if (!F->c2r[c])
      continue;

    fftw_execute(F->c2r[c]);
  }
}

//-------------------------------------
void fft_destroy(fft_t *F) {
  sample_loop {
    fftw_free(F->in[c]);
    fftw_free(F->out[c]);
    fftw_destroy_plan(F->r2c[c]);
    fftw_destroy_plan(F->c2r[c]);
  }
}

//-------------------------------------
carte_t fft_read(fft_t *F, int pos, uint chan) {
  return make_carte(F->out[chan][pos][0], F->out[chan][pos][1]);
}

//-------------------------------------
void fft_write(fft_t *F, int pos, uint chan, carte_t in) {
  F->out[chan][pos][0] = in.x, F->out[chan][pos][1] = in.y;
}

//-------------------------------------
void fft_play(fft_t *F) {
  fft_loop {
    int I = audio.pos - FFT_SIZE + f;
    while (I < 0)
      I += audio.frames;
    while (I >= audio.frames)
      I -= audio.frames;

    sample_loop audio.buf_out[c][I] += F->in[c][f] / FFT_SIZE;
  }
}

//-------------------------------------
// recorder
//-------------------------------------
typedef struct {
  buffer_t *buf;
  int pos;
} recorder_t;
typedef recorder_t *recorder_p;

//-------------------------------------
void recorder_init(recorder_t *R) { ZERO(R, recorder_t); }

//-------------------------------------
recorder_t *recorder_new() {
  recorder_t *R = NEW(recorder_t);
  recorder_init(R);
  return R;
}

//-------------------------------------
void recorder_update(recorder_t *R, sample_t in) {
  if (!R->buf)
    return;

  buffer_write(R->buf, R->pos, in);

  R->pos++;
  if (R->pos >= R->buf->len)
    R->pos = 0;
}

//-------------------------------------
// looper
//-------------------------------------
typedef struct {
  buffer_t buf;
  int write;
  float dur, read, speed;
  bool record;
  sample_t value;
} looper_t;
typedef looper_t *looper_p;

//-------------------------------------
void looper_init(looper_t *L, int len) {
  ZERO(L, looper_t);

  buffer_init(&L->buf, len, 2);

  L->dur = 1;
  L->speed = 1;
  L->record = true;
}

//-------------------------------------
looper_t *looper_new(int len) {
  looper_t *L = NEW(looper_t);
  looper_init(L, len);
  return L;
}

//-------------------------------------
void looper_destroy(looper_t *L) { buffer_destroy(&L->buf); }

//-------------------------------------
sample_t looper_update(looper_t *L, sample_t in) {
  if (L->record) {
    buffer_write(&L->buf, L->write, in);

    L->write++;
    if (L->write >= L->buf.len)
      L->write = 0;

    return in;
  } else {
    L->read += L->speed;

    if (L->read >= L->buf.len * L->dur)
      L->read = 0;
    else if (L->read < 0)
      L->read = (L->buf.len - 1) * L->dur;

    L->value = buffer_read(&L->buf, floorf(L->read));

    return L->value;
  }
}

//-------------------------------------
void looper_set(looper_t *L, bool record) {
  if (record == L->record)
    return;

  if (record)
    L->write = L->read;
  else
    L->read = L->write;

  L->record = record;
}

//-------------------------------------
// metro
//-------------------------------------
typedef struct {
  void (*on_trigger)(void *X);
  int value, dur;
  bool active, oneshot;
} metro_t;
typedef metro_t *metro_p;

//-------------------------------------
void metro_init(metro_t *M) {
  ZERO(M, metro_t);

  M->active = true;
  M->dur = audio.rate;
}

//-------------------------------------
metro_t *metro_new() {
  metro_t *M = malloc(sizeof(metro_t));
  metro_init(M);
  return M;
}

//-------------------------------------
bool metro_update(metro_t *M) {
  if (!M->active)
    return false;

  M->value++;

  if (M->value >= M->dur) {
    if (M->on_trigger)
      M->on_trigger(M);
    if (M->oneshot)
      M->active = false;
    M->value = 0;
    return true;
  } else
    return false;
}

//-------------------------------------
int metro_update_block(metro_t *M) {
  int c = 0;
  for (int i = 0; i < audio.frames; ++i)
    if (metro_update(M))
      c++;

  return c;
}

//-------------------------------------
// sampler
//-------------------------------------
typedef struct {
  sample_t value;
  buffer_t *buf;
  bool forward;
  bool loop, active;
  float start, end, rate, pos;
} sampler_t;
typedef sampler_t *sampler_p;

//-------------------------------------
void sampler_init(sampler_t *S) {
  ZERO(S, sampler_t);
  S->rate = 1, S->forward = 1;
  S->start = 0, S->end = 1;
  S->loop = true, S->active = false;
}

//-------------------------------------
sampler_t *sampler_new() {
  sampler_t *S = malloc(sizeof(sampler_t));
  sampler_init(S);
  return S;
}

//-------------------------------------
void sampler_range(sampler_t *S, float start, float end) {
  start = CLIP(start, 0, 1), end = CLIP(end, 0, 1);
  S->start = MIN(start, end), S->end = MAX(start, end);
}

//-------------------------------------
void sampler_dur(sampler_t *S, float start, float dur) {
  start = CLIP(start, 0, 1);
  float end = CLIP(start + dur, 0, 1);
  S->start = start, S->end = end;
}

//-------------------------------------
void sampler_set(sampler_t *S, buffer_t *B) { S->buf = B; }

//-------------------------------------
void sampler_trigger(sampler_t *S) {
  if (!S->buf)
    return;

  S->pos = (S->forward ? S->start : S->end) * (S->buf->len - 1);
  S->active = true;
}

//-------------------------------------
sample_t sampler_update(sampler_t *S) {
  if (!S->buf || !S->active)
    return sample_zero;

  int ipos = CLIP((int)floorf(S->pos), 0, S->buf->len - 1);
  S->value = buffer_read(S->buf, ipos);

  S->pos += (S->forward ? 1 : -1) * buffer_rate_scale(S->buf, S->rate);

  float start = S->start * (S->buf->len - 1);
  float end = S->end * (S->buf->len - 1);
  if (S->forward && S->pos > end) {
    if (S->loop)
      S->pos = start;
    else
      S->active = false;
  } else if (!S->forward && S->pos < start) {
    if (S->loop)
      S->pos = end;
    else
      S->active = false;
  }

  return S->value;
}

//-------------------------------------
// delay
//-------------------------------------
typedef struct {
  buffer_t buf;
  int pos;
  float del, mix;
  sample_t value;
} delay_t;
typedef delay_t *delay_p;
void delay_init(delay_t *D, int len);
delay_t *delay_new(int len);
void delay_destroy(delay_t *D);

//-------------------------------------
void delay_init(delay_t *D, int len) {
  ZERO(D, delay_t);
  buffer_init(&D->buf, len, 2);

  D->mix = 0.5;
}

//-------------------------------------
delay_t *delay_new(int len) {
  delay_t *D = NEW(delay_t);
  delay_init(D, len);
  return D;
}

//-------------------------------------
void delay_destroy(delay_t *D) { buffer_destroy(&D->buf); }

//-------------------------------------
sample_t delay_update(delay_t *D, sample_t in) {
  int read = ((int)floorf(D->pos - sec2samp(D->del)) + D->buf.len) % D->buf.len;
  D->value = buffer_read(&D->buf, read);

  in = sample_lincomb(in, D->mix, D->value, 1 - D->mix);
  buffer_write(&D->buf, D->pos, in);

  D->pos++;
  if (D->pos >= D->buf.len)
    D->pos = 0;

  return D->value;
}

//-------------------------------------
// delay
//-------------------------------------
#define COMB_MAX 0.1
typedef struct {
  buffer_t buf;
  sample_t value;
  int pos;
  float del;
} comb_t;
typedef comb_t *comb_p;

//-------------------------------------
void comb_init(comb_t *C) {
  ZERO(C, comb_t);
  buffer_init(&C->buf, sec2samp(COMB_MAX), 2);

  C->del = 0.01;
}

//-------------------------------------
comb_t *comb_new() {
  comb_t *C = NEW(comb_t);
  comb_init(C);
  return C;
}

//-------------------------------------
void comb_destroy(comb_t *C) { buffer_destroy(&C->buf); }

//-------------------------------------
sample_t comb_update(comb_t *C, sample_t in) {
  int read = ((int)floorf(C->pos - sec2samp(C->del * COMB_MAX)) + C->buf.len) %
             C->buf.len;
  C->value = sample_add(in, buffer_read(&C->buf, read));
  buffer_write(&C->buf, C->pos, in);

  C->pos++;
  if (C->pos >= C->buf.len)
    C->pos = 0;

  return C->value;
}

//-------------------------------------
// gran
//-------------------------------------
typedef struct {
  sample_t value;
  buffer_t *buf;
  int size, t;
  bool forward;
  float rate, pos, gpos, start, end;
} gran_t;
typedef gran_t *gran_p;

//-------------------------------------
void gran_init(gran_t *G) {
  ZERO(G, gran_t);

  G->rate = 1.0;
  G->pos = G->gpos = 0.0;
  G->forward = 1;
  G->size = 256;
  G->start = 0, G->end = 1.0;
}

//-------------------------------------
gran_t *gran_new() {
  gran_t *G = malloc(sizeof(gran_t));
  gran_init(G);
  return G;
}

//-------------------------------------
void gran_range(gran_t *G, float start, float end) {
  start = CLIP(start, 0, 1), end = CLIP(end, 0, 1);
  G->start = MIN(start, end), G->end = MAX(start, end);
}

//-------------------------------------
void gran_dur(gran_t *G, float start, float dur) {
  start = CLIP(start, 0, 1);
  float end = CLIP(start + dur, 0, 1);
  G->start = start, G->end = end;
}

//-------------------------------------
sample_t gran_update(gran_t *G) {
  if (!G->buf)
    return sample_zero;

  uint ipos = (uint)floorf(G->gpos);
  G->value = buffer_read(G->buf, ipos);

  G->pos += G->buf->rate / audio.rate;
  while (G->pos >= G->buf->len)
    G->pos -= G->buf->len;

  G->gpos += (G->forward ? 1 : -1) * buffer_rate_scale(G->buf, G->rate);

  float start = G->start * (G->buf->len - 1);
  float end = G->end * (G->buf->len - 1);
  if (G->forward && G->gpos > end) {
    G->gpos = start;
  } else if (!G->forward && G->gpos < start) {
    G->gpos = end;
  }

  G->t++;
  if (G->t >= G->size) {
    G->gpos = G->pos;
    G->t = 0;
  }

  return G->value;
}

//-------------------------------------
#define PITCH_MAX_DELAY 5120

typedef struct {
  struct {
    float data[PITCH_MAX_DELAY], read[2];
  } delay[2];
  sample_t value;
  float pitch;
  int write;
  bool active;
} pitch_t;
typedef pitch_t *pitch_p;

//-------------------------------------
void pitch_init(pitch_t *P) {
  ZERO(P, pitch_t);

  P->pitch = 0;
  sample_loop P->delay[c].read[1] = PITCH_MAX_DELAY / 2;
  P->active = true;
}

//-------------------------------------
pitch_t *pitch_new() {
  pitch_t *P = malloc(sizeof(pitch_t));
  pitch_init(P);
  return P;
}

//-------------------------------------
sample_t pitch_update(pitch_t *P, sample_t input) {
  if (!P->active)
    return input;

  float half_delay = PITCH_MAX_DELAY / 2;
  sample_loop {
    // set delay times
    // 1st
    P->delay[c].read[0] += P->pitch;

    while (P->delay[c].read[0] >= PITCH_MAX_DELAY)
      P->delay[c].read[0] -= PITCH_MAX_DELAY;
    while (P->delay[c].read[0] < 0)
      P->delay[c].read[0] += PITCH_MAX_DELAY;

    // 2nd
    P->delay[c].read[1] = P->delay[c].read[0] + half_delay;

    while (P->delay[c].read[1] >= PITCH_MAX_DELAY)
      P->delay[c].read[1] -= PITCH_MAX_DELAY;
    while (P->delay[c].read[1] < 0)
      P->delay[c].read[1] += PITCH_MAX_DELAY;

    // triangular envelope
    float env[2] = {0, 0};
    env[1] = ABS((P->delay[c].read[0] - half_delay) / half_delay);
    env[0] = 1 - env[1];

    // calculate delay lines
    // 1st
    int r = P->write + P->delay[c].read[0];
    while (r >= PITCH_MAX_DELAY)
      r -= PITCH_MAX_DELAY;

    P->value.value[c] = env[0] * P->delay[c].data[(int)r];

    // 2nd
    r = P->write + P->delay[c].read[1];
    while (r >= PITCH_MAX_DELAY)
      r -= PITCH_MAX_DELAY;

    P->value.value[c] += env[1] * P->delay[c].data[(int)r];

    // both
    P->delay[c].data[P->write] = input.value[c];
  }

  P->write++;
  if (P->write >= PITCH_MAX_DELAY)
    P->write = 0;

  return P->value;
}

//-------------------------------------
// filter
//-------------------------------------
typedef enum { LPF, HPF, BPF } filter_type_t;
typedef struct {
  struct {
    float y0, y1, y2;
    float x1, x2;
  } t[2];
  float b0, b1, b2;
  float a0, a1, a2;
  float alpha, w0, cos_w0;
  filter_type_t type;
  float freq, res, gain;
  sample_t value;
} filter_t;
typedef filter_t *filter_p;

void filter_init(filter_t *F);
filter_t *filter_new();
sample_t filter_update(filter_t *F, sample_t in);
void filter_res(filter_t *F, float res);
void filter_gain(filter_t *F, float gain);
void filter_set(filter_t *F, filter_type_t type, float freq);

//-------------------------------------
void filter_init(filter_t *F) {
  ZERO(F, filter_t);
  filter_res(F, 1);
  filter_set(F, LPF, 1000);
}

//-------------------------------------
filter_t *filter_new() {
  filter_t *F = NEW(filter_t);
  filter_init(F);
  return F;
}

//-------------------------------------
sample_t filter_update(filter_t *F, sample_t in) {
  sample_loop {
    typeof(F->t[c]) *t = &F->t[c];
    t->y2 = t->y1;
    t->y1 = t->y0;

    t->y0 = (F->b0 * in.value[c] + F->b1 * t->x1 + F->b2 * t->x2 -
             F->a1 * t->y1 - F->a2 * t->y2) /
            F->a0;

    t->x2 = t->x1;
    t->x1 = in.value[c];

    F->value.value[c] = t->y0;
  }

  return F->value;
}

//-------------------------------------
void filter_res(filter_t *F, float res) {
  F->res = MAX(res, 0.001);
  filter_set(F, F->type, F->freq);
}

//-------------------------------------
void filter_gain(filter_t *F, float gain) { float A = pow(10, F->gain / 40.0); }

//-------------------------------------
void filter_set(filter_t *F, filter_type_t type, float freq) {
  F->type = type;
  F->freq = MAX(freq, 10);

  F->w0 = TAU * F->freq / audio.rate;
  F->alpha = sin(F->w0) / (2.0 * F->res);
  F->cos_w0 = cos(F->w0);

  switch (F->type) {
  case LPF: {
    F->a0 = 1.0 + F->alpha;
    F->a1 = -2.0 * F->cos_w0;
    F->a2 = 1.0 - F->alpha;

    F->b0 = (1.0 - F->cos_w0) * 0.5;
    F->b1 = (1.0 - F->cos_w0);
    F->b2 = F->b0;
  } break;

  case HPF: {
    F->a0 = 1.0 + F->alpha;
    F->a1 = -2.0 * F->cos_w0;
    F->a2 = 1.0 - F->alpha;

    F->b0 = (1.0 + F->cos_w0) * 0.5;
    F->b1 = -(1.0 + F->cos_w0);
    F->b2 = F->b0;
  } break;

  case BPF: {
    F->b0 = F->res * F->alpha;
    F->b1 = 0;
    F->b2 = -F->res * F->alpha;
    F->a0 = 1 + F->alpha;
    F->a1 = -2 * F->cos_w0;
    F->a2 = 1 - F->alpha;
  } break;
  };
}

#endif
