// accel_game_1
// Pebble game with disc(s) - controlled with accelerometer - and blocks to hit with the disc(s)
// Adapted from accel-discs Pebble example https://github.com/pebble-examples/feature-accel-discs/ 
// Fill blocks by hitting with disc for 1,2 or 4 points depending on level, avoiding special X block -10 points
// Level designs include whole or partial bricks to hit & fill, multiple discs, varying disc size, varying speed.
// 56 levels, one new level per minute. After 56 the levels repeat.

#include "pebble.h"
#include "accel_game_1.h"
#include "round_math.h"

#define MATH_PI 3.141592653589793238462
#define NUM_DISCS 2     // start with one disc and increase to this value during game at higher levels
#define NUM_BRICKS 8    // best to make this at least 6 so there is one brick per row. extra bricks are added to existing rows  
#define NUM_BITMAPS 6   // different bricks. do not change.
#define BRICK_H 20      // brick height. do not change.
#define BRICK_W 30      // brick width
#define DISC_START_RADIUS 8 // disc size starts with this. second disc is 0.5 bigger
#define DISC_END_RADIUS 6   // minimum size of disc
#define DISC_DENSITY 0.25   // used to calculate the mass of the disc - to determine its acceleration.
#define ACCEL_RATIO 0.05    // used to calculate the force.
#define ACCEL_STEP_MS 25    // accelerometer data rate - can be reduced to speed up thr game.

static Window *s_main_window;
static Layer *s_disc_layer;
static Layer *s_brick_layer;
static Layer *s_level_layer; 
static TextLayer *s_text_layer;

static GBitmap *s_brick_bitmap[NUM_BITMAPS];

static int s_uptime = 0;     // time in seconds from start of game

static Disc disc_array[NUM_DISCS];    
static Brick brick_array[NUM_BRICKS];

static int s_num_bricks;   
static int s_game_score;
static int s_num_discs;
static int s_accel_step_ms;
static int s_disc_radius;

static int s_level;
static int s_brick_level;
static GRect window_frame;

static double disc_calc_mass(Disc *disc) {
  return MATH_PI * disc->radius * disc->radius * DISC_DENSITY;
}

static void brick_init(Brick *brick) {
  brick->pos.x = BRICK_W * (rand() % (window_frame.size.w/BRICK_W));  // random column position
  brick->pos.y = (s_num_bricks%6)*BRICK_H; // at least one brick per row if NUM_BRICKS >=6
  brick->hit=0;  // records collision that occur between this brick and any disc
  brick->hitting=0; // prevents multiple hits - resets at column one after the brick has moves off screen
  brick->special=0;    // this brick is the special X brick which should be avoided 
#ifdef PBL_COLOR
  brick->color = GColorFromRGB(rand() % 255, rand() % 255, rand() % 255);   // used for Pebble Time
#endif
  s_num_bricks++;   // count the number of bricks during initialisation, to increase row position of the brick
}

static void disc_init(Disc *disc) {

  static double next_radius = DISC_START_RADIUS;
  
  static int number = 0;
  number++; 
  disc->number=number;  // the disc number ranges between 1 and NUM_DISC. 
                        // used to recompute the mass in disc_update when radius changes at higher levels

  GRect frame = window_frame;
  disc->pos.x = (frame.size.w / 2);   // all discs start in the middle of the screen
  disc->pos.y = (frame.size.h / 2);  
  disc->vel.x = 0;
  disc->vel.y = 0;
  disc->radius = next_radius;         // this value is now overridden in disc_update
  disc->mass = disc_calc_mass(disc);
#ifdef PBL_COLOR
  disc->color = GColorFromRGB(rand() % 255, rand() % 255, rand() % 255);
#endif
  next_radius += 0.5;    // this is no longer used - done in disc_update instead
}

static void disc_apply_force(Disc *disc, Vec2d force) {
  disc->vel.x += force.x / disc->mass;
  disc->vel.y += force.y / disc->mass;
}

static void disc_apply_accel(Disc *disc, AccelData accel) {
  disc_apply_force(disc, (Vec2d) {
    .x = accel.x * ACCEL_RATIO,
    .y = -accel.y * ACCEL_RATIO
  });
}

