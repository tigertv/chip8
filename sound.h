/******************************************************************************
  File: sound.h
  Created: (No later than 2019-07-07)
  Updated: 2019-07-14
  Author: Aaron Oman
  Notice: Creative Commons Attribution 4.0 International License (CC-BY 4.0)
 ******************************************************************************/
#ifndef SOUND_VERSION
#define SOUND_VERSION "0.1.0"

struct sound;

void
SoundMemControl(void *(*allocator)(size_t), void (*deallocator)(void *));

void
SoundShutdown(struct sound *sound);

struct sound *
SoundInit();

void
SoundPlay(struct sound *);

void
SoundStop(struct sound *);

int
SoundShouldPlay(struct sound *);


#endif // SOUND_VERSION