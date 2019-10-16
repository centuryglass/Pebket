#include <pebble.h>
#include <ctype.h>
#include "page_view.h"
#include "message_handler.h"
#include "util.h"
#include "debug.h"
#include "options.h"
#include "page_menu.h"
#include "notify.h"
#include "subpage.h"

//----------LOCAL VALUE DEFINITIONS----------
//#define PAGE_DEBUG_ENABLED//comment out to disable page debug logs
#ifdef PAGE_DEBUG_ENABLED
#define PAGE_DEBUG(fmt, ...) APP_LOG(APP_LOG_LEVEL_DEBUG,fmt,##__VA_ARGS__);
#define PAGE_ERROR(fmt, ...) APP_LOG(APP_LOG_LEVEL_ERROR,fmt,##__VA_ARGS__);
#else
#define PAGE_DEBUG(fmt, args...) 
#define PAGE_ERROR(fmt, args...) 
#endif

#define SCROLL_LAYER_MAX_SIZE 25000
//if scroll layer expands past this size, move back

#define DEFAULT_SCROLL_OFFSET_CHANGE 10000
//a good general amount to offset the scroll layers to
//avoid overly frequent offset changes

typedef enum{
  PAGE_ARCHIVE,
  PAGE_DELETE,
  PAGE_FAVORITE,
  PAGE_BOOKMARK
} PageMenuAction;
//----------PAGE DATA----------
static Window * pageWindow = NULL;//main window
static StatusBarLayer * statusBar = NULL;
static ScrollLayer* scrollLayer = NULL;//main scrolling content

bool changingScrollOffset = false; //if true, don't treat scrolling as usual
Bookmark targetMark = {-1,-1};//If changingScrollOffset, this is set to the target bookmark

int totalSubpageCount = 0;//Total number of subpages available for the current page
PageState currPageState = STATE_UNREAD; //status of the current page
FavoriteStatus currFaveState = FAVE_FALSE;//favorite status of the current page
bool bookmarked = false;//True if a page bookmark has been saved
bool waitingForSubpage = false;//true if a subpage has been requested but hasn't arrived yet

static GPoint lastOffset = {0,0};
//----------ACTION MENU DATA----------
ActionMenu * pageMenu;
ActionMenuConfig menuConfig;
char * menuActionTitles []= {"Archive",
                           "Delete",
                           "Favorite",
                            "Bookmark Page"};

char * altActionTitles []= {"Re-add",
                           "Save",
                           "Unfavorite",
                           "Remove Bookmark"};

ActionMenuItem * actions [4];

//----------STATIC FUNCTION DECLARATIONS----------
//window layer unload callback
static void handle_window_unload(Window* window);
//window layer load callback
static void handle_window_load(Window* window);
//scroll layer update callback
static void scroll_layer_update(struct ScrollLayer *scroll_layer, void *context);
//initializes button callbacks
static void click_config_provider(void * context);
//scroll layer single click callback
static void down_single_click_handler(ClickRecognizerRef recognizer, void *context);
//opens the action menu
static void open_action_menu();
//closes the action menu
static void close_action_menu();
//menu action callback
static void menuAction(ActionMenu *action_menu, const ActionMenuItem *action, void *context);
//Moves all scroll layer content to get around scroll layer size limits
static void offsetScrollLayer(int offset);
//Save the current page and viewing location to the phone
static void bookmarkPage();
//scrolls to a bookmarked page location
static void scroll_to_bookmark(Bookmark dest);
//returns the height of the scroll layer frame
static int getScrollLayerHeight();
//given a scroll offset height, returns the nearest page boundary
int getNearestPageBoundary(int scrollOffset);
//resizes the scroll layer to fit its content
void fit_scrollLayer_to_content();

