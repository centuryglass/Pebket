#include <pebble.h>
#include "message_handler.h"
#include "page_menu.h"
#include "options.h"
#include "notify.h"
#include "storage_keys.h"
#include "drawing.h"

//----------LOCAL VALUE DEFINITIONS----------
//#define OPTIONS_DEBUG_ENABLED//comment out to disable menu debug logs
#ifdef OPTIONS_DEBUG_ENABLED
#define OPTIONS_DEBUG(fmt, ...) APP_LOG(APP_LOG_LEVEL_DEBUG,fmt,##__VA_ARGS__);
#define OPTIONS_ERROR(fmt, ...) APP_LOG(APP_LOG_LEVEL_ERROR,fmt,##__VA_ARGS__);
#else
#define OPTIONS_DEBUG(fmt, args...) 
#define OPTIONS_ERROR(fmt, args...) 
#endif

#define DEFAULT_CELL_HEIGHT 44
#ifdef PBL_COLOR 
#define NUM_COLORS 64
#else
#define NUM_COLORS 3
#endif
//----------MENU DATA----------
static MenuLayer * optionsMenu = NULL;
static MenuLayerCallbacks menuCallbacks;
static GTextAttributes * textAttr;
static Window * optionsWindow = NULL;
static StatusBarLayer * statusBar = NULL;
//menu list
typedef enum{
  OPTIONS_MENU_MAIN,
  OPTIONS_MENU_SORT,
  OPTIONS_MENU_DISPLAY,
  OPTIONS_MENU_SET_FONTS,
  OPTIONS_MENU_FONT_CHOICES,
  OPTIONS_MENU_SET_COLORS,
  OPTIONS_MENU_COLOR_CHOICES
}OptionMenuType;

//main menu:
typedef enum{
  OPTIONS_SORT_ORDER,
  OPTIONS_DISPLAY_SETTINGS,
  #ifndef PBL_ROUND
  OPTIONS_TOGGLE_PAGING,
  #endif
  OPTIONS_CLEAR_DATA,
}MainOptions;
static char * mainOptionsText [] = {
  "Change sort order",
  "Display settings",
  #ifndef PBL_ROUND
  "Enable paging",
  #endif
  "Clear saved data",
};

//page sort order submenu
//SortOptions: SortType defined in message_handler.h
static char * sortOptionsText[] = {
  "Show newest first",
  "Show oldest first",
  "Sort by title",
  "Sort by URL",
};

//display settings submenu
typedef enum{
  OPTIONS_FONT,
  OPTIONS_COLOR
}DisplayOptions;
static char * displayOptionsText [] = {
  "Font settings",
  "Color settings"
};

//font settings submenu
typedef enum{
  OPTIONS_MENU_FONT,
  OPTIONS_TITLE_FONT,
  OPTIONS_PAGE_FONT
}FontOptions;
static char * fontOptionsText [] = {
  "Set menu font",
  "Set page title font",
  "Set page text font"
};



//color settings submenu
typedef enum{
  OPTIONS_BG_COLOR,
  OPTIONS_TEXT_COLOR,
  OPTIONS_SELECTED_BG_COLOR,
  OPTIONS_SELECTED_TEXT_COLOR
}ColorOptions;
static char * colorOptionsText [] = {
  "Set background color",
  "Set text color",
  "Set selection background color",
  "Set selection text color"
};

//font choice menu
static char * fontChoicesText [] = {
  "Gothic 14",
  "Gothic 14 Bold",
  "Gothic 18",
  "Gothic 18 Bold",
  "Gothic 24",
  "Gothic 24 Bold",
  "Gothic 28",
  "Gothic 28 Bold",
  "Bitham 30 Black",
  "Bitham 42 Bold",
  "Bitham 42 Light",
  "Roboto 21 Condensed",
  "Droid Serif 28 Bold"
};

//font resource keys
static char * fontKeys [] = {
  FONT_KEY_GOTHIC_14,
  FONT_KEY_GOTHIC_14_BOLD,
  FONT_KEY_GOTHIC_18,
  FONT_KEY_GOTHIC_18_BOLD,
  FONT_KEY_GOTHIC_24,
  FONT_KEY_GOTHIC_24_BOLD,
  FONT_KEY_GOTHIC_28,
  FONT_KEY_GOTHIC_28_BOLD,
  FONT_KEY_BITHAM_30_BLACK,
  FONT_KEY_BITHAM_42_BOLD,
  FONT_KEY_BITHAM_42_LIGHT,
  FONT_KEY_ROBOTO_CONDENSED_21,
  FONT_KEY_DROID_SERIF_28_BOLD
};

