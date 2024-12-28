#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>

#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <thread>
#include <chrono>
#include <memory>

#include "mysdl2.h"
#include "player.h"

/*
  Link:
    SDL2, SDL2main, SDL2_image, SDL2_ttf, avformat, avcodec, avutil, swscale
*/


#define DISP_W 640
#define DISP_H 480

using namespace sdl2;

SDL sdl;

std::atomic<bool> run(true);
std::vector<Pixel24> buffer( DISP_W * DISP_H);
std::thread work;

std::mutex mut;

void video(std::string_view url) {
 std::unique_ptr<Player> player = std::make_unique<Player>(url, DISP_W,
                                                            DISP_H);
  while (run) {
    auto start = std::chrono::high_resolution_clock::now();
    player->play();
    player->decode();
    {
      std::lock_guard<std::mutex> lg(mut);
      for(int i = 0; i < DISP_W * DISP_H; i++){
        uint8_t *data = &player->buffer[i*3];
        buffer[i].r = data[0];
        buffer[i].g = data[1];
        buffer[i].b = data[2];

      }
    }
    auto stop = std::chrono::high_resolution_clock::now();
    double elapsed =
        double(
            std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start)
                .count()) * 1e-6;

    double delay = 1000.0 / player->fps - elapsed;
    std::this_thread::sleep_for(std::chrono::milliseconds(int(delay)));
  }
}

void init() {
  printf("*** INIT ***\n");
  work =
      std::thread(
          video,
          "http://commondatastorage.googleapis.com/gtv-videos-bucket/sample/ElephantsDream.mp4");
  printf("************\n");
}

void step() {
}

void draw() {
  auto bg = sdl.lock();

  Pixel24 bgColor(180, 200, 214);

  bg.inverted = false;
//  for (int y = 0; y < bg.h; y++)
//    for (int x = 0; x < bg.w; x++)
//      bg.plot(x, y, bgColor);

  std::lock_guard<std::mutex> lg(mut);
  for (int y = 0; y < bg.h; y++)
    for (int x = 0; x < bg.w; x++)
      bg.plot(x, y, buffer[y * DISP_W + x]);

}

void term() {
  printf("*** TERM ***\n");
  run = false;
  work.join();
  printf("************\n");
}

int main(int argc, char *args[]) {
  setbuf( stdout, NULL);

  if (!sdl.init( DISP_W, DISP_H, false, "go with the flow")) {
    return 0;
  }

  double drawTimeInSecs = 0.0;
  Uint32 drawCount = 0.0;
  double invFreq = 1.0 / sdl.getPerfFreq();

  auto start = sdl.getTicks();
  auto last = start;
  init();

  while (true) {
    auto now = sdl.getTicks();
    auto elapsed = now - last;
    if (sdl.keyDown(SDLK_ESCAPE))
      break;
    if (elapsed >= 20) {
      sdl.pump();
      step();
      last = (now / 20) * 20;
    }
    Uint64 beginCount = sdl.getPerfCounter();
    draw();
    drawCount++;
    sdl.swap();
    drawTimeInSecs += (double) (sdl.getPerfCounter() - beginCount) * invFreq;
  }
  term();
  if (drawCount)
    printf("Average draw time: %f ms\n",
           (float) ((drawTimeInSecs * 1e3) / (double) drawCount));

  sdl.term();

  printf("goodbye!\n");
  return 0;
}
