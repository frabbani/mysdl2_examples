#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <algorithm>

#include <mysdl2/mysdl2.h>

#include "water.h"

using namespace sdl2;

#define DISP_W 512
#define DISP_H 512

SDL sdl;

enum DrawType {
  Height,
  Divergence,
  Gradient
};

std::shared_ptr<Water> water = nullptr;
Field<float> drop;
DrawType drawType { Height };

Pixel24 toPixel(float value) {
  value = std::clamp(value, 0.0f, 1.0f);
  value = 127.0f + 128.0f * value;
  return Pixel24(value, value, value);
}

void init() {
  water = std::make_shared<Water>(DISP_W, DISP_H, 1.0f);
  Bitmap bitmap("drop.bmp");
  printf("water droplet size: %d x %d\n", bitmap.width(), bitmap.height());
  bitmap.lock();
  if (bitmap.pixels.data) {
    auto &pixels = bitmap.pixels;
    drop.init(pixels.w, pixels.h);
    for (int y = 0; y < drop.h; y++)
      for (int x = 0; x < drop.w; x++)
        drop(x, y) = float(pixels.get24(x, y)->r) / 255.0f;
  }
}

void term() {
  Bitmap bitmap(water->w, water->h, 24, "save.bmp");
  bitmap.lock();
  if (bitmap.pixels.data) {
    for (int y = 0; y < water->h; y++)
      for (int x = 0; x < water->w; x++) {
        auto pix = toPixel(water->n(x, y));
        bitmap.pixels.plot(x, y, pix.r, pix.g, pix.b);
      }
    printf("screenshot saved to file \'save.bmp\'\n");
  }
  bitmap.save("save.bmp");
  bitmap.unlock();
}

void step() {
  if (sdl.keyDown('1'))
    drawType = DrawType::Height;
  if (sdl.keyDown('2'))
    drawType = DrawType::Divergence;
  if (sdl.keyDown('3'))
    drawType = DrawType::Gradient;

  if (sdl.mouseKeyPress(0)) {
    printf("drip...\n");
    for (int y = 0; y < drop.h; y++)
      for (int x = 0; x < drop.w; x++) {
        int xo = sdl.mouseX - drop.w / 2 + x;
        int yo = sdl.mouseY - drop.h / 2 + y;
        (*water)(xo, yo) = drop.get(x, y) * 0.33f;
        if ((*water)(xo, yo) > 1.0f)
          (*water)(xo, yo) = 1.0f;
        if ((*water)(xo, yo) < -1.0f)
          (*water)(xo, yo) = -1.0f;
      }
  }
  water->step();
}

void draw() {

  auto pixels = sdl.lock();
  for (int y = 0; y < water->h; y++)
    for (int x = 0; x < water->w; x++) {
      Pixel24 pix;
      if (drawType == DrawType::Height)
        pix = toPixel((*water)(x, y));
      else if (drawType == DrawType::Divergence) {
        float v = water->n_xx(x, y) + water->n_yy(x, y);
        pix = toPixel(v * 5.0f);
      } else if (drawType == DrawType::Gradient) {
        constexpr float subdue = 0.33f;
        float b = std::clamp(water->n(x, y), 0.0f, 1.0f);
        float r = std::clamp(water->n_x(x, y), -1.0f, 1.0f);
        float g = std::clamp(water->n_y(x, y), -1.0f, 1.0f);

        float len = sqrtf(r * r + g * g);
        if (len > 0.0f) {
          r /= len;
          g /= len;
          r *= subdue;
          g *= subdue;
        }
        r = 0.5f + r * 0.5f;
        g = 0.5f + g * 0.5f;
        pix = Pixel24(r * 255.0f, g * 255.0f, b * 255.0f);
      }
      pixels.plot(x, y, pix.r, pix.g, pix.b);
    }
}

int main(int argc, char *args[]) {
  setbuf( stdout, NULL);
  if (!sdl.init( DISP_W, DISP_H, false))
    return 0;

  setbuf( stdout, NULL);

  double drawTimeInSecs = 0.0;
  double stepTimeInSecs = 0.0;
  Uint32 drawCount = 0.0;
  Uint32 stepCount = 0.0;
  double invFreq = 1.0 / sdl.getPerfFreq();

  auto start = sdl.getTicks();
  auto last = start;
  init();
  while (true) {
    auto now = sdl.getTicks();
    auto elapsed = now - last;
    if (elapsed >= 20) {
      sdl.pump();
      if (sdl.keyDown(SDLK_ESCAPE))
        break;

      Uint64 begin = sdl.getPerfCounter();
      step();
      stepCount++;
      stepTimeInSecs += (double) (sdl.getPerfCounter() - begin) * invFreq;
      last = (now / 20) * 20;
    }

    Uint64 begin = sdl.getPerfCounter();
    draw();
    drawCount++;
    sdl.swap();
    drawTimeInSecs += (double) (sdl.getPerfCounter() - begin) * invFreq;
  }
  term();
  if (stepCount)
    printf("Average step time: %f ms\n",
           (float) ((stepTimeInSecs * 1e3) / (double) drawCount));
  if (drawCount)
    printf("Average draw time: %f ms\n",
           (float) ((drawTimeInSecs * 1e3) / (double) drawCount));

  sdl.term();
  printf("goodbye!\n");
  return 0;
}
