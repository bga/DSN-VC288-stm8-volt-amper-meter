#pragma once

// #include "common.h"

namespace _7SegmentsFont {

  enum {
    /*
       000
      5   1
      5   1
      5   1
       666
      4   2
      4   2
      4   2
       333
    */
    s0 = _BV(3),
    s1 = _BV(0),
    s2 = _BV(2),
    s3 = _BV(5),
    s4 = _BV(6),
    s5 = _BV(4),
    s6 = _BV(1),

    d0 = (s0 | s1 | s2 | s3 | s4 | s5),
    d1 = (s1 | s2),
    d2 = (s0 | s1 | s6 | s4 | s3),
    d3 = (s0 | s1 | s6 | s2 | s3),
    d4 = (s5 | s6 | s1 | s2),
    d5 = (s0 | s5 | s6 | s2 | s3),
    d6 = (s0 | s5 | s6 | s4 | s2 | s3),
    d7 = (s0 | s1 | s2),
    d8 = (s0 | s1 | s2 | s3 | s4 | s5 | s6),
    d9 = (s0 | s1 | s2 | s3 | s5 | s6),
    A = (s0 | s1 | s2 | s4 | s5 | s6),
    a = (s0 | s1 | s2 | s3 | s4 | s6),
    B = (s2 | s3 | s4 | s5 | s6),
    C = (s0 | s5 | s4 | s3),
    c = (s3 | s4 | s6),
    D = (s1 | s2 | s3 | s4 | s6),
    E = (s0 | s3 | s4 | s5 | s6),
    F = (s0 | s4 | s5 | s6),
    G = (s2 | s3 | s4 | s5),
    H = (s2 | s4 | s5 | s6),
    I = (s4),
    J = (s1 | s2 | s3 | s3),
    K = (s0 | s2 | s4 | s5 | s6),
    L = (s5 | s4 | s3),
    M = (s2 | s4 | s6),
    O = (s2 | s3 | s4 | s6),
    P = (s0 | s5 | s1 | s6 | s4), 
    Q = (s0 | s1 | s2 | s5 | s6),
    R = (s0 | s1 | s4 | s5),
    S = (s0 | s5 | s6 | s2 | s3),
    T = (s3 | s4 | s5 | s6),
    U = (s1 | s2 | s3 | s4 | s5),
    u = (s2 | s3 | s4),
    // V = (s1 | s2 | s3 | s5),
    // W = (s1 | s3 | s5),
    // X = (s1 | s2 | s4 | s5 | s6),
    Y = (s1 | s2 | s3 | s5 | s6),
    // Z = (s0 | s1 | s3 | s6),
    questionMark = (s0 | s1 | s4 | s6),
     // = (s0 | s1 | s2 | s3 | s4 | s5 | s6),
  };

  const U8 digits[] = {
    d0,
    d1,
    d2,
    d3,
    d4,
    d5,
    d6,
    d7,
    d8,
    d9, 
  };
  
  const U8 digitsHex[] = {
    d0,
    d1,
    d2,
    d3,
    d4,
    d5,
    d6,
    d7,
    d8,
    d9, 
    A, 
    B, 
    C, 
    D, 
    E, 
    F, 
  };
}