//----------PUBLIC FUNCTIONS----------
//Loads new page text
void load_page_text(char * pageText,int subpageIndex,int pageSize,
                    int pageState,int faveStatus,int bookmarkOffset){
  memDebug("load_page_text: loading page text");
  if(pageText == NULL || strlen(pageText) == 0){
    PAGE_DEBUG( "load_page_text: received no text");
    waitingForSubpage = false;
    return;
  }
  totalSubpageCount = pageSize;
  currPageState = (PageState) pageState;
  currFaveState = (FavoriteStatus) faveStatus;
  if(pageWindow == NULL)init_page();
  if(!window_stack_contains_window(pageWindow)) 
    window_stack_push(pageWindow, true);
  subpage_set_parent(scrollLayer);
  subpage_init(pageText,subpageIndex);
  waitingForSubpage = false; 
  //if this is the first time the bookmarked subpage has loaded, go to the marked spot
  if(!bookmarked && bookmarkOffset != -1){
    PAGE_DEBUG("load_page_text: loading bookmark at subpage %d, %d percent",subpageIndex,bookmarkOffset);
    bookmarked = true;
    scroll_to_bookmark((Bookmark){subpageIndex, bookmarkOffset});
    //also ask for the nearest adjacent subpage
    waitingForSubpage = true;
    int nearestPage = subpageIndex > 50 ? subpageIndex - 1 : subpageIndex + 1;
    if(nearestPage >= 0 && nearestPage < pageSize){
      PAGE_DEBUG("load_page_text:requesting nearest page, %d",nearestPage);
      get_page_text(nearestPage);
    }
  }
}

/**
*Initializes a new page
*the page window isn't added to the stack until text is provided
*/
void init_page(){
  PAGE_DEBUG("load_page:opening page");
  //Initialize window:
  pageWindow = window_create();  
  window_set_window_handlers(pageWindow, (WindowHandlers) {
    .load = handle_window_load,
    .unload = handle_window_unload,
  });
}


//Removes the currently loaded page
void unload_page(){
  PAGE_DEBUG("unload_page:removing page");
  if(pageWindow != NULL){
    window_stack_remove(pageWindow, true);
    PAGE_DEBUG("unload_page:destroying window at %d",(int) pageWindow);
    window_destroy(pageWindow);
    pageWindow = NULL;
  }
  PAGE_DEBUG("unload_page:page destroyed");
}



//----------STATIC FUNCTIONS----------
/**
*window load callback
*/
static void handle_window_load(Window* window) {
    PAGE_DEBUG("handle_window_load:loading window content");
  window_set_background_color(pageWindow,getBGColor());
  Layer * windowLayer = window_get_root_layer(window);
  GRect frame =  layer_get_frame(windowLayer);
  if(statusBar == NULL) statusBar = getStatusBar();
  if(statusBar != NULL){
    frame.origin.y += STATUS_BAR_LAYER_HEIGHT;
    frame.size.h -= STATUS_BAR_LAYER_HEIGHT;
    layer_add_child(windowLayer, status_bar_layer_get_layer(statusBar));
  }
  //Initialize scroll layer:
  scrollLayer = scroll_layer_create(frame);
  scroll_layer_set_callbacks(scrollLayer, (ScrollLayerCallbacks) {
    .content_offset_changed_handler = scroll_layer_update,
    .click_config_provider = click_config_provider
  });
  layer_add_child(windowLayer,scroll_layer_get_layer(scrollLayer));
  scroll_layer_set_click_config_onto_window(scrollLayer, pageWindow);
  scroll_layer_set_content_size(scrollLayer,GSize(0,SCROLL_LAYER_MAX_SIZE));
  scroll_layer_set_paging(scrollLayer, getPagingEnabled());
  PAGE_DEBUG("handle_window_load:window loaded");
}

