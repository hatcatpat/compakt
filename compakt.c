//#define MIDI_DEBUG
#include "compakt.h"

#include <dirent.h>

//-------------------------------------
// fft
//-------------------------------------
float window(int i) { return 1 - SQR(1 - 2 * (float)i / (FFT_SIZE - 1)); }
float wrap(float x) {
  if (x >= 0)
    return fmodf(x + PI, TAU) - PI;
  else
    return fmodf(x - PI, -TAU) + PI;
}

#define FFT_HOP_SIZE 256
#define FFT_BUF_SIZE (2 * FFT_SIZE)
struct {
  struct {
    float phase[FFT_SIZE], mag[FFT_SIZE], freq[FFT_SIZE];
  } analysis[2];
  struct {
    float phase[FFT_SIZE], mag[FFT_SIZE], freq[FFT_SIZE];
  } synthesis[2];
  float buf[FFT_BUF_SIZE * 2], out[FFT_BUF_SIZE * 2];
  float radii[FFT_HALF_SIZE * 2];
  float shift;
  fft_p fft;
  array_p arr;
  int rec, read, write, hopcounter;
  int hop_size;
} fft;

#define FFT_SHIFT_RANGE 4
void fft_pitchshift_changed(void *X, float value) {
  fft.shift = powf(2.0, FFT_SHIFT_RANGE * norm2bi(value));
}

void fft_pitchshift() {
  loop(f, FFT_SIZE) {
    int I = fft.rec - FFT_SIZE + f;
    while (I < 0)
      I += FFT_BUF_SIZE;
    while (I >= FFT_BUF_SIZE)
      I -= FFT_BUF_SIZE;

    float w = window(f);
    sample_loop fft.fft->in[c][f] = fft.buf[I * 2 + c] * w;
  }

  fft_r2c(fft.fft);

  // analysis
  fft_loop {
    float p = (float)f / FFT_SIZE;
    sample_loop {
      carte_t C = fft_read(fft.fft, f, c);
      polar_t P = carte2polar(C);
      float phase = P.angle;
      float mag = P.radius;

      static float m = 0.9;
      fft.radii[f * 2 + c] =
          fft.radii[f * 2 + c] * m + (P.radius / 16) * (1 - m);

      float diff = phase - fft.analysis[c].phase[f];
      float freq = TAU * p;
      diff = wrap(diff - freq * FFT_HOP_SIZE);
      float dev = diff * (float)FFT_SIZE / (float)FFT_HOP_SIZE / TAU;

      fft.analysis[c].freq[f] = (float)f + dev;
      fft.analysis[c].mag[f] = mag;
      fft.analysis[c].phase[f] = phase;

      fft.synthesis[c].freq[f] = fft.synthesis[c].mag[f] = 0;
    }
  }

  fft_loop {
    sample_loop {
      int bin = floorf(f * fft.shift + 0.5);
      if (bin <= FFT_HALF_SIZE) {
        fft.synthesis[c].mag[bin] += fft.analysis[c].mag[f];
        fft.synthesis[c].freq[bin] = fft.analysis[c].freq[f] * fft.shift;
      }
    }
  }

  // synthesis
  fft_loop {
    float p = (float)f / FFT_SIZE;
    sample_loop {
      float dev = fft.synthesis[c].freq[f] - f;
      float diff = dev * TAU * (float)FFT_HOP_SIZE / (float)FFT_SIZE;
      float freq = TAU * p;
      diff += freq * FFT_HOP_SIZE;
      float angle = wrap(fft.synthesis[c].phase[f] + diff);
      fft.synthesis[c].phase[f] = angle;

      polar_t P = make_polar(fft.synthesis[c].mag[f], angle);
      carte_t C = polar2carte(P);
      fft_write(fft.fft, f, c, C);
    }
  }

  fft_c2r(fft.fft);

  loop(f, FFT_SIZE) {
    int I = fft.write - FFT_SIZE + f;
    while (I < 0)
      I += FFT_BUF_SIZE;
    while (I >= FFT_BUF_SIZE)
      I -= FFT_BUF_SIZE;

    float w = window(f);
    sample_loop fft.out[I * 2 + c] += fft.fft->in[c][f] * w / FFT_SIZE;
  }

  fft.write += fft.hop_size;
  if (fft.write >= FFT_BUF_SIZE)
    fft.write -= FFT_BUF_SIZE;
}

//-------------------------------------
enum { K, A, SN, P, T, SH, CL, NUM_BUF };
buffer_p buf[NUM_BUF];

sampler_p smp;
slider_p smp_spd;
void spd_changed(void *X, float value) { smp->rate = value * 2; }

slider_p volume_sl;

slider_p pitch_sl;

struct {
  metro_p met;
  slider_p sl;
} met;
void met_changed(void *X, float value) { met.met->dur = sec2samp(value * 0.5); }