//font resource key indices
typedef enum{
  GOTH14,
  GOTH14BOLD,
  GOTH18,
  GOTH18BOLD,
  GOTH24,
  GOTH24BOLD,
  GOTH28,
  GOTH28BOLD,
  BIT30BLACK,
  BIT42BOLD,
  BIT42LIGHT,
  ROBOTO21,
  DROID28
} FontIndex;

//selected fonts
static int selectedFonts [] = {ROBOTO21,//menu font
                        GOTH18BOLD,//title font
                        GOTH24BOLD};//text font

static int menuContext = 1;
OptionMenuType currentMenu = OPTIONS_MENU_MAIN;

static bool pagingEnabled = false;
//selected colors
GColor selectedColors []= {
  {GColorWhiteARGB8},
  {GColorBlackARGB8},
  {COLOR_FALLBACK(GColorRedARGB8,GColorLightGrayARGB8)},
  {COLOR_FALLBACK(GColorWhiteARGB8,GColorBlackARGB8)}
};

#ifdef PBL_COLOR
static char * colorNames [] = {
  "Black",
  "Oxford Blue",
  "Duke Blue",
  "Blue",
  "Dark Green",
  "Midnight Green",
  "Cobalt Blue",
  "Blue Moon",
  "Islamic Green",
  "Jaeger Green",
  "Tiffany Blue",
  "Vivid Cerulean",
  "Green",
  "Malachite",
  "Medium Spring Green",
  "Cyan",
  "Bulgarian Rose",
  "Imperial Purple",
  "Indigo",
  "Electric Ultramarine",
  "Army Green",
  "Dark Gray",
  "Liberty",
  "Very Light Blue",
  "Kelly Green",
  "May Green",
  "Cadet Blue",
  "Picton Blue",
  "Bright Green",
  "Screamin' Green",
  "Medium Aquamarine",
  "Electric Blue",
  "Dark Candy Apple Red",
  "Jazzberry Jam",
  "Purple",
  "Vivid Violet",
  "Windsor Tan",
  "Rose Vale",
  "Purpureus",
  "Lavender Indigo",
  "Limerick",
  "Brass",
  "Light Gray",
  "Baby Blue Eyes",
  "Spring Bud",
  "Inchworm",
  "Mint Green",
  "Celeste",
  "Red",
  "Folly",
  "Fashion Magenta",
  "Magenta",
  "Orange",
  "Sunset Orange",
  "Brilliant Rose",
  "Shocking Pink",
  "Chrome Yellow",
  "Rajah",
  "Melon",
  "Rich Brilliant Lavender",
  "Yellow",
  "Icterine",
  "Pastel Yellow",
  "White"
};
#endif
static bool optionsInitialized = false;

//----------STATIC FUNCTION DECLARATIONS----------
static void options_window_create(Window * window);
static void menu_window_appear(Window * window);
static void submenu_window_disappear(Window * window);
static GColor get_color_from_index(int index);
static void open_submenu(OptionMenuType submenu);
static GSize getCellTextSize(int cellIndex, GRect textBounds);
static char * getCellText(int cellIndex);
static bool cellHasImage(int cellIndex);
static void drawCellImage(GContext *ctx, int cellIndex, GRect bounds);
static int getImageSize(int cellIndex);
static int getOptionsIndex(MenuIndex *cell_index);

//-----MENU LAYER CALLBACKS-----
static uint16_t getNumRows
  (struct MenuLayer *menu_layer, uint16_t section_index, void *callback_context);
static int16_t getCellHeight
  (struct MenuLayer *menu_layer, MenuIndex *cell_index, void *callback_context);
static void drawRow
  (GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *callback_context);
static void selectClick
  (struct MenuLayer *menu_layer, MenuIndex *cell_index, void *callback_context);
static void selectionWillChange
  (struct MenuLayer *menu_layer, MenuIndex *new_index, MenuIndex old_index, void *callback_context);
//submenu specific click callback handlers
static void mainMenuSelectClick
  (struct MenuLayer *menu_layer, int optionsIndex, void *callback_context);
static void sortMenuSelectClick
  (struct MenuLayer *menu_layer, int optionsIndex, void *callback_context);
static void displayMenuSelectClick
  (struct MenuLayer *menu_layer, int optionsIndex, void *callback_context);
static void fontMenuSelectClick
  (struct MenuLayer *menu_layer, int optionsIndex, void *callback_context);