/**
*window unload callback
*/
static void handle_window_unload(Window* window) {
  PAGE_DEBUG("handle_window_unload: unload starting, destroying subpages");
  subpage_destroy_all();
  waitingForSubpage = false;
  bookmarked = false;
  totalSubpageCount = 0;
  changingScrollOffset = false;
  if(statusBar != NULL){
    status_bar_layer_destroy(statusBar);
    statusBar = NULL;
  }
  if(scrollLayer != NULL){
    PAGE_DEBUG("handle_window_unload:destroying scroll layer at %d",(int) scrollLayer);
    scroll_layer_destroy(scrollLayer);
    scrollLayer = NULL;
  }
  lastOffset = GPoint(0,0);
}


/**
*page scrolling callback
*/
static void scroll_layer_update(struct ScrollLayer *scroll_layer, void *context){
  Bookmark currentMark = bookmark_from_parent_offset();
  GPoint offset = scroll_layer_get_content_offset(scroll_layer);
  #ifdef PAGE_DEBUG_ENABLED
  PAGE_DEBUG("tTop:%d, offset:%d(pg%d,%d%%), tEnd:%d",
                -get_text_top(),offset.y,currentMark.subpage,currentMark.offsetPercent,-get_text_bottom());
  #endif
  if(!changingScrollOffset && 
     ((offset.y - lastOffset.y) > 200 || (offset.y-lastOffset.y) < -200)){
      PAGE_DEBUG("scroll_layer_update:Offset skipped from %d to %d,resetting to last offset.",
                 lastOffset.y,offset.y);
    scroll_to_bookmark(bookmark_from_offset(lastOffset.y));
    return;
  }
  lastOffset = offset;
  if(changingScrollOffset){//don't make requests when scrolling to a bookmark
    //scrolling is complete if the offset isn't more than 1 percent away from expected
    int distanceFromExpected = (targetMark.subpage * 100 + targetMark.offsetPercent) -
      (currentMark.subpage *100 + currentMark.offsetPercent);
    if(distanceFromExpected < 0) distanceFromExpected *= -1;
    if(targetMark.subpage != -1 &&
       targetMark.offsetPercent != -1 &&
       distanceFromExpected > 2){
      PAGE_DEBUG("offset:%d, expected pg.%d %d%%, found pg.%d %d%% distance=%d",offset.y,targetMark.subpage,
                 targetMark.offsetPercent,currentMark.subpage,currentMark.offsetPercent,distanceFromExpected);
      scroll_to_bookmark(targetMark);
    }else{
      PAGE_DEBUG("scroll_layer_update:reached expected offset")
      hide_notification();
      targetMark = (Bookmark){-1,-1};
      changingScrollOffset = false;
    } 
  }
  if(waitingForSubpage || changingScrollOffset){
    PAGE_DEBUG("scroll_layer_get_content_offset:skipping, wSubpage=%d cOffset=%d",
               waitingForSubpage,changingScrollOffset);
    return;
  }
  int scrollLayerHeight = getScrollLayerHeight();
  int topBounds = scrollLayerHeight*4;
  int bottomBounds = SCROLL_LAYER_MAX_SIZE - scrollLayerHeight*4;
  int offsetShift = 0;
  //if reaching layer bottom edge, translate everything up
  if(-offset.y >= bottomBounds){
    offsetShift = DEFAULT_SCROLL_OFFSET_CHANGE;
    PAGE_DEBUG("scroll_layer_get_content_offset:offset=%d,reaching scroll end, translate up",-offset.y );
  }
  //if reaching offset zero, translate down
  else if(-offset.y <= topBounds &&
         (first_page_index() != 0 || get_text_top() < 0)){
    offsetShift = -DEFAULT_SCROLL_OFFSET_CHANGE;
    PAGE_DEBUG("scroll_layer_get_content_offset:offset=%d,reaching scroll start, translate down",-offset.y );
  }
  topBounds = get_text_top() + scrollLayerHeight*4;
  bottomBounds = get_text_bottom() - scrollLayerHeight*4;
  //if reaching the text start, translate to scrollLayer start
  if(first_page_index() == 0 && get_text_top() != 0 &&
    -offset.y <= topBounds){
    PAGE_DEBUG("scroll_layer_get_content_offset:offset=%d,reaching page start, translate to scroll start",-offset.y );
    offsetShift = get_text_top();
  }
  //if reaching the text end, translate to scrollLayer end
  else if(-offset.y >= bottomBounds && 
    last_page_index() == totalSubpageCount-1){
    offsetShift = get_text_bottom() - SCROLL_LAYER_MAX_SIZE;
    PAGE_DEBUG("scroll_layer_get_content_offset:offset=%d,reaching page end, translate to scroll end",-offset.y );
  }
  if(offsetShift != 0){
    offsetScrollLayer(offsetShift);
    return;
  }
  //if not moving scroll offset, see if new text needs to load
  topBounds = get_text_top() + (2*scrollLayerHeight) -10;
  bottomBounds = get_text_bottom() - (2*scrollLayerHeight) +10;
  //if reaching end, get new text
  if(-offset.y >= bottomBounds && 
    last_page_index()+1 < totalSubpageCount &&
    !waitingForSubpage){
    waitingForSubpage = true;
    get_page_text(last_page_index() + 1);
    PAGE_DEBUG("scroll_layer_update:requesting page %d",last_page_index() + 1);
  }
  //if reaching beginning, get earlier text
  else if(-offset.y <= topBounds &&
         first_page_index() != 0 &&
         !waitingForSubpage){
    waitingForSubpage = true;
    get_page_text(first_page_index() - 1);
    PAGE_DEBUG("scroll_layer_update:requesting page %d",first_page_index() - 1);
  }
}


