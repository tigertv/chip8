/******************************************************************************
  File: system.c
  Created: (No later than 2019-07-07)
  Updated: 2019-07-14
  Author: Aaron Oman
  Notice: Creative Commons Attribution 4.0 International License (CC-BY 4.0)
 ******************************************************************************/
#include <string.h> // memset
#include <stdlib.h> // malloc, free
#include <stdio.h>
#include <pthread.h>

#include "system.h"

#define GRAPHICS_WIDTH 64
#define GRAPHICS_HEIGHT 32
#define MEMORY_SIZE 4096
#define NUM_REGISTERS 16
#define GRAPHICS_MEM_SIZE GRAPHICS_WIDTH*GRAPHICS_HEIGHT
#define STACK_SIZE 16
#define NUM_KEYS 16
#define FONT_SIZE 80

struct system_wfk { // wait for key
        unsigned char key; // 0 - 16
        int waiting; // bool
        int justChanged; // bool
};

struct system_private {
        struct system_wfk wfk;
        pthread_rwlock_t timerRwLock;
        pthread_rwlock_t soundRwLock;
        // There are two timer registers that count at 60 Hz. When set above
        // zero they will count down to zero.
        unsigned char delayTimer;
        unsigned char soundTimer;

        int soundTimerTriggered;
};

typedef void *(*allocator)(size_t);
typedef void (*deallocator)(void *);

static unsigned char MEMORY[MEMORY_SIZE];
static unsigned char GFX[GRAPHICS_MEM_SIZE];
static allocator ALLOCATOR = malloc;
static deallocator DEALLOCATOR = free;

