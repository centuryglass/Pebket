#include <pebble.h>
#include "page_menu.h"
#include "message_handler.h"
#include "util.h"
#include "notify.h"
#include "options.h"

//----------LOCAL VALUE DEFINITIONS----------
#define PAGE_MENU_DEBUG_ENABLED//comment out to disable menu debug logs
#ifdef PAGE_MENU_DEBUG_ENABLED
#define PAGE_MENU_DEBUG(fmt, ...) APP_LOG(APP_LOG_LEVEL_DEBUG,fmt,##__VA_ARGS__);
#define PAGE_MENU_ERROR(fmt, ...) APP_LOG(APP_LOG_LEVEL_ERROR,fmt,##__VA_ARGS__);
#else
#define PAGE_MENU_DEBUG(fmt, args...) 
#define PAGE_MENU_ERROR(fmt, args...) 
#endif

#define MAX_NUM_TITLES 20 //maximum number of pages to hold at one time
#define TITLE_LOAD_NUM 10 //number of new pages to request when loading more titles

//----------PAGE MENU DATA----------
char * pageTitles[MAX_NUM_TITLES] = {NULL}; //stores page titles
static char * loadingText = "loading"; //text to display for menu indices with no title
static char * refreshText = "Refresh pages"; //text to display for menu indices with no title
static int firstTitleIndex = 0; //index of first stored title
static int numTitles = 0;//number of currently stored pages
GFont menuFont;
static Window *menu_window;
static StatusBarLayer * statusBar = NULL;
static MenuLayer *titleMenu;
static GTextAttributes * textAttr;

//Indicates if there is an active page request waiting to be resolved
//Used to prevent duplicate requests from piling up
static bool waitingForPages = false;

//Indicates if initial page load has occurred
static bool pagesLoaded = false;

//----------STATIC FUNCTION DECLARATIONS----------
static void handle_window_load(Window* window);
static void handle_window_unload(Window * window);
static void handle_window_disappear(Window * window);
static void requestNextTitles();
static void requestPreviousTitles();
static char * getCellText(MenuIndex *cell_index);
static int getTitleIndex(MenuIndex *cell_index);

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

//----------PUBLIC FUNCTIONS----------
//Load a new page list menu
void init_page_menu(void) {
  PAGE_MENU_DEBUG("init_page_menu:loading menu window");
  //initialize window
  if(menu_window == NULL)menu_window = window_create();
  window_set_window_handlers(menu_window, (WindowHandlers) {
    .load = handle_window_load,
    .unload = handle_window_unload,
    .disappear = handle_window_disappear
  });
  Layer * windowLayer = window_get_root_layer(menu_window);
  GRect frame = layer_get_frame(windowLayer); 
  if(statusBar == NULL) statusBar = getStatusBar();
  if(statusBar != NULL){
    frame.origin.y += STATUS_BAR_LAYER_HEIGHT;
    frame.size.h -= STATUS_BAR_LAYER_HEIGHT;
    layer_add_child(windowLayer, status_bar_layer_get_layer(statusBar));
  }
  //create the menu
  titleMenu = menu_layer_create(frame);
  //load font and text attributes
  menuFont = getTitleFont();
  if(textAttr == NULL)textAttr = graphics_text_attributes_create();
  graphics_text_attributes_enable_screen_text_flow(textAttr,1);
  menu_layer_set_highlight_colors(titleMenu, getSelectedBGColor(), getSelectedTextColor());
  menu_layer_set_normal_colors(titleMenu,getBGColor(), getTextColor());
  layer_add_child(windowLayer, (Layer *)titleMenu);
  //load menu callbacks
  menu_layer_set_callbacks(titleMenu, NULL, (MenuLayerCallbacks){
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
  });
  window_stack_push(menu_window, true);
  if(!pagesLoaded){
    requestInitialTitles();
  }
  PAGE_MENU_DEBUG("init_page_menu: init complete");
}


//unload the current page list menu
void destroy_page_menu(void) {
  PAGE_MENU_DEBUG("destroy_page_menu:destroying current menu");
  window_stack_remove(menu_window, true);
  if(menu_window != NULL){
    window_destroy(menu_window);
    menu_window = NULL;
  }
  PAGE_MENU_DEBUG("destroy_page_menu:destroy complete");
}


