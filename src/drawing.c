#include <pebble.h>
#include "drawing.h"
#include "options.h"

#define PADDING_SIZE 2
//#define DRAW_DEBUG_ENABLED//comment out to disable menu debug logs
#ifdef DRAW_DEBUG_ENABLED
#define DRAW_DEBUG(fmt, ...) APP_LOG(APP_LOG_LEVEL_DEBUG,fmt,##__VA_ARGS__);
#define DRAW_ERROR(fmt, ...) APP_LOG(APP_LOG_LEVEL_ERROR,fmt,##__VA_ARGS__);
#else
#define DRAW_DEBUG(fmt, args...) 
#define DRAW_ERROR(fmt, args...) 
#endif
static GRect adjust_icon_bounds(GRect rawBounds);

static GRect adjust_icon_bounds(GRect bounds){
  //icon should be square
  if(bounds.size.w > bounds.size.h){
    bounds.origin.x += ((bounds.size.w-bounds.size.h)/2);
    bounds.size.w = bounds.size.h;
  }else if(bounds.size.h > bounds.size.w){
    bounds.origin.y += ((bounds.size.h-bounds.size.w)/2);
    bounds.size.h = bounds.size.w;
  }
  //icon should be padded
  bounds.size.w -= PADDING_SIZE * 2;
  bounds.size.h -= PADDING_SIZE * 2;
  bounds.origin.x += PADDING_SIZE;
  bounds.origin.y += PADDING_SIZE;
  return bounds;
}



void draw_color_preview(GContext *ctx, GRect bounds, GColor color){
  graphics_context_set_stroke_width(ctx,3);
  //GRect colorFrame = adjust_icon_bounds(bounds);
  graphics_draw_rect(ctx, bounds);
  graphics_context_set_fill_color(ctx, color);
  graphics_fill_rect(ctx, bounds, 6, GCornersAll);
}

void draw_font_preview(GContext *ctx, GRect bounds, GFont font){
  graphics_context_set_stroke_width(ctx,3);
  GRect fontBounds = adjust_icon_bounds(bounds);
  graphics_draw_rect(ctx, fontBounds);
  fontBounds.origin.y -= 2;
  graphics_draw_text(ctx,
                    "A",
                    font,
                    fontBounds,
                    GTextOverflowModeWordWrap,
                    GTextAlignmentCenter,NULL);
}

void draw_checkbox(GContext *ctx, GRect bounds, bool checked){
  graphics_context_set_stroke_width(ctx,3);
  GRect checkbox = adjust_icon_bounds(bounds);
  graphics_draw_rect(ctx, checkbox);
  if(checked){//draw check mark
    GPoint p1 = GPoint(checkbox.origin.x, checkbox.origin.y + checkbox.size.h/2 - 3);
    GPoint p2 = GPoint(checkbox.origin.x + checkbox.size.w/3, checkbox.origin.y + checkbox.size.h - 3);
    GPoint p3 = GPoint(checkbox.origin.x + checkbox.size.w, checkbox.origin.y);
    graphics_draw_line(ctx, p1, p2);
    graphics_draw_line(ctx, p2, p3);
  }
}