static void fontChoiceSelectClick
  (struct MenuLayer *menu_layer, int optionsIndex, void *callback_context);
static void colorMenuSelectClick
  (struct MenuLayer *menu_layer, int optionsIndex, void *callback_context);
static void colorChoiceSelectClick
  (struct MenuLayer *menu_layer, int optionsIndex, void *callback_context);
//----------PUBLIC FUNCTIONS----------
//creates the options menu
void init_options(){
  if(optionsInitialized){
    OPTIONS_DEBUG("init_options:skipping repeat initialization");
    return;
  }
  //load fonts
  if(persist_exists(PERSIST_KEY_MENU_FONT))
    selectedFonts[OPTIONS_MENU_FONT] = persist_read_int(PERSIST_KEY_MENU_FONT);
  if(persist_exists(PERSIST_KEY_TITLE_FONT))
    selectedFonts[OPTIONS_TITLE_FONT] = persist_read_int(PERSIST_KEY_TITLE_FONT);
  if(persist_exists(PERSIST_KEY_PAGE_FONT))
    selectedFonts[OPTIONS_PAGE_FONT] = persist_read_int(PERSIST_KEY_PAGE_FONT);
  if(persist_exists(PERSIST_KEY_BG_COLOR))
    selectedColors[OPTIONS_BG_COLOR].argb =  persist_read_int(PERSIST_KEY_BG_COLOR);
  if(persist_exists(PERSIST_KEY_TEXT_COLOR))
    selectedColors[OPTIONS_TEXT_COLOR].argb = persist_read_int(PERSIST_KEY_TEXT_COLOR);
  if(persist_exists(PERSIST_KEY_BG_SELECTION_COLOR))
    selectedColors[OPTIONS_SELECTED_BG_COLOR].argb = persist_read_int(PERSIST_KEY_BG_SELECTION_COLOR);
  if(persist_exists(PERSIST_KEY_TEXT_SELECTION_COLOR))
    selectedColors[OPTIONS_SELECTED_TEXT_COLOR].argb = persist_read_int(PERSIST_KEY_TEXT_SELECTION_COLOR);
  #ifndef PBL_ROUND
  pagingEnabled = persist_read_bool(PERSIST_KEY_ENABLE_PAGING);
  #endif
  //load text attributes
  if(textAttr == NULL)textAttr = graphics_text_attributes_create();
  graphics_text_attributes_enable_screen_text_flow(textAttr,1);
  optionsInitialized = true;
}

//destroys the main menu
void destroy_options(){
  if(!optionsInitialized) return;
  OPTIONS_DEBUG("unloading options menu");
  if(optionsWindow != NULL){
    window_stack_remove(optionsWindow, true);
    window_destroy(optionsWindow);
    optionsWindow = NULL;
  }
  if(textAttr != NULL){
    graphics_text_attributes_destroy(textAttr);
    textAttr = NULL;
  }
  if(statusBar != NULL){
    status_bar_layer_destroy(statusBar);
    statusBar = NULL;
  }
  
  //save persistent data
  persist_write_int(PERSIST_KEY_MENU_FONT, selectedFonts[OPTIONS_MENU_FONT]);
  persist_write_int(PERSIST_KEY_TITLE_FONT, selectedFonts[OPTIONS_TITLE_FONT]);
  persist_write_int(PERSIST_KEY_PAGE_FONT, selectedFonts[OPTIONS_PAGE_FONT]);
  persist_write_int(PERSIST_KEY_BG_COLOR, selectedColors[OPTIONS_BG_COLOR].argb);
  persist_write_int(PERSIST_KEY_TEXT_COLOR, selectedColors[OPTIONS_TEXT_COLOR].argb);
  persist_write_int(PERSIST_KEY_BG_SELECTION_COLOR, selectedColors[OPTIONS_SELECTED_BG_COLOR].argb);
  persist_write_int(PERSIST_KEY_TEXT_SELECTION_COLOR, selectedColors[OPTIONS_SELECTED_TEXT_COLOR].argb);
  persist_write_bool(PERSIST_KEY_ENABLE_PAGING,pagingEnabled);
  optionsInitialized = false;
}

//shows the options menu
void open_options_menu(){
  if(!optionsInitialized)init_options();
  if(optionsWindow == NULL){
    OPTIONS_DEBUG("loading options menu");
    optionsWindow = window_create();
    window_set_window_handlers(optionsWindow, (WindowHandlers) {
      .load = options_window_create,
      .appear = menu_window_appear,
    });
  }
  window_stack_push(optionsWindow, true);
}

