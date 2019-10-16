#include <pebble.h>
#include "util.h"
#include "storage_keys.h"
#include "debug.h"
#include "message_handler.h"
#include "main_menu.h"
#include "page_menu.h"
#include "page_view.h"
#include "options.h"

//----------LOCAL VALUE DEFINITIONS----------
#define DEBUG_MAIN  //uncomment to enable main program debug logging

//----------STATIC FUNCTIONS----------

void handle_init(void) {
  init_options();
  //Register app message functions
  message_handler_init();
  init_main_menu();
}

//unload program
void handle_deinit(void) {
  messaging_deinit();
  destroy_main_menu();
  destroy_page_menu();
  destroy_options();
  unload_page();
}
int main(void) {
  handle_init();
  app_event_loop();
  handle_deinit();
}