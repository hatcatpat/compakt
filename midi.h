#ifndef MIDI
#define MIDI

#include "gui.h"
#include "utils.h"

#include <portmidi.h>
#include <pthread.h>

//-------------------------------------
#define MIDI_NUM_TRACKS 4

struct {
  pthread_t thread_id;
  PmStream *in;
  uint track;
  float ctrl[MIDI_NUM_TRACKS][256];
  bool quit, valid;
} midi;
int midi_init(int id);
void midi_cleanup();
void *midi_loop();

#define MIDI_BUFFER_SIZE 8
#define MIDI_RATE 16

//-------------------------------------
extern void midi_callback(uint track, uint ctrl, float value);

//-------------------------------------
void *midi_loop() {
  PmEvent buffer[MIDI_BUFFER_SIZE];

  while (!midi.quit) {
    while (Pm_Poll(midi.in)) {
      int n = Pm_Read(midi.in, buffer, MIDI_BUFFER_SIZE);
      n = MIN(n, MIDI_BUFFER_SIZE);
      for (int i = 0; i < n; ++i) {
        int ctrl = Pm_MessageData1(buffer[i].message);
        float value = (float)Pm_MessageData2(buffer[i].message) / 127;

        if (ctrl == 62 || ctrl == 61) {
          if (value > 0.5 && midi.ctrl[midi.track][62] < 0.5) {
            if (ctrl == 62) {
              midi.track++;
              if (midi.track >= MIDI_NUM_TRACKS)
                midi.track = 0;
            } else {
              if (midi.track == 0)
                midi.track = MIDI_NUM_TRACKS - 1;
              else
                midi.track--;
            }
          }
        } else {
          midi_callback(midi.track, ctrl, value);
          midi.ctrl[midi.track][ctrl] = value;
        }

#ifdef MIDI_DEBUG
        printf("[midi] ctrl %i, value %f, track %i\n", ctrl, value, midi.track);
#endif
      }
    }

    delay(MIDI_RATE);
  }

  return NULL;
}

//-------------------------------------
int midi_init(int id) {
  memset(&midi, 0, sizeof(midi));

  PmError error = Pm_Initialize();
  if (error) {
    printf("[midi error] unable to initialize portmidi: %s\n",
           Pm_GetErrorText(error));
    return -1;
  }

  if (id == -1) {
    for (int i = 0; i < Pm_CountDevices(); ++i) {
      const PmDeviceInfo *info = Pm_GetDeviceInfo(i);
      printf("device %i %s %s\n", i, info->interf, info->name);
    }

    return 0;
  }

  const PmDeviceInfo *info = Pm_GetDeviceInfo(id);
  if (!info) {
    printf("[midi error] unable to get device info\n");
    return -1;
  }

  printf("[midi] opening midi input device %i %s %s\n", id, info->interf,
         info->name);
  error = Pm_OpenInput(&midi.in, id, NULL, 0, NULL, NULL);
  if (error) {
    printf("[midi error] unable to open midi input: %s\n",
           Pm_GetErrorText(error));
    return -1;
  }

  Pm_SetFilter(midi.in, PM_FILT_ACTIVE | PM_FILT_CLOCK | PM_FILT_SYSEX);

  PmEvent buffer[1];
  while (Pm_Poll(midi.in))
    Pm_Read(midi.in, buffer, 1);

  midi.quit = false;
  midi.valid = true;
  midi.track = 0;
  memset(midi.ctrl, 0, 256 * MIDI_NUM_TRACKS * sizeof(float));

  pthread_create(&midi.thread_id, NULL, midi_loop, NULL);

  return 0;
}

//-------------------------------------
void midi_cleanup() {
  if (midi.valid) {
    midi.quit = true;
    pthread_join(midi.thread_id, NULL);

    if (midi.in)
      Pm_Close(&midi.in);
    Pm_Terminate();
  }
}

//-------------------------------------
void midi_draw() {
  static uint track = 0;

  if (track == midi.track)
    return;

  track = midi.track;

  color_background();
  rect_t r = {
      (WIDTH - 1 - 0.5) * GRID_SIZE,
      (HEIGHT - 1 - 0.5) * GRID_SIZE,
      GRID_SIZE,
      GRID_SIZE,
  };
  draw_rect(&r);

  color_accent();
  draw_int(midi.track + 1, WIDTH - 1, HEIGHT - 1);
  color_background();
}

#endif
