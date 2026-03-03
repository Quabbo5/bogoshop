#ifndef LICENSE_H
#define LICENSE_H

#include <SDL2/SDL.h>

int  validate_key(const char *key);
int  load_license(void);
void save_license(const char *key);
int  show_license_screen(SDL_Window *win, SDL_Renderer *ren);

#endif