/**
*move scroll layer contents; allows expanding scroll layer
*past size limits
*offset: amount to move scroll layer offset and all scroll
*layer contents.  NOTE: offset follows the same rules as 
*scroll layer content offsets, where down is negative
*/
static void offsetScrollLayer(int offset){
  
  PAGE_DEBUG("offsetScrollLayer: changing offset by %d",offset);
  //save current page offset
  Bookmark currentMark;
    //if currently moving to a bookmark, save the intended destination 
  if(changingScrollOffset)currentMark = targetMark;
  else{
    currentMark = bookmark_from_parent_offset();
  }
  //move text layers
  subpage_translate_all(-offset);
  //move scroll offset
  scroll_to_bookmark(currentMark);
  
  GPoint soffset = scroll_layer_get_content_offset(scrollLayer);
  PAGE_DEBUG("offsetScrollLayer complete, offset is at %d",soffset.y);
}


//scrolls to a bookmarked page location
static void scroll_to_bookmark(Bookmark dest){
  PAGE_DEBUG("scroll_to_bookmark: scrolling %d percent into subpage %d",
             dest.offsetPercent,dest.subpage);
  GPoint scrollOffset = offset_from_bookmark(dest);
  //if paging is enabled, move bookmark to the nearest page boundary
  if(getPagingEnabled()){
    scrollOffset.y = getNearestPageBoundary(scrollOffset.y);
    dest = bookmark_from_offset(scrollOffset.y);
  }
  targetMark = dest;
  changingScrollOffset = true;
  PAGE_DEBUG("scroll_to_bookmark: moving to %d",scrollOffset.y);
  scroll_layer_set_content_offset(scrollLayer,scrollOffset,false);
}

//Save the current page and viewing location to the phone
static void bookmarkPage(){
  Bookmark currentMark = bookmark_from_parent_offset();
  if(currentMark.subpage != -1){
    PAGE_DEBUG("Bookmarking page at %d percent of subpage %d",currentMark.offsetPercent,currentMark.subpage);
    bookmark_current_page(currentMark.subpage, currentMark.offsetPercent);
    bookmarked = true;
  }   
}


/**
*returns the height of the scroll layer frame
*/
static int getScrollLayerHeight(){
  return layer_get_frame(scroll_layer_get_layer(scrollLayer)).size.h;
}