/**
*Returns a status bar layer already set up
*with user selected colors, or NULL if the status
*bar is disabled.  Caller is responsible for destroying
*the status bar on close.
*/
StatusBarLayer * getStatusBar(){
  return status_bar_layer_create();
}

GFont getMenuFont(){
  if(!optionsInitialized)init_options();
  //OPTIONS_DEBUG("Getting menu font, key:%s",menuFontKey);
  return fonts_get_system_font(fontKeys[selectedFonts[OPTIONS_MENU_FONT]]);
}

GFont getTitleFont(){
  if(!optionsInitialized)init_options();
  //OPTIONS_DEBUG("Getting title font, key:%s",titleFontKey);
  return fonts_get_system_font(fontKeys[selectedFonts[OPTIONS_TITLE_FONT]]);}

GFont getPageFont(){
  if(!optionsInitialized)init_options();
  //OPTIONS_DEBUG("Getting page font, key:%s",pageFontKey);
  return fonts_get_system_font(fontKeys[selectedFonts[OPTIONS_PAGE_FONT]]);}


GColor getBGColor(){
  if(!optionsInitialized)init_options();
  return selectedColors[OPTIONS_BG_COLOR];
}

GColor getTextColor(){
  if(!optionsInitialized)init_options();
  return selectedColors[OPTIONS_TEXT_COLOR];
}

GColor getSelectedBGColor(){
  if(!optionsInitialized)init_options();
  return selectedColors[OPTIONS_SELECTED_BG_COLOR];
}

GColor getSelectedTextColor(){
  if(!optionsInitialized)init_options();
  return selectedColors[OPTIONS_SELECTED_TEXT_COLOR];
}

bool getPagingEnabled(){
  if(!optionsInitialized)init_options();
  #ifdef PBL_ROUND
  pagingEnabled = true;
  #endif
  return pagingEnabled;
}


//----------STATIC FUNCTIONS----------

//opens a submenu of type (submenu)
static void open_submenu(OptionMenuType submenuType){
  Window * submenuWindow = window_create();
  window_set_window_handlers(submenuWindow, (WindowHandlers) {
    .load = menu_window_appear,
    .appear = menu_window_appear,
    .disappear = submenu_window_disappear
  });
  MenuIndex newIndex;
  if(submenuType == OPTIONS_MENU_COLOR_CHOICES)
    newIndex = (MenuIndex){0,NUM_COLORS};
  else newIndex = (MenuIndex){0,1};
  currentMenu = submenuType;
  menu_layer_set_selected_index(optionsMenu,newIndex,MenuRowAlignCenter,false);
  menu_layer_reload_data(optionsMenu);
  window_stack_push(submenuWindow, true);
}

/**
*Given a raw menu index, return the correct index to
*use for other menu operations.  Also ensures all indices
*are valid, and complains if it has to fix them
*/
static int getOptionsIndex(MenuIndex *cell_index){
  int actualNumRows;
  int index;
  if(currentMenu == OPTIONS_MENU_COLOR_CHOICES){
    actualNumRows = NUM_COLORS;
    index = cell_index->row % NUM_COLORS;
  }else{
    actualNumRows = getNumRows(optionsMenu,0,NULL) - 2;
    index = cell_index->row -1;
  }
  if(index >= actualNumRows){
    OPTIONS_ERROR("getOptionsIndex: index:%d, but there's only %d rows",index,actualNumRows);
    index %= actualNumRows;
    OPTIONS_DEBUG("getOptionsIndex: Corrected index to %d for now, fix that already",index);
  }
  return index;
}

//menu window creation callback
static void options_window_create(Window * window){
  window_set_background_color(window, getBGColor());
  //create menu
  if(optionsMenu == NULL){
    Layer * windowLayer = window_get_root_layer(window);
    GRect frame = layer_get_frame(windowLayer);
    if(statusBar == NULL) statusBar = getStatusBar();
    if(statusBar != NULL){
      frame.origin.y += STATUS_BAR_LAYER_HEIGHT;
      frame.size.h -= STATUS_BAR_LAYER_HEIGHT;
      layer_add_child(windowLayer, status_bar_layer_get_layer(statusBar));
    }
    optionsMenu = menu_layer_create(frame);
    menu_layer_set_highlight_colors(optionsMenu,getSelectedBGColor(),getSelectedTextColor());
    menu_layer_set_normal_colors(optionsMenu, getBGColor(), getTextColor());
    layer_add_child(windowLayer, menu_layer_get_layer(optionsMenu));
    //load menu callbacks
    menuCallbacks = (MenuLayerCallbacks){
      .get_num_sections = NULL,
      .get_num_rows =  getNumRows,
      .get_cell_height = getCellHeight,
      .get_header_height = NULL,
      .draw_row = drawRow,
      .draw_header = NULL,
      .select_click = selectClick,
      .select_long_click = selectClick,
      .selection_changed = NULL,
      .selection_will_change = selectionWillChange,
      .get_separator_height = NULL
    };
    menu_layer_set_callbacks(optionsMenu, &menuContext, menuCallbacks);
    MenuIndex startIndex = {0,1};
    menu_layer_set_selected_index(optionsMenu,startIndex,MenuRowAlignCenter,false);
    menu_layer_set_click_config_onto_window(optionsMenu, window);
}}