static void disc_update(Disc *disc) {

  // update disc position
  disc->pos.x += disc->vel.x;
  disc->pos.y += disc->vel.y;

#ifdef PBL_ROUND
  double e=0.7;
  // -1 accounts for how pixels are drawn onto the screen. Pebble round has a 180x180 pixel screen.
  // Since this is an even number, the centre of the screen is a line separating two side by side
  // pixels. Thus, if you were to draw a pixel at (90, 90), it would show up on the bottom right
  // pixel from the center point of the screen.
  Vec2d circle_center = (Vec2d) { .x = window_frame.size.w / 2 - 1,
                                  .y = window_frame.size.h / 2 - 1 };

  if ((square(circle_center.x - disc->pos.x) + square(circle_center.y - disc->pos.y)) > square(circle_center.x - disc->radius)) {
    // Check to see whether disc is within screen radius
    Vec2d norm = subtract(disc->pos, circle_center);
    if (get_length(norm) > (circle_center.x - disc->radius)) {
      norm = set_length(norm, (circle_center.x - disc->radius), get_length(norm));
      disc->pos = add(circle_center, norm);
    }
    disc->vel = multiply(find_reflection_velocity(circle_center, disc), e);
  }
#else
  double e = 0.5;

  if ((disc->pos.x - disc->radius < 0 && disc->vel.x < 0)
    || (disc->pos.x + disc->radius > window_frame.size.w && disc->vel.x > 0)) {
    disc->vel.x = -disc->vel.x * e;
  }

 // amended to avoid overlap with time/score bar
  if ((disc->pos.y - disc->radius < 0 && disc->vel.y < 0)
    || (disc->pos.y + disc->radius > window_frame.size.h-BRICK_H && disc->vel.y > 0)) {
    disc->vel.y = -disc->vel.y * e;
  }
#endif
}

static void brick_update(Brick *brick) {
  
  // update brick position
  brick->pos.x += 1;
  if (brick->pos.x >= window_frame.size.w) {
     brick->pos.x =0;
     brick->pos.y +=BRICK_H/2;
     if (brick->hit==4) brick->hit=0;
     if (brick->hitting==1) brick->hitting=0;
     if (rand()%8==0) brick->special=1; else brick->special=0;
  }

 if (brick->pos.y >= window_frame.size.h-(BRICK_H*2)) {
      brick_init(brick);
      brick->pos.x = 0; 
      brick->pos.y = 0;
//      brick->hit=0;
  }
  
  // update centers (used to detect collisions with discs) 
    brick->center.x = brick->pos.x+BRICK_W/2;
    brick->center.y = brick->pos.y+BRICK_H/2;

}

static void brick_draw(GContext *ctx, Brick *brick) {

  // force to black to avoid compile error in SDK2 
    graphics_context_set_fill_color(ctx, GColorBlack);

#ifdef PBL_COLOR
  graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(brick->color, GColorBlack));
#endif

  if (brick->hit==0 && brick->special==1) {    
    graphics_draw_bitmap_in_rect(ctx, s_brick_bitmap[5], GRect(brick->pos.x, brick->pos.y,BRICK_W,BRICK_H));
  }
  else
  {
    graphics_draw_bitmap_in_rect(ctx, s_brick_bitmap[brick->hit], GRect(brick->pos.x, brick->pos.y,BRICK_W,BRICK_H));
  }

}

static void disc_draw(GContext *ctx, Disc *disc) {

  // force to black to avoid compile error in SDK2 
    graphics_context_set_fill_color(ctx, GColorBlack);

  #ifdef PBL_COLOR
  graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(disc->color, GColorBlack));
#endif

  disc->radius=s_disc_radius+(0.5*disc->number);
  disc->mass = disc_calc_mass(disc);  
  graphics_fill_circle(ctx, GPoint(disc->pos.x, disc->pos.y), disc->radius);

  // update text more often! 
  // Use a long-lived buffer
  static char s_text_buffer[64];

  // Get time since launch
  int seconds = s_uptime % 60;
  int minutes = s_uptime / 60;
//  int minutes = (s_uptime % 3600) / 60;
//  int hours = s_uptime / 3600;

  // Update the TextLayer
  snprintf(s_text_buffer, sizeof(s_text_buffer), "%dm%ds score %d", minutes, seconds, s_game_score);
  text_layer_set_text(s_text_layer, s_text_buffer);
  
}

static void disc_layer_update_callback(Layer *me, GContext *ctx) {
  for (int i = 0; i < s_num_discs; i++) {
    disc_draw(ctx, &disc_array[i]);
  }
}

