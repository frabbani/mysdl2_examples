#pragma once

#include "flow.h"

struct Water : public ScalarIntegrator {
  float cSq = 0.0f;  //celerity squared (less than 1.0f/DT * DT)
  Field<float> dn_dt;

  Water( int w_, int h_, float cSq_ ) : ScalarIntegrator( w_, h_), cSq(cSq_){
    dn_dt.init( w, h );
  }

  float n( int x, int y ) const {
    return fields[curr].get( x, y );
  }

  float n_t(int x, int y)  override {
    return dn_dt.get( x, y );
  }

  void sim() override {
    for( int y = 0; y < h; y++ )
      for( int x = 0; x < w; x++ ){
        float n_tt = cSq * ( n_xx(x,y) + n_yy(x,y) );
        dn_dt( x, y ) += n_tt * DT;
      }
  }

  float& operator()(int x, int y) {
    return ScalarIntegrator::operator ()(x, y);
  }

  void step(){
    ScalarIntegrator::step();
  }


};
