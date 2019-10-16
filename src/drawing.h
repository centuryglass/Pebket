#pragma once
#include <pebble.h>
typedef enum{
  NO_IMAGE,
  CHECKBOX,
  COLOR_PREVIEW,
  FONT_PREVIEW,
}ImageType;

void draw_color_preview(GContext *ctx, GRect bounds, GColor color);
void draw_font_preview(GContext *ctx, GRect bounds, GFont font);
void draw_checkbox(GContext *ctx, GRect bounds, bool checked);