//submenu window creation callback
static void menu_window_appear(Window * window){
  window_set_background_color(window, getBGColor());
  Layer * windowLayer = window_get_root_layer(window);
  layer_add_child(windowLayer, menu_layer_get_layer(optionsMenu));
  if(statusBar != NULL)
    layer_add_child(windowLayer, status_bar_layer_get_layer(statusBar));
  menu_layer_set_click_config_onto_window(optionsMenu, window);
  menu_layer_reload_data(optionsMenu);
}

//submenu window disappear callback
static void submenu_window_disappear(Window * window){
  if(!window_stack_contains_window(window)){
    window_destroy(window);
    OptionMenuType parentMenu;
    switch(currentMenu){
      case OPTIONS_MENU_MAIN:
        OPTIONS_ERROR("submenu_window_destroy called by main options menu!");
        parentMenu = OPTIONS_MENU_MAIN;
        break;
      case OPTIONS_MENU_SORT:
        parentMenu = OPTIONS_MENU_MAIN;
        break;
      case OPTIONS_MENU_DISPLAY:
        parentMenu = OPTIONS_MENU_MAIN;
        break;
      case OPTIONS_MENU_SET_FONTS:
        parentMenu = OPTIONS_MENU_DISPLAY;
        break;
      case OPTIONS_MENU_FONT_CHOICES:
        parentMenu = OPTIONS_MENU_SET_FONTS;
        break;
      case OPTIONS_MENU_SET_COLORS:
        parentMenu = OPTIONS_MENU_DISPLAY;
        break;
      case OPTIONS_MENU_COLOR_CHOICES:
        parentMenu = OPTIONS_MENU_SET_COLORS;
        break;
      default:
        OPTIONS_ERROR("submenu_window_destroy:current menu type is invalid");
        parentMenu = OPTIONS_MENU_MAIN;  
    }
    currentMenu = parentMenu;
    MenuIndex newIndex = {0,1};
    menu_layer_set_selected_index(optionsMenu,newIndex,MenuRowAlignCenter,false);
    menu_layer_reload_data(optionsMenu);
  }
}

static GColor get_color_from_index(int index){
  index %= NUM_COLORS;
  //OPTIONS_ERROR("get_color_from_index:color index is %d",index);
  #ifdef PBL_COLOR
  int b = (index % 4) * 85;
  index /= 4;
  int g = (index % 4) * 85;
  index /= 4;
  int r = index * 85;
  return GColorFromRGB(r,g,b);
  #else
  switch(index){
    case 0:
      return GColorWhite;
    case 1:
      return GColorLightGray;
    case 2:
      return GColorBlack;
    default:
      OPTIONS_ERROR("get_color_from_index:invalid index %d",index);
      return GColorBlack;
  }
  #endif
}

//-----MENU LAYER CALLBACKS-----
static bool isLastRow(int rowNum){
  return rowNum >= getNumRows(optionsMenu,0,NULL)-1;
}
//gets the number of rows in the menu
static uint16_t getNumRows(struct MenuLayer *menu_layer, uint16_t section_index, void *callback_context) {
  int numRows = 0;
  switch(currentMenu){
    case OPTIONS_MENU_MAIN:
      numRows = sizeof(mainOptionsText)/sizeof(char *);
      break;
    case OPTIONS_MENU_SORT:
      numRows = sizeof(sortOptionsText)/sizeof(char *);
      break;
    case OPTIONS_MENU_DISPLAY:
      numRows = sizeof(displayOptionsText)/sizeof(char *);
      break;
    case OPTIONS_MENU_SET_FONTS:
      numRows = sizeof(fontOptionsText)/sizeof(char *);
      break;
    case OPTIONS_MENU_FONT_CHOICES:
      numRows = sizeof(fontChoicesText)/sizeof(char *);
      break;
    case OPTIONS_MENU_SET_COLORS:
      numRows = sizeof(colorOptionsText)/sizeof(char *);
      break;
    case OPTIONS_MENU_COLOR_CHOICES:
      numRows = NUM_COLORS * 3;
      break;
  }
  numRows = numRows + 2;
  return numRows;
}

