#ifndef RENDER_H
#define RENDER_H

#include <SDL2/SDL.h>

void draw_text(SDL_Renderer *ren, float x, float y, const char *text,
               Uint8 r, Uint8 g, Uint8 b);
void draw_text_scaled(SDL_Renderer *ren, float x, float y, const char *text,
                      float scale, Uint8 r, Uint8 g, Uint8 b);
int  text_width(const char *text);

#endif