//given a scroll offset height, returns the nearest page boundary
int getNearestPageBoundary(int scrollOffset){
  PAGE_DEBUG("Finding nearest page boundary to %d",scrollOffset);
  //find nearest page boundary
  int viewHeight = getScrollLayerHeight();
  int pageFraction = scrollOffset % viewHeight;
  if(pageFraction < viewHeight/2) scrollOffset -= pageFraction;
  else scrollOffset += viewHeight - pageFraction;
  while(scrollOffset > 0) scrollOffset -= viewHeight;
  while(scrollOffset < -SCROLL_LAYER_MAX_SIZE+viewHeight)
    scrollOffset += viewHeight;
  
  PAGE_DEBUG("Nearest page boundary is %d",scrollOffset);
  return scrollOffset;  
}

//----------ACTION MENU----------
//actionMenu item selection callback
static void menuAction(ActionMenu *action_menu, const ActionMenuItem *action, void *context){
  if(action == actions[PAGE_ARCHIVE]) send_action(ACTION_ARCHIVE);
  else if(action == actions[PAGE_DELETE]) send_action(ACTION_DELETE);
  else if(action == actions[PAGE_FAVORITE]) send_action(ACTION_FAVORITE);
  else if(action == actions[PAGE_BOOKMARK]){
    if(!bookmarked)bookmarkPage();
    else{
      PAGE_DEBUG("menuAction:deleting bookmark");
      send_action(ACTION_REMOVE_BOOKMARK);
      bookmarked = false;
    }
  }
  close_action_menu();
}

static void menuClosing(ActionMenu *menu, const ActionMenuItem *performed_action, void *context){
  if(performed_action == actions[PAGE_ARCHIVE]){
    unload_page();
    destroy_page_menu();
  }
  else if(performed_action == actions[PAGE_DELETE]){
    unload_page();
    destroy_page_menu();
  }
}
static void menuClosed(ActionMenu *menu, const ActionMenuItem *performed_action, void *context){
}


static void open_action_menu(){
  PAGE_DEBUG("open_action_menu:page state: %d, fave status: %d",currPageState,currFaveState);
  char * firstTitle = (currPageState == STATE_UNREAD) ? menuActionTitles[PAGE_ARCHIVE] : altActionTitles[PAGE_ARCHIVE];
  char * secondTitle = menuActionTitles[PAGE_DELETE];
  char * thirdTitle = (currFaveState == FAVE_FALSE) ? menuActionTitles[PAGE_FAVORITE] : altActionTitles[PAGE_FAVORITE];
  char * fourthTitle = !bookmarked ? menuActionTitles[PAGE_BOOKMARK] : altActionTitles[PAGE_BOOKMARK];
  ActionMenuLevel * mainLevel = action_menu_level_create(4);
  actions[PAGE_ARCHIVE] = action_menu_level_add_action(mainLevel,
                          firstTitle,
                          menuAction,
                          NULL);
  actions[PAGE_DELETE] = action_menu_level_add_action(mainLevel,
                         secondTitle,
                         menuAction,
                         NULL);
  actions[PAGE_FAVORITE] = action_menu_level_add_action(mainLevel,
                           thirdTitle,
                           menuAction,
                           NULL);
  actions[PAGE_BOOKMARK] = action_menu_level_add_action(mainLevel,
                           fourthTitle,
                           menuAction,
                           NULL);
  menuConfig.root_level = mainLevel;
  menuConfig.will_close = menuClosing;
  menuConfig.did_close = menuClosed;
  menuConfig.align = ActionMenuAlignCenter;
  action_menu_set_result_window(pageMenu,pageWindow);
  action_menu_open(&menuConfig);
}

static void close_action_menu(){
  if(pageMenu != NULL)action_menu_close(pageMenu, true);
}
static void click_config_provider(void * context){
  window_single_click_subscribe(BUTTON_ID_SELECT, down_single_click_handler);  
}

static void down_single_click_handler(ClickRecognizerRef recognizer, void *context){
  open_action_menu();
}
