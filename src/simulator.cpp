#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <MadDisplay/MadDisplay.hpp>
#include <showcase.hpp>
#include <cstdio>
#include <cstring>

int main(int argc, char** argv) {
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) { std::fprintf(stderr, "%s\n", SDL_GetError()); return 1; }
  SDL_Window* window = SDL_CreateWindow("MadDisplay OLED Simulator | Space: scene | Enter: toggle | P: pause",
      SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1024, 512, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
  if (!window) { std::fprintf(stderr, "%s\n", SDL_GetError()); SDL_Quit(); return 1; }
  SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!renderer) renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
  if (!renderer) { std::fprintf(stderr, "%s\n", SDL_GetError()); SDL_DestroyWindow(window); SDL_Quit(); return 1; }
  SDL_RenderSetLogicalSize(renderer, 128, 64);
  SDL_RaiseWindow(window);
  SDL_Texture* texture = SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,128,64);
  if (!texture) { SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit(); return 1; }
  uint32_t rgba[128*64]{};
  mad::BareBuffer<128, 64> canvas;
  mad_demo::Showcase showcase;
  bool done = false, paused = false;
  uint32_t animationUs = 0;
  uint64_t previous = SDL_GetPerformanceCounter();
  const bool smoke = argc > 1 && std::strcmp(argv[1], "--smoke") == 0;
  unsigned frames = 0;
  while (!done) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT) done = true;
      if (event.type == SDL_KEYDOWN && !event.key.repeat) {
        if (event.key.keysym.sym == SDLK_ESCAPE) done = true;
        if (event.key.keysym.sym == SDLK_SPACE || event.key.keysym.sym == SDLK_RIGHT) showcase.next();
        if (event.key.keysym.sym == SDLK_RETURN) showcase.toggle();
        if (event.key.keysym.sym == SDLK_p) paused = !paused;
      }
    }
    const uint64_t now = SDL_GetPerformanceCounter();
    if (!paused) animationUs += smoke ? 166667u : static_cast<uint32_t>((now - previous) * 1000000 / SDL_GetPerformanceFrequency());
    previous = now;
    showcase.render(canvas, animationUs);
    for (int y = 0; y < 64; ++y) for (int x = 0; x < 128; ++x)
      rgba[y*128+x] = canvas.isOn(x,y) ? 0xffd2f5ffu : 0xff070c11u;
    SDL_UpdateTexture(texture,nullptr,rgba,128*sizeof(uint32_t));
    SDL_RenderCopy(renderer,texture,nullptr,nullptr);
    SDL_RenderPresent(renderer);
    if (smoke && ++frames >= 120) done = true;
  }
  SDL_DestroyTexture(texture); SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit();
  return 0;
}
