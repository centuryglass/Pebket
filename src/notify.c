#include <pebble.h>
#include "notify.h"
#include "util.h"
#include "message_handler.h"
#include "page_menu.h"
#include "page_view.h"

#define NOTIFY_DEBUG_ENABLED//comment out to disable menu debug logs
#ifdef NOTIFY_DEBUG_ENABLED
#define NOTIFY_DEBUG(fmt, ...) APP_LOG(APP_LOG_LEVEL_DEBUG,fmt,##__VA_ARGS__);
#define NOTIFY_ERROR(fmt, ...) APP_LOG(APP_LOG_LEVEL_ERROR,fmt,##__VA_ARGS__);
#else
#define NOTIFY_DEBUG(fmt, args...) 
#define NOTIFY_ERROR(fmt, args...) 
#endif

Window * notifyWindow = NULL; 
TextLayer * notifyTextLayer = NULL;
char * text = NULL;
GColor windowColor = {GColorWhiteARGB8};
AppTimer * notifyTimer = NULL;
int timerDuration = 0;
GFont notifyFont;
NotifyCallbacks callbacks = {0};

void notify_window_timeout(void * data);
void notify_window_load(Window * window);
void notify_window_appear(Window * window);
void notify_window_disappear(Window * window);
void notify_window_unload(Window * window);

//Shows a new notification screen
void show_notification(char * notifyMsg, int duration, GColor bgColor, NotifyCallbacks nCallbacks){
  NOTIFY_DEBUG("show_notification: showing notification %s",notifyMsg);
  if(notifyWindow != NULL) hide_notification(NULL);
  callbacks = nCallbacks;
  #ifdef PBL_COLOR
  windowColor = bgColor;
  #endif
  timerDuration = duration;
  text = malloc_strcpy(text, notifyMsg);
  //create window
  if(notifyWindow == NULL){
    notifyWindow = window_create();
    window_set_background_color(notifyWindow, windowColor);
    window_set_window_handlers(notifyWindow, (WindowHandlers) {
    .load = notify_window_load,
    .appear = notify_window_appear,
    .disappear = notify_window_disappear,
    .unload = notify_window_unload
  });
  }
  window_stack_push(notifyWindow, true);
}

void hide_notification(){
  if(notifyWindow == NULL)return;
  NOTIFY_DEBUG("hide_notification: closing normally, callback = %d",(int)callbacks.onNormalClose);
  if(callbacks.onNormalClose != NULL) callbacks.onNormalClose();
  //disable unneeded closing callbacks and close window
  callbacks = (NotifyCallbacks){0};
  //cancel any unneeded timers
  if(notifyTimer != NULL){
    app_timer_cancel(notifyTimer);
    notifyTimer = NULL;
  }
  //remove window
  if(notifyWindow != NULL){
    window_stack_remove(notifyWindow, true);
    window_destroy(notifyWindow);
    notifyWindow = NULL;
  }
}

void notify_window_timeout(void * data){
  NOTIFY_DEBUG("notify_window_timeout: timer finished, callback = %d",(int)callbacks.onTimeout);
  if(callbacks.onTimeout != NULL) callbacks.onTimeout();
  //disable unneeded closing callbacks and close window
  callbacks = (NotifyCallbacks){0};
  hide_notification();
  
}

void notify_window_load(Window * window){
  NOTIFY_DEBUG("notify_window_load: window loading...");
  //create text layer
  GRect frame = layer_get_frame(window_get_root_layer(window));
  if(notifyTextLayer == NULL){
    notifyTextLayer = text_layer_create(frame);
    text_layer_set_text(notifyTextLayer,text);
    text_layer_set_text_alignment(notifyTextLayer, GTextAlignmentCenter);
    notifyFont = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
    text_layer_set_font(notifyTextLayer,notifyFont);
  }
  frame = layer_get_frame(text_layer_get_layer(notifyTextLayer));
  GSize textSize = text_layer_get_content_size(notifyTextLayer);
  frame.origin.y = (frame.size.h - textSize.h)/2;
  frame.size.h -= frame.origin.y;
  text_layer_set_background_color(notifyTextLayer,windowColor);
  layer_set_frame(text_layer_get_layer(notifyTextLayer),frame);
  layer_add_child(window_get_root_layer(window),text_layer_get_layer(notifyTextLayer));
  #ifdef PBL_ROUND
  text_layer_enable_screen_text_flow_and_paging(notifyTextLayer, 5);
  #endif
  if(timerDuration > 0)notifyTimer = app_timer_register(timerDuration * 1000, notify_window_timeout, NULL);
  else notifyTimer = NULL;
  
}

void notify_window_disappear(Window * window){
  if(window_stack_contains_window(window)){
    NOTIFY_DEBUG("notify_window_disappear: disappear callback = %d",(int)callbacks.onDisappear);
    if(callbacks.onDisappear != NULL) callbacks.onDisappear();
  }
}

void notify_window_appear(Window * window){
  NOTIFY_DEBUG("notify_window_appear: appear callback = %d",(int)callbacks.onAppear);
  if(callbacks.onAppear != NULL) callbacks.onAppear();
}

void notify_window_unload(Window * window){
  NOTIFY_DEBUG("notify_window_unload:unloading,suddenClose callback = %d",(int)callbacks.onSuddenClose);
  if(notifyTimer != NULL){
    app_timer_cancel(notifyTimer);
    notifyTimer = NULL;
  }
  //run sudden close callback if present,
  //this will have been removed if closed normally
  if(callbacks.onSuddenClose != NULL) callbacks.onSuddenClose();
  //close window
  hide_notification();
  //remove text layer
  if(notifyTextLayer != NULL){
    text_layer_destroy(notifyTextLayer);
    notifyTextLayer = NULL;
  }
  //remove text
  if(text != NULL){
    free(text);
    text = NULL;
  }
}

//Predefined callbacks:

//Closes any pageView window
void closeText(){
  NOTIFY_DEBUG("running closeText callback");
  unload_page();
}

//Closes pageView and pageMenu
void closeList(){
  NOTIFY_DEBUG("running closeList callback");
  unload_page();
  destroy_page_menu();
}

//closes the entire app
void closeApp(){
  NOTIFY_DEBUG("running closeApp callback");
  window_stack_pop_all(true);
}

//reloads and opens the page list
void reloadList(){
  NOTIFY_DEBUG("running reloadList callback");
  unload_page();
  destroy_page_menu();
}