struct {
  comb_p comb;
  slider_p del;
} comb;
void comb_del_changed(void *X, float value) { comb.comb->del = value; }

struct {
  delay_p del;
  slider_p del_sl, mix_sl;
  button_p active;
} del;
void del_del_changed(void *X, float value) { del.del->del = value * 2; }
void del_mix_changed(void *X, float value) { del.del->mix = value; }

struct {
  filter_p filter;
  slider_p res, freq, type;
} filter;
void filter_freq_changed(void *X, float value) {
  filter_set(filter.filter, filter.filter->type, value * 10000);
}
void filter_res_changed(void *X, float value) {
  filter_res(filter.filter, value * 50);
}
void filter_type_changed(void *X, float value) {
  filter_set(filter.filter, floorf(value * 2), filter.filter->freq);
}

struct {
  button_p active;
  slider_p bit;
} crush;

struct {
  button_p active;
  slider_p dur, speed;
  looper_p looper;
} looper;
void looper_toggle(bool value) { looper_set(looper.looper, value); }
void looper_dur(void *X, float value) { looper.looper->dur = value; }
void looper_speed(void *X, float value) {
  looper.looper->speed = norm2bi(value) * 4;
}

//-------------------------------------
void audio_callback() {
  looper.looper->dur = looper.dur->value;

  audio_loop {
    sample_t s = sample_zero;

    if (metro_update(met.met)) {
      smp->buf = buf[irand(0, NUM_BUF)];
      sampler_trigger(smp);
    }

    if (smp->active)
      s = sampler_update(smp);

    //
    // fft
    //
    sample_loop fft.buf[fft.rec * 2 + c] = s.value[c];
    fft.rec++;
    if (fft.rec >= FFT_BUF_SIZE)
      fft.rec = 0;

    s = make_sample(fft.out[fft.read * 2 + 0], fft.out[fft.read * 2 + 1]);
    s = comb_update(comb.comb, s);
    s = filter_update(filter.filter, s);
    audio_out(s);

    s = delay_update(del.del, s);
    if (del.active->value)
      audio_out(s);

    sample_loop fft.out[fft.read * 2 + c] = 0;

    fft.read++;
    if (fft.read >= FFT_BUF_SIZE)
      fft.read = 0;

    fft.hopcounter++;
    if (fft.hopcounter >= fft.hop_size) {
      fft.hopcounter = 0;
      fft_pitchshift();
    }

    // post
    s = audio_get();
    s = sample_mul_s(s, volume_sl->value);
    if (crush.active->value)
      s = effect_bit(s, scale_norm(crush.bit->value, 1, 16));
    s = looper_update(looper.looper, s);
    audio_set(s);
  }
}

//-------------------------------------
void gui_callback() { midi_draw(); }

//-------------------------------------
void midi_callback(uint track, uint ctrl, float value) {
  switch (track) {
    //
  case 0: {
    switch (ctrl) {
    case 16:
      slider_set(pitch_sl, value);
      break;
    case 17:
      slider_set(filter.freq, value);
      break;
    case 18:
      slider_set(filter.res, value);
      break;
    case 19:
      slider_set(filter.type, value);
      break;
    case 20:
      slider_set(comb.del, value);
      break;
    case 21:
      slider_set(del.del_sl, value);
      break;
    case 22:
      slider_set(del.mix_sl, value);
      break;
    case 23:
      slider_set(met.sl, value);
      break;
    case 7:
      slider_set(volume_sl, value);
      break;
    case 0:
      slider_set(crush.bit, value);
      break;
    case 2:
      slider_set(smp_spd, value);
      break;
    case 5:
      slider_set(looper.dur, value);
      break;
    case 6:
      slider_set(looper.speed, value);
      break;

      //
    case 32:
      if (value > 0.5 && midi.ctrl[track][ctrl] < 0.5)
        button_set(crush.active, !crush.active->value);
      break;
    case 37:
      if (value > 0.5 && midi.ctrl[track][ctrl] < 0.5)
        button_set(del.active, !del.active->value);
      break;
    case 69:
      if (value > 0.5 && midi.ctrl[track][ctrl] < 0.5) {
        looper_set(looper.looper, looper.active->value);
        button_set(looper.active, !looper.active->value);
      }
      break;
    }
  } break;
    //
  }
}

