#pragma once

#include <vector>

// FDM central scheme with grid spacing of 1

#define DT 0.5  // since d^2(u)/dx^2 has a 2x multiplier

template<class T>
struct Field {
  int w = 0;
  int h = 0;
  std::vector<T> data;

  void bound(int &x, int &y) const {
    x %= w;
    y %= h;
    if (x < 0)
      x += w;
    if (y < 0)
      y += h;
  }

  void init(int w_, int h_) {
    w = abs(std::max(w_, 1));
    h = abs(std::max(h_, 1));
    data = std::vector<T>(w * h);
  }

  T& operator()(int x, int y) {
    bound(x, y);
    return data[y * w + x];
  }

  T get(int x, int y) const {
    bound(x, y);
    return data[y * w + x];
  }

};

struct ScalarField : public Field<float> {

  ScalarField(int w_, int h_) {
    init(w_, h_);
    for( int y = 0; y < h; y++ )
      for( int x = 0; x < w; x++ )
        Field<float>::operator()(x,y) = 0.0f;
  }

  float n_x(int x, int y) const {
    return 0.5f * (get(x + 1, y) - get(x - 1, y));
  }

  float n_xx(int x, int y) const {
    return get(x + 1, y) + get(x - 1, y) - 2.0f * get(x, y);
  }

  float n_y(int x, int y) const {
    return 0.5f * (get(x, y + 1) - get(x, y - 1));
  }

  float n_yy(int x, int y) const {
    return get(x, y + 1) + get(x, y - 1) - 2.0f * get(x, y);
  }
};

struct ScalarIntegrator {
  int w = 0, h = 0;
  ScalarField fields[3];

  int curr = 0;
  int next = 1;
  int prev = 2;

  ScalarIntegrator(int w_, int h_)
      :
      fields { { w_, h_ }, { w_, h_ }, { w_, h_ } } {
    w = fields[0].w;
    h = fields[0].h;
  }

  virtual ~ScalarIntegrator() = default;

  void step() {
    sim();
    for (int y = 0; y < h; y++)
      for (int x = 0; x < w; x++)
        fields[next](x, y) = fields[curr](x, y) + n_t(x, y) * DT;
    prev = curr;
    curr = next;
    next = (next + 1) % 3;
  }

  float& operator()(int x, int y) {
    return fields[curr](x, y);
  }

  float n_x(int x, int y) const {
    return fields[curr].n_x(x, y);
  }

  float n_xx(int x, int y) const {
    return fields[curr].n_xx(x, y);
  }

  float n_y(int x, int y) const {
    return fields[curr].n_y(x, y);
  }

  float n_yy(int x, int y) const {
    return fields[curr].n_yy(x, y);
  }

  virtual float n_t(int x, int y) {
    return 0.0f;
  }

  virtual void sim() {
  }
};
