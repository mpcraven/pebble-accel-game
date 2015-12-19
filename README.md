# pebble-accel-game
Pebble game using accelerometer

Note: The .png graphics needed to build the game in the SDK/CloudPebble can be found zipped in the Issues tab.
Also: A .pbw file (built in SDK2 for Pebble or Pebble Steel) to upload to the watch is also included the zip, as an alternative to building.

// accel_game_1
// Pebble game with disc(s) - controlled with accelerometer - and blocks to hit with the disc(s)
// Adapted from accel-discs Pebble example https://github.com/pebble-examples/feature-accel-discs/
// Fill blocks by hitting with disc for 1,2 or 4 points depending on level, avoiding special X block -10 points
// Level designs include whole or partial bricks to hit & fill, multiple discs, varying disc size, varying speed.
// 54 levels, one new level per minute, after which the levels repeat.
// Compiled and tested for Pebble/Pebble Steel. Not optimised for, nor tested on, Pebble Round, and not tested on Pebble Time, sorry, but it may still work (main difference will be Coloured discs).