//add new page titles to the menu
void update_titles(const char * newTitleString, int firstNewIndex){
  if(firstNewIndex == firstTitleIndex && pagesLoaded){
    PAGE_MENU_DEBUG("update_titles:ignoring duplicate title update");
    hide_notification();
    return;
  }
  PAGE_MENU_DEBUG("update_titles:loading new titles");
  pagesLoaded = true;
  if(menu_window == NULL)init_page_menu();
  PAGE_MENU_DEBUG("update_titles:adding titles, newIndex=%d, oldIndex=%d",
          firstNewIndex,firstTitleIndex);
  //break string into title strings
  char * newTitles[MAX_NUM_TITLES] = {NULL};
  int strIndex = 0;
  int newTitleCount;
  for(newTitleCount = 0; newTitleCount < MAX_NUM_TITLES; newTitleCount++){
    size_t numChars = 0;
    for(size_t i = strIndex; i < strlen(newTitleString); i++){
      if(newTitleString[i] == '\n') break;
      else numChars++;
    }
    if(numChars > 0){
      newTitles[newTitleCount] = malloc_strncpy(newTitles[newTitleCount], 
                                                newTitleString + strIndex, 
                                                numChars);
      PAGE_MENU_DEBUG("update_titles:title %d set to:%s",
              newTitleCount,newTitles[newTitleCount]);
      strIndex += numChars + 1;
    }else break;
  }
  PAGE_MENU_DEBUG("update_titles:found %d new titles",newTitleCount);
  if(newTitleCount > numTitles)numTitles = newTitleCount;
  //get current index
  MenuIndex menuIndex = menu_layer_get_selected_index(titleMenu);
  int titleIndex = getTitleIndex(&menuIndex);
  //check if new titles are before current ones, after, or equal
  if(firstNewIndex == firstTitleIndex){
    if(newTitleCount != MAX_NUM_TITLES){
      PAGE_MENU_ERROR("update_titles: error, initial load found %d titles, expected %d",
              newTitleCount,MAX_NUM_TITLES);
  }}
  //titles are before current ones
  else if(firstNewIndex < firstTitleIndex){ 
    int titleOffset = firstTitleIndex - firstNewIndex;
    if(titleOffset < newTitleCount){
      PAGE_MENU_ERROR(
              "update_titles: error, loading titles %d before title %d, only found %d titles",
              titleOffset, firstTitleIndex, newTitleCount);
    }else if(titleIndex > MAX_NUM_TITLES - titleOffset){
      PAGE_MENU_ERROR(
              "update_titles: error, removing indices after %d when menu index is at %d",
              MAX_NUM_TITLES - titleOffset, titleIndex);
    }else{//translate titles forward by titleOffset
      for(int i = MAX_NUM_TITLES-1; i >=0; i--){
        if(i + titleOffset >= MAX_NUM_TITLES){
          if(pageTitles[i] != NULL){
            free(pageTitles[i]);
            pageTitles[i] = NULL;
        }}
        else{
          pageTitles[i + titleOffset] = pageTitles[i];
          pageTitles[i] = NULL;
    }}}
    if(firstNewIndex == 0) menuIndex.row--;//removing "loading" item from top of menu, move selection
    firstTitleIndex = firstNewIndex;
    menuIndex.row += titleOffset;
  }
  //titles are after current ones
  else if(firstNewIndex > firstTitleIndex){ 
    int titleOffset = newTitleCount;
    if(menuIndex.row < titleOffset){
      PAGE_MENU_ERROR(
              "update_titles: error, removing indices before %d when menu index is at %d",
              titleOffset, menuIndex.row);
    }else{//translate titles backward by titleOffset
      for(int i = 0; i < MAX_NUM_TITLES; i++){
        if(i - titleOffset < 0){
          if(pageTitles[i] != NULL){
            free(pageTitles[i]);
            pageTitles[i] = NULL;
        }}
        else{
            pageTitles[i - titleOffset] = pageTitles[i];
            pageTitles[i] = NULL;
    }}}
    if(firstTitleIndex == 0) menuIndex.row++;//adding "loading" item to top of menu, move selection
     
    firstTitleIndex += newTitleCount;
    menuIndex.row -= titleOffset;
  }
  PAGE_MENU_DEBUG("update_titles:moved old titles");
  //copy over new titles
  int startIndex = firstNewIndex-firstTitleIndex;
  for(int i = startIndex; i < newTitleCount+startIndex && i < MAX_NUM_TITLES; i++){
    if(newTitles[i - startIndex] != NULL){
      char * oldTitle = pageTitles[i];
      pageTitles[i] = newTitles[i - startIndex];
      if(oldTitle != NULL) free(oldTitle);
  }}
  PAGE_MENU_DEBUG("update_titles:copied new titles");
  menu_layer_set_selected_index(titleMenu, menuIndex, MenuRowAlignCenter, false);
  PAGE_MENU_DEBUG("update_titles:re-loading menu data");
  menu_layer_reload_data(titleMenu);
  waitingForPages = false;
  PAGE_MENU_DEBUG("update_titles:Loading new pages complete");
  //close loading notification
  hide_notification();
}

