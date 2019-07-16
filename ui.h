/******************************************************************************
  File: ui.h
  Created: 2019-06-27
  Updated: 2019-07-16
  Author: Aaron Oman
  Notice: Creative Commons Attribution 4.0 International License (CC-BY 4.0)
 ******************************************************************************/
#ifndef UI_VERSION
#define UI_VERISON "0.1.0"

#include "SDL2/SDL.h"

struct system;
struct opcode;
struct ui;

struct ui_debug {
        int enabled;
        int resume;
        int waiting;
};

struct ui_debug
UIDebugInfo(struct ui *ui);

void
UIMemControl(void *(*allocator)(size_t), void (*deallocator)(void *));

struct ui *
UIInit(int shouldBeEnabled, unsigned int widgetWidth, unsigned int widgetHeight, SDL_Window *window);

void
UIInputBegin(struct ui *u);

void
UIInputEnd(struct ui *u);

void
UIHandleEvent(struct ui *u, SDL_Event *event);

void
UIWidgets(struct ui *u, struct system *s, struct opcode *c);

void
UIRender(struct ui *u);

void
UIDeinit(struct ui *u);

int
UIDebugIsEnabled(struct ui *u);

void
UIDebugSetEnabled(struct ui *u, int);

int
UIDebugIsWaiting(struct ui *u);

void
UIDebugSetWaiting(struct ui*u, int);

int
UIDebugShouldResume(struct ui *u);

void
UIDebugSetResume(struct ui*u, int);

#endif // UI_VERSION