static GSize getCellTextSize(int cellIndex, GRect textBounds){
  GSize textSize = GSize(0,0);
  char * cellText = getCellText(cellIndex);
  if(cellText != NULL){
    GFont rowFont;
    if(currentMenu == OPTIONS_MENU_FONT_CHOICES)
      rowFont = fonts_get_system_font(fontKeys[cellIndex]);
    else rowFont = getMenuFont();
    //if there's an image, make sure there's room for it
    if(cellHasImage(cellIndex)){
      int iconSize = getImageSize(cellIndex);
      textBounds.size.w -= iconSize + 2;
    }
    textSize = graphics_text_layout_get_content_size_with_attributes(
                    getCellText(cellIndex), 
                    rowFont, 
                    textBounds, 
                    GTextOverflowModeWordWrap,
                    GTextAlignmentLeft,
                    textAttr);
  }
  return textSize;
}

static int getImageSize(int cellIndex){
  if(!cellHasImage(cellIndex))return 0;
  //the icon should be big enough to contain the letter 'a'
  //in whatever the active font is
  GFont cellFont = getMenuFont();
  if(currentMenu == OPTIONS_MENU_SET_FONTS)
    cellFont = fonts_get_system_font(fontKeys[selectedFonts[cellIndex]]);
  GSize textSize = graphics_text_layout_get_content_size_with_attributes(
                    "A", 
                    cellFont, 
                    GRect(0,0,100,100), 
                    GTextOverflowModeWordWrap,
                    GTextAlignmentLeft,
                    textAttr);
  int size = textSize.h > textSize.w ? textSize.h : textSize.w;
  size += 5;
  return size;
}

//Gets the height of a menu item
static int16_t getCellHeight(struct MenuLayer *menu_layer, MenuIndex *cell_index, void *callback_context) {
  if(isLastRow(cell_index->row) || cell_index->row == 0)return 0;
  GRect bounds = layer_get_bounds(menu_layer_get_layer(menu_layer));
  GSize textSize = getCellTextSize(getOptionsIndex(cell_index), bounds);
  int size = textSize.h;
  //set the default size as the minimum size
  if(size < DEFAULT_CELL_HEIGHT) size = DEFAULT_CELL_HEIGHT;
  //make sure size is smaller than the bounds
  else if(size >= bounds.size.h) size = bounds.size.h - 2;
  //otherwise just add some padding to the text size and use that
  else size += 5;
  return size;
}

static char * getCellText(int cellIndex){
  switch(currentMenu){
    case OPTIONS_MENU_MAIN:
      return mainOptionsText[cellIndex];
    case OPTIONS_MENU_SORT:
      return sortOptionsText[cellIndex];
    case OPTIONS_MENU_DISPLAY:
      return displayOptionsText[cellIndex];
    case OPTIONS_MENU_SET_FONTS:
      return fontOptionsText[cellIndex];
    case OPTIONS_MENU_FONT_CHOICES:
      return fontChoicesText[cellIndex];
    case OPTIONS_MENU_SET_COLORS:
      return colorOptionsText[cellIndex];
    case OPTIONS_MENU_COLOR_CHOICES:
      #ifdef PBL_COLOR
      return colorNames[cellIndex];
      #else
      return NULL;
      #endif
  }
  return NULL;
}

static bool cellHasImage(int cellIndex){
  return
    #ifndef PBL_ROUND
    (currentMenu == OPTIONS_MENU_MAIN && cellIndex == OPTIONS_TOGGLE_PAGING) ||
    #endif
    currentMenu == OPTIONS_MENU_SET_COLORS ||
    currentMenu == OPTIONS_MENU_SET_FONTS;
}

//draws the cell image, if one exists
static void drawCellImage(GContext *ctx, int cellIndex, GRect bounds){
  #ifndef PBL_ROUND
  if (currentMenu == OPTIONS_MENU_MAIN && cellIndex == OPTIONS_TOGGLE_PAGING){
    draw_checkbox(ctx, bounds, pagingEnabled);
  }
  #endif
  if(currentMenu == OPTIONS_MENU_SET_COLORS){
    GColor imgColor = selectedColors[cellIndex];
    draw_color_preview(ctx, bounds, imgColor);
  }
  if(currentMenu == OPTIONS_MENU_SET_FONTS){
    GFont imgFont = fonts_get_system_font(fontKeys[selectedFonts[cellIndex]]);
    draw_font_preview(ctx, bounds, imgFont);
  }
}