//Requests the first MAX_NUM_TITLES titles from pocket
void requestInitialTitles(){
  PAGE_MENU_DEBUG("requestPreviousTitles:requesting %d titles",MAX_NUM_TITLES);
  if(!waitingForPages){
    get_page_titles(0,MAX_NUM_TITLES);
    waitingForPages = true;
    show_notification("Loading pages...", -1, getBGColor(),
                      (NotifyCallbacks){.onDisappear = hide_notification,
                                        .onSuddenClose = closeList});
  } 
}

//Removes a title from the list
void remove_title(int titleIndex){
  if(titleIndex >= firstTitleIndex &&
    titleIndex  < firstTitleIndex + numTitles){
    int arrayIndex = titleIndex - firstTitleIndex;
    if(pageTitles[arrayIndex] != NULL)
      free(pageTitles[arrayIndex]);
    for(int i = arrayIndex; i < MAX_NUM_TITLES; i++){
      if(i == MAX_NUM_TITLES) pageTitles[i] = NULL;
      else pageTitles[i] = pageTitles[i+1];
    }
    numTitles--;
  }
}

//----------STATIC FUNCTIONS----------
//window load callback
static void handle_window_load(Window* window){
  window_set_background_color(window, getBGColor());
  
  //start menu at first title
  menu_layer_set_selected_index(titleMenu,MenuIndex(0, 1),MenuRowAlignCenter,false);
  menu_layer_set_click_config_onto_window(titleMenu, window);
  
}

static void handle_window_disappear(Window * window){
  if(!window_stack_contains_window(window)){
    PAGE_MENU_DEBUG("handle_window_disappear:window left stack, destroying");
    window_destroy(window);
    menu_window = NULL;
  }
}

//window unload callback
static void handle_window_unload(Window* window) {
  PAGE_MENU_DEBUG("handle_window_unload:destroying window contents");
  if(statusBar != NULL){
    status_bar_layer_destroy(statusBar);
    statusBar = NULL;
  }
  if(titleMenu != NULL){
    menu_layer_destroy(titleMenu);
    titleMenu = NULL;
  }
  for(int i = 0; i < MAX_NUM_TITLES; i++){
    if(pageTitles[i] != NULL){
      free(pageTitles[i]);
      pageTitles[i] = NULL;
  }}
  if(textAttr != NULL){
    graphics_text_attributes_destroy(textAttr);
    textAttr = NULL;
  }
  pagesLoaded = false;
  waitingForPages = false;
  firstTitleIndex = 0;
  numTitles = 0;
}


//Requests the next TITLE_LOAD_NUM titles from pocket
static void requestNextTitles(){
  if(!waitingForPages){
    PAGE_MENU_DEBUG("requestNextTitles:requesting %d titles at %d",
                   TITLE_LOAD_NUM,firstTitleIndex + numTitles);
    get_page_titles(firstTitleIndex + numTitles,TITLE_LOAD_NUM);
    waitingForPages = true;
  } 
}