static void brick_layer_update_callback(Layer *me, GContext *ctx) {
  for (int i = 0; i < NUM_BRICKS; i++) {
    brick_draw(ctx, &brick_array[i]);
  }
}

static void level_layer_update_callback(Layer *me, GContext *ctx) {
// add high score to screen 
  if (s_uptime%60<12 && s_uptime>0) {
  static char buf[] = "000000000000000000";    /* <-- implicit NUL-terminator at the end here */
  snprintf(buf, sizeof(buf), "Level %d", s_level);
  
  GRect frame = GRect(0, 80-(4*s_uptime%60), 144, 40);
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, 
  buf,
  fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
  frame,
  GTextOverflowModeTrailingEllipsis,
  GTextAlignmentCenter,
  NULL
  );
  }
}


static void check_collision(Brick *brick, Disc *disc)
{
  // check to see if the centres of a disk & brick are close enough, and assign score
  // also make sure the disc has moved away before hitting again 
   if(brick->hitting==0) { 
    if(  
      ((brick->center.x>=disc->pos.x && brick->center.x-disc->pos.x<=BRICK_W/2) || 
       (brick->center.x<disc->pos.x && disc->pos.x-brick->center.x<BRICK_W/2)) &&
      ((brick->center.y>=disc->pos.y && brick->center.y-disc->pos.y<=BRICK_H/2) || 
       (brick->center.y<disc->pos.y && disc->pos.y-brick->center.y<BRICK_H/2))
    )
    {
        brick->hitting=1;
        if (brick->hit<4)
        {
          if (brick->special==1 && brick->hit==0) 
          {
             s_game_score-=10;                 // penalise hits on the X brick
             vibes_short_pulse();
             brick->hit=4;
          }
          else 
          {
              if (s_brick_level==1) brick->hit=4;     // whole brick   
              if (s_brick_level==2) brick->hit+=2;    // half a brick
              if (s_brick_level==3) brick->hit++;     // quarter of a brick 
              if (brick->hit==4) s_game_score+=s_brick_level;  // add 1,2 or 4 depending on brick level 
          }  
        }
    }
  }
}

// main loop for triggering updates to disc and brick and collision checking
static void timer_callback(void *data) {
  AccelData accel = (AccelData) { .x = 0, .y = 0, .z = 0 };
  accel_service_peek(&accel);
  
  // check for disk collisions from last update
  for (int i = 0; i < s_num_discs; i++) {
    for (int j = 0; j < NUM_BRICKS; j++) {
      Disc *disc = &disc_array[i];
      Brick *brick = &brick_array[j];
      check_collision(brick, disc);
    }
  }
  
  // update disc position
  for (int i = 0; i < s_num_discs; i++) {
    Disc *disc = &disc_array[i];
    disc_apply_accel(disc, accel);
    disc_update(disc);
  }
  layer_mark_dirty(s_brick_layer);
  
  //update brick positions
  for (int i = 0; i < NUM_BRICKS; i++) {
    Brick *brick = &brick_array[i];
    brick_update(brick);
  }
  layer_mark_dirty(s_disc_layer);

  // update level display
  layer_mark_dirty(s_level_layer);

  // do this again
  app_timer_register(s_accel_step_ms, timer_callback, NULL);
}