static unsigned char fontset[FONT_SIZE] = {
        0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
        0x20, 0x60, 0x20, 0x20, 0x70, // 1
        0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
        0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
        0x90, 0x90, 0xF0, 0x10, 0x10, // 4
        0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
        0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
        0xF0, 0x10, 0x20, 0x40, 0x40, // 7
        0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
        0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
        0xF0, 0x90, 0xF0, 0x90, 0x90, // A
        0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
        0xF0, 0x80, 0x80, 0x80, 0xF0, // C
        0xE0, 0x90, 0x90, 0x90, 0xE0, // D
        0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
        0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

void SystemMemControl(allocator Alloc, deallocator Dealloc) {
        ALLOCATOR = Alloc;
        DEALLOCATOR = Dealloc;
}

struct system *SystemInit() {
        struct system_private *prv = (struct system_private *)ALLOCATOR(sizeof(struct system_private));
        memset(prv, 0, sizeof(struct system_private));

        struct system *s = (struct system *)ALLOCATOR(sizeof(struct system));
        memset(s, 0, sizeof(struct system));

        s->memory = MEMORY;
        memset(MEMORY, 0, MEMORY_SIZE);

        s->gfx = GFX;
        memset(GFX, 0, GRAPHICS_MEM_SIZE);

        s->pc = 0x200;

        s->fontp = 0;
        for (int i=s->fontp; i<FONT_SIZE; i++) {
                s->memory[i] = fontset[i];
        }

        s->displayWidth = 64;
        s->displayHeight = 32;

        s->prv = prv;
        pthread_rwlockattr_t attr;
        pthread_rwlockattr_init(&attr);
        pthread_rwlockattr_setpshared(&attr, 1);

        if (0 != pthread_rwlock_init(&s->prv->timerRwLock, &attr)) {
                fprintf(stderr, "Couldn't initialize system timer rwlock");
                return NULL;
        }

        if (0 != pthread_rwlock_init(&s->prv->soundRwLock, &attr)) {
                fprintf(stderr, "Couldn't initialize system sound rwlock");
                return NULL;
        }

        return s;
}

void SystemFree(struct system *s) {
        DEALLOCATOR(s);
}

// Each opcode is a two-byte instruction, so we have to double increment each time.
void SystemIncrementPC(struct system *s) {
        s->pc += 2;
}

unsigned short SystemFontSprite(struct system *s, unsigned int index) {
        return s->fontp + (index * 5);
}

int SystemLoadProgram(struct system *s, unsigned char *m, unsigned int size) {
        unsigned char *mem = &s->memory[0x200];
        unsigned short max_size = MEMORY_SIZE - 0x200;

        if (size > max_size) {
                return 0;
        }

        for (int i=0; i<size; i++) {
                mem[i] = m[i];
        }

        return !0;
}

void SystemPushStack(struct system *s) {
        if (s->sp > 0xF) {
                return;
                // TODO: ERROR?!?
        }

        s->stack[s->sp] = s->pc;
        s->sp++;
}

void SystemPopStack(struct system *s) {
        if (s->sp < 1) {
                return;
                // TODO: ERROR?!?
        }

        s->stack[s->sp] = 0; // Debug - set to zero after "free."
        s->sp--;
        s->pc = s->stack[s->sp];
}

void SystemClearScreen(struct system *s) {
        memset(s->gfx, 0, GRAPHICS_MEM_SIZE);
}

// Display: Draws a sprite at coordinate (VX, VY) that has a width of 8 pixels
// and a height of N pixels. Each row of 8 pixels is read as bit-coded starting
// from memory location I; I value doesn’t change after the execution of this
// instruction. As described above, VF is set to 1 if any screen pixels are
// flipped from set to unset when the sprite is drawn, and to 0 if that doesn’t
// happen.
// I'm assuming (VX, VY) is the lower-left corner of the sprint, not the center.
void SystemDrawSprite(struct system *s, unsigned int x_pos, unsigned int y_pos, unsigned int height) {
        s->v[15] = 0;

        for (int y = 0; y < height; y++) {
                // I contains a 1-byte bitmap representing a line of the sprite.
                // [XXXX XXXX]
                unsigned char pixel = s->memory[s->i + y];

                for (int x = 0; x < 8; x++) {
                        if ((pixel & (0x80 >> x)) == 0) { // This line taken from www.multigesture.net/articles/how-to-write-an-emulator-chip-8-interpreter/
                                continue;
                        }

                        int y_off = (y_pos + y) * GRAPHICS_WIDTH;
                        int x_off = (x_pos + x);
                        int pos = y_off + x_off;

                        if (s->gfx[pos] == 0xFF) {
                                s->v[15] = 1;
                        }

                        s->gfx[pos] ^= 0xFF;
                }
        }
}

void SystemWFKSet(struct system *s, unsigned char key) {
        s->prv->wfk.waiting = 1;
        s->prv->wfk.justChanged = 0;
        s->prv->wfk.key = key;
}

int SystemWFKWaiting(struct system *s) {
        return s->prv->wfk.waiting;
}

void SystemWFKOccurred(struct system *s, unsigned char key) {
        s->prv->wfk.waiting = 0;
        s->prv->wfk.justChanged = 1;
        s->v[s->prv->wfk.key] = key;
}

int SystemWFKChanged(struct system *s) {
        return s->prv->wfk.justChanged;
}

void SystemWFKStop(struct system *s) {
        s->prv->wfk.justChanged = 0;
}


void SystemDecrementTimers(struct system *s) {
        if (0 == pthread_rwlock_wrlock(&s->prv->timerRwLock)) {
                if (s->prv->delayTimer > 0) {
                        s->prv->delayTimer--;
                }

                if (s->prv->soundTimer > 0) {
                        s->prv->soundTimer--;
                }

                pthread_rwlock_unlock(&s->prv->timerRwLock);
        } else {
                fprintf(stderr, "Failed to lock system timer rw lock");
        }
}

int SystemDelayTimer(struct system *s) {
        int result;
        if (0 == pthread_rwlock_rdlock(&s->prv->timerRwLock)) {
                result = s->prv->delayTimer;
                pthread_rwlock_unlock(&s->prv->timerRwLock);
        } else {
                fprintf(stderr, "Failed to lock system timer rw lock");
        }

        return result;
}

int SystemSoundTimer(struct system *s) {
        int result;
        if (0 == pthread_rwlock_rdlock(&s->prv->timerRwLock)) {
                result = s->prv->soundTimer;
                pthread_rwlock_unlock(&s->prv->timerRwLock);
        } else {
                fprintf(stderr, "Failed to lock system timer rw lock");
        }

        return result;
}

void SystemSetTimers(struct system *s, int dt, int st) {
        if (0 == pthread_rwlock_wrlock(&s->prv->timerRwLock)) {
                if (dt != -1) {
                        s->prv->delayTimer = dt;
                }
                if (st != -1) {
                        s->prv->soundTimer = st;
                }
                pthread_rwlock_unlock(&s->prv->timerRwLock);
        } else {
                fprintf(stderr, "Failed to lock system timer rw lock");
        }
}

int SystemSoundTriggered(struct system *s) {
        int result;
        if (0 == pthread_rwlock_rdlock(&s->prv->soundRwLock)) {
                result = (s->prv->soundTimerTriggered && s->prv->soundTimer == 0);
                pthread_rwlock_unlock(&s->prv->soundRwLock);
        } else {
                fprintf(stderr, "Failed to lock system timer rw lock");
        }

        return result;
}

void SystemSoundSetTrigger(struct system *s, int v) {
        if (0 == pthread_rwlock_wrlock(&s->prv->soundRwLock)) {
                s->prv->soundTimerTriggered = v;
                pthread_rwlock_unlock(&s->prv->soundRwLock);
        } else {
                fprintf(stderr, "Failed to lock system timer rw lock");
        }
}