//Requests the previous TITLE_LOAD_NUM titles from pocket
static void requestPreviousTitles(){
  if(firstTitleIndex != 0 && !waitingForPages){
    PAGE_MENU_DEBUG("requestPreviousTitles:requesting %d titles at %d",
                   TITLE_LOAD_NUM,firstTitleIndex - TITLE_LOAD_NUM);
    int newIndex = firstTitleIndex - TITLE_LOAD_NUM;
    if(newIndex < 0) newIndex = 0;
    get_page_titles(newIndex,TITLE_LOAD_NUM);
    waitingForPages = true;
  }
}





//Given a menu index, get what page title index is selected 
static int getTitleIndex(MenuIndex *cell_index){
  int index = -1;
  if(pagesLoaded){
      index = cell_index->row - 1;
      if(index <0 || index >= MAX_NUM_TITLES) index = -1;
  }
  return index;
}


//Gets the text of the cell at a given index
static char * getCellText(MenuIndex *cell_index){
  //PAGE_MENU_DEBUG("getCellText: begin");
  char * text = NULL;
  int index = getTitleIndex(cell_index);
  //PAGE_MENU_DEBUG("getCellText: index = %d",index);
  if(index != -1) text = pageTitles[index];
  else if(firstTitleIndex == 0 
          && cell_index->row == 0)
    text = refreshText;
  if(text == NULL)text = loadingText;
  //PAGE_MENU_DEBUG("getCellText: text = %s",text);
  return text;
}


//-----MENU LAYER CALLBACKS-----
//gets the number of rows in the menu
static uint16_t getNumRows(struct MenuLayer *menu_layer, uint16_t section_index, void *callback_context) {
  return numTitles + 2;
}


//Gets the height of a menu item
static int16_t getCellHeight(struct MenuLayer *menu_layer, MenuIndex *cell_index, void *callback_context) {  
  char * text = getCellText(cell_index);
  if(text == NULL)text = loadingText;
  GSize text_size = graphics_text_layout_get_content_size_with_attributes
                    (text,
                    menuFont,
                    layer_get_bounds(menu_layer_get_layer(menu_layer)),
                    GTextOverflowModeFill,
                    GTextAlignmentLeft,
                    textAttr);            
  return text_size.h + 5;
}


//Draws menu items
static void drawRow(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *callback_context){
  //PAGE_MENU_DEBUG("drawRow:drawing row %d",cell_index->row);
  GRect bounds = layer_get_bounds(cell_layer);
  char * text = getCellText(cell_index);
  graphics_draw_text(ctx,
                    text,
                    menuFont,
                    bounds,
                    GTextOverflowModeFill,
                    GTextAlignmentLeft,
                    textAttr);
  graphics_draw_rect(ctx,bounds);
    //if reaching the beginning of the local list, request earlier items
  if(cell_index->row == 0 &&
     pagesLoaded &&
    !waitingForPages) 
    requestPreviousTitles();
  //if reaching the end of the list, request later items
  if(cell_index->row >= numTitles &&
     pagesLoaded &&
    !waitingForPages) 
    requestNextTitles();
}


//Callback for the select button
static void selectClick(struct MenuLayer *menu_layer, MenuIndex *cell_index, void *callback_context){
  int pageNum = getTitleIndex(cell_index) + firstTitleIndex;
  if(pageNum >= 0){
    PAGE_MENU_DEBUG("selectClick:requesting page %d",pageNum);
    show_notification("Loading page text...", -1, getBGColor(),
                      (NotifyCallbacks){.onDisappear = hide_notification,
                                        .onSuddenClose = closeText});
    request_page(pageNum);
  }
  //send update request
  else if(cell_index->row == 0 &&
         !waitingForPages &&
         pagesLoaded){
    send_action(ACTION_UPDATE_PAGES);
    show_notification("Refreshing page list...", -1, getBGColor(), 
                      (NotifyCallbacks){.onDisappear = hide_notification,
                                        .onSuddenClose = closeList});
    
  }
}


//Callback for changing menu selection
static void selectionWillChange(struct MenuLayer *menu_layer, MenuIndex *new_index, MenuIndex old_index, void *callback_context){
//currently does nothing
} 