static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect frame = window_frame = layer_get_frame(window_layer);
  
  s_brick_layer = layer_create(frame);
  layer_set_update_proc(s_brick_layer, brick_layer_update_callback);
  layer_add_child(window_layer, s_brick_layer);

  s_disc_layer = layer_create(frame);
  layer_set_update_proc(s_disc_layer, disc_layer_update_callback);
  layer_add_child(window_layer, s_disc_layer);
  
  s_level_layer = layer_create(frame);
  layer_set_update_proc(s_level_layer, level_layer_update_callback);
  layer_add_child(window_layer, s_level_layer);

  srand(time(NULL)); // create random seed
  
  for (int i = 0; i < NUM_DISCS; i++) {
    disc_init(&disc_array[i]);
  }

  for (int i = 0; i < NUM_BRICKS; i++) {
    brick_init(&brick_array[i]);
  }

  // Create bitmap for the special brick and other bricks
  s_brick_bitmap[0]= gbitmap_create_with_resource(RESOURCE_ID_0_RECT_30_20);
  s_brick_bitmap[1]= gbitmap_create_with_resource(RESOURCE_ID_1_RECT_30_20);
  s_brick_bitmap[2]= gbitmap_create_with_resource(RESOURCE_ID_2_RECT_30_20);
  s_brick_bitmap[3]= gbitmap_create_with_resource(RESOURCE_ID_3_RECT_30_20);
  s_brick_bitmap[4]= gbitmap_create_with_resource(RESOURCE_ID_4_RECT_30_20);  
  s_brick_bitmap[5]= gbitmap_create_with_resource(RESOURCE_ID_SPECIAL_RECT_30_20);

   // Create output TextLayer in bottom of screen - used for the time and score
  // not tested on Pebble Round and probably won't show properly on round screen - but could be modified accordingly
  s_text_layer = text_layer_create(GRect(0, 134, 144, BRICK_H));
  text_layer_set_text_alignment(s_text_layer, GTextAlignmentCenter);
  text_layer_set_font(s_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_background_color(s_text_layer, GColorBlack);
  text_layer_set_text_color(s_text_layer, GColorWhite);
  text_layer_set_text(s_text_layer, "0m0s score 0");

  layer_add_child(window_layer, text_layer_get_layer(s_text_layer));

  // initialise score and other static values
  s_game_score=0;
  s_num_discs=1;   // increase from this value up to NUM_DISCS during game
  s_num_bricks=1;  // ditto for NUM_BRICKS
  s_brick_level=1;  // increases to 2 or 4 then back to 1 
  s_level=1;       // increases every minute
  s_disc_radius=DISC_START_RADIUS;
  s_accel_step_ms=ACCEL_STEP_MS;
}

// Displays the elapsed time in seconds
static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  // Use a long-lived buffer
  static char s_text_buffer[64];

  // Get time since launch
  int seconds = s_uptime % 60;
  int minutes = s_uptime / 60;
//  int minutes = (s_uptime % 3600) / 60;
//  int hours = s_uptime / 3600;

  // Update the TextLayer
  snprintf(s_text_buffer, sizeof(s_text_buffer), "%dm%ds score %d", minutes, seconds, s_game_score);
  text_layer_set_text(s_text_layer, s_text_buffer);
  // Increment s_uptime
  s_uptime++;
  
  // new level every 60 secs
  // With NUM_DISCS=2, DISC_START_RADIUS=8, DISC_END_RADIUS=6 and ACCEL_STEP_MS=25, we get the following levels
  // First, Increase brick complexity level from 1 whole brick to 2 halves,4 quarters then back to 1 whole. Levels 1-3
  // then, Increase number of discs to 2 then back to 1. Levels 4-6
  // then, Decrease disc radius by one from 8 to 7 to 6 then back to 8 (also, second disc is 0.5 bigger). Levels 7-18 
  // then, Increase update rate 25, 20, 15 ms then back to 25ms. Levels 19-56. 
  // then, level 57 is the same as level 1.
  if (s_uptime % 60 == 0)
  {
    s_level++;
    if (s_brick_level<3) s_brick_level++; 
    else {
      s_brick_level=1; // reset brick level
      if(s_num_discs<NUM_DISCS) {
        s_num_discs++; 
      }
      else {
        s_num_discs=1; // reset disc number
        if(s_disc_radius>DISC_END_RADIUS) {
          s_disc_radius-=1;
        }
        else
        { 
           s_disc_radius=DISC_START_RADIUS;        
           if (s_accel_step_ms>15) {
             s_accel_step_ms-=5;
           } 
           else {
             s_accel_step_ms=ACCEL_STEP_MS;
           }
        }
      }
    }
  }
  
  // keep backlight on
  light_enable_interaction();  
}

static void main_window_unload(Window *window) {
  layer_destroy(s_disc_layer);
  layer_destroy(s_brick_layer);
  layer_destroy(s_level_layer);
  text_layer_destroy(s_text_layer);
}

static void init(void) {
  s_main_window = window_create();

  // Give main window a white background and define its handlers
  window_set_background_color(s_main_window, GColorWhite);
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });
  window_stack_push(s_main_window, true);

  // Subscribe to TickTimer Service to display time in minutes and seconds
  tick_timer_service_subscribe(SECOND_UNIT, tick_handler);

  // Subscribe to Accelerometer Service to capture x,y,x data to move the disc(s)
  accel_data_service_subscribe(0, NULL);
  
  // Register a timer
  app_timer_register(ACCEL_STEP_MS, timer_callback, NULL);  
}

static void deinit(void) {
  accel_data_service_unsubscribe();
  tick_timer_service_unsubscribe();
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