//Draws menu items
static void drawRow(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *callback_context){
  if(isLastRow(cell_index->row) || cell_index->row == 0)return;
  int optionsIndex = getOptionsIndex(cell_index);
  bool cell_is_selected = 
    menu_layer_get_selected_index(optionsMenu).row == cell_index->row;
  GTextAlignment drawnTextAlign = GTextAlignmentLeft;
  GRect bounds = layer_get_bounds(cell_layer);
  //draw row border
  if(currentMenu == OPTIONS_MENU_COLOR_CHOICES){
    drawnTextAlign = GTextAlignmentCenter;
    GColor cellColor = get_color_from_index(optionsIndex);
    graphics_context_set_fill_color(ctx,cellColor);
    graphics_fill_rect(ctx,bounds,5,GCornersAll);
    graphics_context_set_text_color(ctx,gcolor_legible_over(cellColor));
    if(cell_is_selected){
        graphics_context_set_stroke_color(ctx,gcolor_legible_over(cellColor));
        graphics_context_set_stroke_width(ctx,6);
        graphics_draw_rect(ctx,bounds);
    }
  }else graphics_draw_rect(ctx,bounds);
  char * text = getCellText(optionsIndex);
  GFont rowFont = getMenuFont();
  if(currentMenu == OPTIONS_MENU_FONT_CHOICES)
    rowFont = fonts_get_system_font(fontKeys[optionsIndex]);
  //draw cell image
  if(cellHasImage(optionsIndex)){
    GRect imageBounds = bounds;
    if(text != NULL){
      int imageSize = getImageSize(optionsIndex);
      imageBounds.size.h = imageSize;
      imageBounds.size.w = imageSize;
      imageBounds.origin.x += bounds.size.w - imageSize;
      imageBounds.origin.y += (bounds.size.h-imageSize)/2;
      drawCellImage(ctx,optionsIndex,imageBounds);
      //resize text bounds so they don't overlap the image
      bounds.size.w -= imageSize + 2;
    }
  }
  //center text vertically
  GSize textSize = getCellTextSize(optionsIndex, bounds);
  if(bounds.size.h > textSize.h)
    bounds.origin.y += (bounds.size.h - textSize.h)/2;
  //draw text
  if(text != NULL){
    graphics_draw_text(ctx,
                    text,
                    rowFont,
                    bounds,
                    GTextOverflowModeWordWrap,
                    drawnTextAlign,
                    textAttr);
  }
}


//Callback for the select button: calls the callback for the current submenu
static void selectClick(struct MenuLayer *menu_layer, MenuIndex *cell_index, void *callback_context){
  int optionsIndex = getOptionsIndex(cell_index);
  switch(currentMenu){
    case OPTIONS_MENU_MAIN:
      mainMenuSelectClick(menu_layer,optionsIndex,callback_context);
      break;
    case OPTIONS_MENU_SORT:
      sortMenuSelectClick(menu_layer,optionsIndex,callback_context);
      break;
    case OPTIONS_MENU_DISPLAY:
      displayMenuSelectClick(menu_layer,optionsIndex,callback_context);
      break;
    case OPTIONS_MENU_SET_FONTS:
      fontMenuSelectClick(menu_layer,optionsIndex,callback_context);
      break;
    case OPTIONS_MENU_FONT_CHOICES:
      fontChoiceSelectClick(menu_layer,optionsIndex,callback_context);
      break;
    case OPTIONS_MENU_SET_COLORS:
      colorMenuSelectClick(menu_layer,optionsIndex,callback_context);
      break;
    case OPTIONS_MENU_COLOR_CHOICES:
      colorChoiceSelectClick(menu_layer,optionsIndex,callback_context);
      break;
  }
}

//Select click callback for the main menu
static void mainMenuSelectClick
  (struct MenuLayer *menu_layer, int optionsIndex, void *callback_context){
  switch((MainOptions) optionsIndex){
    case OPTIONS_SORT_ORDER:
      open_submenu(OPTIONS_MENU_SORT);
      break;
    case OPTIONS_CLEAR_DATA:
      OPTIONS_DEBUG("sending ACTION_CLEAR_STORAGE request");
      send_action(ACTION_CLEAR_STORAGE);
      break;
    case OPTIONS_DISPLAY_SETTINGS:
      open_submenu(OPTIONS_MENU_DISPLAY);
      break;
    #ifndef PBL_ROUND
    case OPTIONS_TOGGLE_PAGING:
      if(pagingEnabled){
        pagingEnabled = false;
      }else{
        pagingEnabled = true;
      }
      menu_layer_reload_data(optionsMenu);
    break;
    #endif
  }
}

