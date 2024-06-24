#pragma once

#include <SDL2/SDL.h>
#include <stdint.h>

class SDLWindow 
{
    private:
        SDL_Window *window;
        SDL_Renderer *renderer;
        SDL_Surface *surface;
        uint32_t *pixels;
        int width;
        int height;

    public:
        SDLWindow(const char* title, int w, int h) : width(w), height(h) 
        {
            if (SDL_Init(SDL_INIT_VIDEO) < 0) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s", SDL_GetError());
                exit(3);
            }

            window = SDL_CreateWindow(title, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, w, h, SDL_WINDOW_RESIZABLE);
            if (!window) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create window: %s", SDL_GetError());
                exit(3);
            }

            renderer = SDL_CreateRenderer(window, -1, 0);
            if (!renderer) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create renderer: %s", SDL_GetError());
                exit(3);
            }

            surface = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_RGBA32);
            if (!surface) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create surface: %s", SDL_GetError());
                exit(3);
            }

            pixels = (uint32_t *)surface->pixels;
        }

        ~SDLWindow() 
        {
            SDL_FreeSurface(surface);
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            SDL_Quit();
        }

        void update(uint32_t* newPixels) 
        {
            // Copy new pixel data
            memcpy(pixels, newPixels, width * height * sizeof(uint32_t));

            // Create a texture from the updated surface
            SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
            if (!texture) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create texture: %s", SDL_GetError());
                exit(3);
            }

            // Render the texture to the renderer
            SDL_RenderCopy(renderer, texture, NULL, NULL);
            SDL_RenderPresent(renderer);

            // Clean up texture
            SDL_DestroyTexture(texture);
        }
};