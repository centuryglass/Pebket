#include <pebble.h>
#include "main_menu.h"
#include "message_handler.h"
#include "page_menu.h"
#include "options.h"

//----------LOCAL VALUE DEFINITIONS----------
//#define MAIN_MENU_DEBUG_ENABLED//comment out to disable menu debug logs
#ifdef MAIN_MENU_DEBUG_ENABLED
#define MAIN_MENU_DEBUG(fmt, ...) APP_LOG(APP_LOG_LEVEL_DEBUG,fmt,##__VA_ARGS__);
#define MAIN_MENU_ERROR(fmt, ...) APP_LOG(APP_LOG_LEVEL_ERROR,fmt,##__VA_ARGS__);
#else
#define MAIN_MENU_DEBUG(fmt, args...) 
#define MAIN_MENU_ERROR(fmt, args...) 
#endif
//----------STATIC FUNCTION DECLARATIONS----------
static void main_menu_window_create(Window * window);
static void main_menu_window_destroy(Window * window);
static void main_menu_window_appear(Window * window);
static void main_menu_layer_select(int index, void * context);

//----------MENU DATA----------
static Window * menuWindow = NULL;
static StatusBarLayer * statusBar = NULL;
GBitmap * listIcon = NULL;
GBitmap * favoriteIcon = NULL;
GBitmap * archiveIcon = NULL;
GBitmap * optionsIcon = NULL;

static SimpleMenuLayer * mainMenu = NULL;
static SimpleMenuItem menuItems [] = {
  {.title = "My List",
  .subtitle = NULL,
  .icon = NULL,
   .callback = main_menu_layer_select},
{.title = "Favorites",
  .subtitle = NULL,
  .icon = NULL,
   .callback = main_menu_layer_select},
{.title = "Archive",
  .subtitle = NULL,
  .icon = NULL,
   .callback = main_menu_layer_select},
{.title = "Options",
  .subtitle = NULL,
  .icon = NULL,
   .callback = main_menu_layer_select}};
static SimpleMenuSection mainSection []= {{
  .title = NULL,
  .items = menuItems,
  .num_items = 4
}};

typedef enum{
  MY_LIST,
  FAVORITES,
  ARCHIVE,
  OPTIONS
}MainMenuIndex;

//----------PUBLIC FUNCTIONS----------
//creates the main menu
void init_main_menu(){
  if(menuWindow == NULL){
    menuWindow = window_create();
    window_set_window_handlers(menuWindow, (WindowHandlers) {
      .load = main_menu_window_create,
      .appear = main_menu_window_appear,
      .unload = main_menu_window_destroy,
    });
    window_stack_push(menuWindow, true);
}}

//destroys the main menu
void destroy_main_menu(){
  if(menuWindow != NULL){
    window_stack_remove(menuWindow, true);
    window_destroy(menuWindow);
    menuWindow = NULL;
    destroy_page_menu();
  }
}

//----------STATIC FUNCTIONS----------

//menu window creation callback
static void main_menu_window_create(Window * window){
  window_set_background_color(window, getBGColor());
  //create menu icons
  if(listIcon == NULL) listIcon = gbitmap_create_with_resource(RESOURCE_ID_LIST_ICON);
  if(favoriteIcon == NULL) favoriteIcon = gbitmap_create_with_resource(RESOURCE_ID_FAVORITE_ICON);
  if(archiveIcon == NULL) archiveIcon = gbitmap_create_with_resource(RESOURCE_ID_ARCHIVE_ICON);
  if(optionsIcon == NULL) optionsIcon = gbitmap_create_with_resource(RESOURCE_ID_OPTIONS_ICON);
  menuItems[MY_LIST].icon = listIcon;
  menuItems[FAVORITES].icon = favoriteIcon;
  menuItems[ARCHIVE].icon = archiveIcon;
  menuItems[OPTIONS].icon = optionsIcon;
  Layer * windowLayer = window_get_root_layer(window);
  GRect frame = layer_get_frame(windowLayer);
  //create menu
  if(statusBar == NULL) statusBar = getStatusBar();
  if(statusBar != NULL){
    frame.origin.y += STATUS_BAR_LAYER_HEIGHT;
    frame.size.h -= STATUS_BAR_LAYER_HEIGHT;
    layer_add_child(windowLayer, status_bar_layer_get_layer(statusBar));
  }
  if(mainMenu == NULL){
    mainMenu = simple_menu_layer_create(frame,
                                        window,
                                        mainSection,
                                        1,
                                        NULL);
    main_menu_window_appear(window);
    layer_add_child(windowLayer, simple_menu_layer_get_layer(mainMenu));
}}


static void main_menu_window_appear(Window * window){
  window_set_background_color(window, getBGColor());
  menu_layer_set_normal_colors(simple_menu_layer_get_menu_layer(mainMenu),
                                                                     getBGColor(), getTextColor());
  menu_layer_set_highlight_colors(simple_menu_layer_get_menu_layer(mainMenu),
                                                                     getSelectedBGColor(), getSelectedTextColor());
}

//menu window destruction callback
static void main_menu_window_destroy(Window * window){
  if(mainMenu != NULL){
    simple_menu_layer_destroy(mainMenu);
    mainMenu = NULL;
  }
  if(listIcon != NULL){
    gbitmap_destroy(listIcon);
    listIcon = NULL;
  }
  if(favoriteIcon != NULL){
    gbitmap_destroy(favoriteIcon);
    favoriteIcon = NULL;
  }
  
  if(archiveIcon != NULL){
    gbitmap_destroy(archiveIcon);
    archiveIcon = NULL;
  }
  if(optionsIcon != NULL){
    gbitmap_destroy(optionsIcon);
    optionsIcon = NULL;
  }
  if(statusBar != NULL){
    status_bar_layer_destroy(statusBar);
    statusBar = NULL;
  }}

//menu item selection callback
static void main_menu_layer_select(int index, void * context){
  switch((MainMenuIndex)index){
    case MY_LIST:
      setPageState(STATE_UNREAD);
      setFavoriteStatus(FAVE_ALL);
      requestInitialTitles();
    break;
    case FAVORITES:
      setPageState(STATE_ALL);
      setFavoriteStatus(FAVE_TRUE);
      requestInitialTitles();
    break;
    case ARCHIVE:
      setPageState(STATE_ARCHIVE);
      setFavoriteStatus(FAVE_ALL);
      requestInitialTitles();
    break;
    case OPTIONS:
      open_options_menu();
    break;
  }
}