//Select click callback for the page sort menu
static void sortMenuSelectClick
  (struct MenuLayer *menu_layer, int optionsIndex, void *callback_context){
  setSortType((SortType)optionsIndex);
  char notifyText[32];
  switch((SortType)optionsIndex){
    case SORT_NEWEST:
      strcpy(notifyText,"Showing newest pages first.");
      break;
    case SORT_OLDEST:
      strcpy(notifyText,"Showing oldest pages first.");
      break;
    case SORT_URL:
      strcpy(notifyText,"Sorting pages by address.");
      break;
    case SORT_TITLE:
      strcpy(notifyText,"Sorting pages by title.");
      break;
  }
  show_notification(notifyText, 1, GColorWhite, 
                    (NotifyCallbacks){.onDisappear = hide_notification}); 
}

//Select click callback for the display options menu
static void displayMenuSelectClick
  (struct MenuLayer *menu_layer, int optionsIndex, void *callback_context){
  switch((DisplayOptions)optionsIndex){
    case OPTIONS_FONT:
      open_submenu(OPTIONS_MENU_SET_FONTS);
      break;
    case OPTIONS_COLOR:
      open_submenu(OPTIONS_MENU_SET_COLORS);
      break;
  }
}

//Select click callback for the display font menu
static void fontMenuSelectClick
  (struct MenuLayer *menu_layer, int optionsIndex, void *callback_context){
  *(int *) callback_context = optionsIndex;
  open_submenu(OPTIONS_MENU_FONT_CHOICES);
}

//Select click callback for the font choice menu
static void fontChoiceSelectClick
  (struct MenuLayer *menu_layer, int optionsIndex, void *callback_context){
  selectedFonts[*(int *)callback_context] = optionsIndex;
  OPTIONS_DEBUG("fontChoiceSelectClick context:fontkey is %s",fontKeys[selectedFonts[*(int *)callback_context]]);
  window_stack_pop(true);
}

//Select click callback for the display color menu
static void colorMenuSelectClick
  (struct MenuLayer *menu_layer, int optionsIndex, void *callback_context){
  *(int *) callback_context = optionsIndex;
  open_submenu(OPTIONS_MENU_COLOR_CHOICES);
}

//Select click callback for the color choice menu
static void colorChoiceSelectClick
  (struct MenuLayer *menu_layer, int optionsIndex, void *callback_context){
  GColor chosenColor = get_color_from_index(optionsIndex);
  selectedColors[*(int *)callback_context] = chosenColor;
  //having the same text and background color is not allowed
  if(gcolor_equal(getSelectedBGColor(),getSelectedTextColor()))
     selectedColors[OPTIONS_SELECTED_TEXT_COLOR] = gcolor_legible_over(getSelectedBGColor());
  if(gcolor_equal(getBGColor(),getTextColor()))
     selectedColors[OPTIONS_TEXT_COLOR] = gcolor_legible_over(getBGColor());
  //apply colors to menu
  menu_layer_set_highlight_colors(optionsMenu, getSelectedBGColor(), getSelectedTextColor());
  menu_layer_set_normal_colors(optionsMenu, getBGColor(), getTextColor());
  window_stack_pop(true);
}


//Callback for changing menu selection
static void selectionWillChange(struct MenuLayer *menu_layer, MenuIndex *new_index, MenuIndex old_index, void *callback_context){
  int numRows = getNumRows(menu_layer,0,NULL);
  //if it's the color menu, use looping to create the illusion of an infinite loop
  if(currentMenu == OPTIONS_MENU_COLOR_CHOICES){
    if(new_index->row < NUM_COLORS)
      new_index->row = NUM_COLORS * 2 - 1;
    else if(new_index->row >= NUM_COLORS*2)
      new_index->row = NUM_COLORS;
  }
  else{
    //otherwise just loop from end to start, and from start to end
    if(new_index->row == numRows-1)
      new_index->row = 1;
    else if(new_index->row == 0)
      new_index->row = numRows-2;
  }
  OPTIONS_DEBUG("old index:%d, new index:%d", old_index.row,new_index->row);
} 