//-------------------------------------
int main(void) {
  init();

  midi_init(3);

  //-------------------------------------
  // setup
  {
    //
    fft.fft = fft_new();
    fft.arr = array_new(1, 1, WIDTH - 2, 6);
    array_set(fft.arr, FFT_HALF_SIZE, 2, fft.radii, 0, 1);
    fft.arr->dynamic = true;
    fft.hop_size = FFT_HOP_SIZE;
    fft.rec = fft.read = fft.hopcounter = 0;
    fft.write = FFT_SIZE + FFT_HOP_SIZE;

    //
    pitch_sl = slider_new(1, 8, 1, 4);
    pitch_sl->on_change = fft_pitchshift_changed;
    slider_set(pitch_sl, 0.5);
    slider_set_vert(pitch_sl, true);
    widget_name(pitch_sl, "pit");

    //
    volume_sl = slider_new(WIDTH - 2, 8, 1, HEIGHT - 9);
    slider_set_vert(volume_sl, true);
    widget_name(volume_sl, "vol");
    slider_set(volume_sl, 0);

    //
    filter.filter = filter_new(), filter_set(filter.filter, LPF, 1000);
    filter_res(filter.filter, 1);

    filter.freq = slider_new(3, 8, 1, 4);
    filter.freq->on_change = filter_freq_changed;
    slider_set_vert(filter.freq, true);
    widget_name(filter.freq, "frq");
    slider_set(filter.freq, 1);

    filter.res = slider_new(5, 8, 1, 4);
    filter.res->on_change = filter_res_changed;
    slider_set_vert(filter.res, true);
    widget_name(filter.res, "res");
    slider_set(filter.res, 0.1);

    filter.type = slider_new(7, 8, 1, 4);
    filter.type->on_change = filter_type_changed;
    slider_set_vert(filter.type, true);
    widget_name(filter.type, "t");

    //
    comb.comb = comb_new();

    comb.del = slider_new(9, 8, 1, 4);
    slider_set_vert(comb.del, true);
    comb.del->on_change = comb_del_changed;
    widget_name(comb.del, "com");
    slider_set(comb.del, 0.1);

    //
    del.del = delay_new(sec2samp(2));

    del.active = button_new(11, 13, 1, 1);
    del.active->toggle = true;
    widget_name(del.active, "del");

    del.del_sl = slider_new(11, 8, 1, 4);
    slider_set_vert(del.del_sl, true);
    del.del_sl->on_change = del_del_changed;
    widget_name(del.del_sl, "del");
    slider_set(del.del_sl, 1);

    del.mix_sl = slider_new(13, 8, 1, 4);
    slider_set_vert(del.mix_sl, true);
    del.mix_sl->on_change = del_mix_changed;
    widget_name(del.mix_sl, "mix");
    slider_set(del.mix_sl, 0.5);

    //
    {
      char *files[NUM_BUF] = {
          "samples/k/0.wav",  "samples/a/0.wav", "samples/sn/0.wav",
          "samples/p/0.wav",  "samples/t/0.wav", "samples/sh/0.wav",
          "samples/cl/0.wav",
      };
      loop(b, NUM_BUF) { buf[b] = buffer_new(), buffer_load(buf[b], files[b]); }
    }

    //
    met.met = metro_new(), met.met->dur = sec2samp(0.1);
    met.sl = slider_new(15, 8, 1, 4);
    met.sl->on_change = met_changed;
    slider_set_vert(met.sl, true);
    widget_name(met.sl, "met");
    slider_set(met.sl, 1);

    //
    crush.bit = slider_new(1, 13, 1, 4);
    slider_set_vert(crush.bit, true);
    slider_set(crush.bit, 1);
    widget_name(crush.bit, "bit");

    crush.active = button_new(3, 13, 1, 1);
    crush.active->toggle = true;
    widget_name(crush.active, "csh");

    //
    smp = sampler_new(), smp->buf = buf[K];
    sampler_trigger(smp);

    smp_spd = slider_new(7, 13, 1, 4);
    smp_spd->on_change = spd_changed;
    slider_set_vert(smp_spd, true);
    slider_set(smp_spd, 0.5);
    widget_name(smp_spd, "spd");

    //
    looper.looper = looper_new(sec2samp(1));

    looper.dur = slider_new(13, 13, 1, 4);
    looper.dur->on_change = looper_dur;
    slider_set_vert(looper.dur, true);
    widget_name(looper.dur, "dur");
    slider_set(looper.dur, 1);

    looper.speed = slider_new(15, 13, 1, 4);
    looper.speed->on_change = looper_speed;
    slider_set_vert(looper.speed, true);
    widget_name(looper.speed, "spd");
    slider_set(looper.speed, 0.75);

    looper.active = button_new(11, 15, 1, 1);
    widget_name(looper.active, "loo");
    looper.active->toggle = true;
    looper.active->on_click = looper_toggle;
  }

  //-------------------------------------
  start();

  //-------------------------------------
  midi_cleanup();
  cleanup();

  //-------------------------------------
  // destroy
  {
    looper_destroy(looper.looper);
    delay_destroy(del.del), free(del.del);
    comb_destroy(comb.comb), free(comb.comb);
    free(filter.filter);
    free(smp);
    free(met.met);
    fft_destroy(fft.fft), free(fft.fft);
    loop(b, NUM_BUF) buffer_destroy(buf[b]), free(buf[b]);
  }

  return 0;
}
