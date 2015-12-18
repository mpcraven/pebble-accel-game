#pragma once

typedef struct Vec2d {
  double x;
  double y;
} Vec2d;

typedef struct Disc {
#ifdef PBL_COLOR
  GColor color;
#endif
  Vec2d pos;
  Vec2d vel;
  double mass;
  double radius;
  int number;
} Disc;

typedef struct Brick {
#ifdef PBL_COLOR
  GColor color;
#endif
  Vec2d pos;
  Vec2d center;
  int special;
  int hit;
  int hitting;
} Brick;
