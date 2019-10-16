#include <pebble.h>
#include "message_handler.h"
#include "messaging_core.h"
#include "util.h"
#include "page_menu.h"
#include "page_view.h"
#include "storage_keys.h"
#include "notify.h"

//----------LOCAL VALUE DEFINITIONS----------
//#define MSG_DEBUG_ENABLED//comment out to disable menu debug logs
#ifdef MSG_DEBUG_ENABLED
#define MSG_DEBUG(fmt, ...) APP_LOG(APP_LOG_LEVEL_DEBUG,fmt,##__VA_ARGS__);
#define MSG_ERROR(fmt, ...) APP_LOG(APP_LOG_LEVEL_ERROR,fmt,##__VA_ARGS__);
#else
#define MSG_DEBUG(fmt, args...) 
#define MSG_ERROR(fmt, args...) 
#endif

//----------APPMESSAGE KEY DEFINITIONS----------
enum{
  KEY_MESSAGE_CODE,
    //int8: identifying the purpose of the message, bi-directional
  KEY_MESSAGE_TEXT,
    //cstring: contains the main text associated with the message(if relevant)
  KEY_NUM_ITEMS,
    //int16: specifies a quantity
  KEY_INDEX,
    //int16: specifies an item index
  KEY_PAGE_STATE,
  KEY_FAVORITE,
  KEY_TAG,
  KEY_CONTENT_TYPE,
  KEY_SORT_ORDER,
  KEY_OPCODE,
  KEY_SCROLL_OFFSET
};

//----------APPMESSAGE MESSAGE CODES----------
//Valid message codes for messages sent from Pebble
typedef enum{
  CODE_PAGE_TITLE_REQUEST,
  CODE_LOAD_PAGE_REQUEST,
  CODE_PAGE_TEXT_REQUEST,
  CODE_ARCHIVE_PAGE,
  CODE_DELETE_PAGE,
  CODE_FAVORITE_PAGE,
  CODE_CLEAR_LOCAL_STORAGE,
  CODE_UPDATE_TITLES,
  CODE_BOOKMARK_PAGE,
  CODE_REMOVE_BOOKMARK
} PebbleMessageCode;

//Valid message codes for messages received from JavaScript
typedef enum{
  CODE_PAGE_TITLE_RESPONSE,
    //Message providing page titles
  CODE_PAGE_TEXT_RESPONSE,
    //Message providing page text
  CODE_INIT_SIGNAL,
  CODE_RESET_PAGE_DATA,
  CODE_OP_SUCCEEDED,
  CODE_OP_FAILED
} JSMessageCode;

//Types of operations javascript may report
typedef enum{
  OP_LOGIN,
  OP_LOAD_PAGES,
  OP_LOAD_TEXT,
  OP_TOGGLE_FAVE,
  OP_TOGGLE_ARCHIVE,
  OP_DELETE_PAGE
}OpCode;

//----------LOCAL VARIABLES----------
PageState pageState = STATE_ALL;
FavoriteStatus favoriteStatus = FAVE_ALL;
SortType sortType = SORT_NEWEST;

static void process_message(DictionaryIterator *iterator);
//----------PUBLIC FUNCTIONS----------
//Initializes AppMessage functionality
void message_handler_init(){
  open_messaging();
  register_inbox_handler(process_message);
  toggle_message_sending(false);
}

//Shuts down AppMessage functionality
void messaging_deinit(){
    close_messaging();
}

//sets the page state to request when getting pages
void setPageState(PageState newState){pageState = newState;}

//sets the favorite status to request when getting pages
void setFavoriteStatus(FavoriteStatus newFavStatus){favoriteStatus = newFavStatus;}

//sets the page sort order to request when getting pages
void setSortType(SortType newSortType){sortType = newSortType;}


/**
*Requests updated page titles
*@param updateType the appropriate update request code
*/
void get_page_titles(int firstTitleIndex, int numTitles){
  MSG_DEBUG("get_page_titles:Requesting %d titles at %d",numTitles,firstTitleIndex);
  uint8_t buf[PEBBLE_DICT_SIZE] = {0};//default buffer values to 0 to avoid 
    //junk data overwriting legitimate keys
  DictionaryIterator iter;
  dict_write_begin(&iter,buf,PEBBLE_DICT_SIZE);
  dict_write_int8(&iter, KEY_MESSAGE_CODE, CODE_PAGE_TITLE_REQUEST);
  dict_write_int16(&iter, KEY_INDEX, firstTitleIndex);
  dict_write_int16(&iter, KEY_NUM_ITEMS, numTitles);
  dict_write_int8(&iter, KEY_PAGE_STATE, pageState);
  dict_write_int8(&iter, KEY_FAVORITE, favoriteStatus);
  dict_write_int8(&iter, KEY_SORT_ORDER, sortType);
  dict_write_end(&iter);
  MSG_DEBUG("get_page_titles:Attempting to send request");
  add_message(buf);
}

/**
*Loads the text of a saved page
*pageIndex: the page to load
*/
void request_page(int pageIndex){
  uint8_t buf[PEBBLE_DICT_SIZE] = {0};//default buffer values to 0 to avoid 
    //junk data overwriting legitimate keys
  DictionaryIterator iter;
  dict_write_begin(&iter,buf,PEBBLE_DICT_SIZE);
  dict_write_int8(&iter, KEY_MESSAGE_CODE, CODE_LOAD_PAGE_REQUEST);
  dict_write_int16(&iter, KEY_INDEX, pageIndex);
  dict_write_end(&iter);
  MSG_DEBUG("request_page:Attempting to send request");
  add_message(buf);
}

/**
*gets text from the loaded page
*numChars: number of characters to load
*offset: first character index
*/
void get_page_text(int subPage){
  uint8_t buf[PEBBLE_DICT_SIZE] = {0};//default buffer values to 0 to avoid 
    //junk data overwriting legitimate keys
  DictionaryIterator iter;
  dict_write_begin(&iter,buf,PEBBLE_DICT_SIZE);
  dict_write_int8(&iter, KEY_MESSAGE_CODE, CODE_PAGE_TEXT_REQUEST);
  dict_write_int16(&iter, KEY_INDEX, subPage);
  dict_write_end(&iter);
  MSG_DEBUG("get_page_titles:Attempting to send request");
  add_message(buf); 
}

/**
*Sends a simple command to javascript
*action: the action to perform
*/
void send_action(JSAction action){
  PebbleMessageCode actionCode;
  switch(action){
    case ACTION_ARCHIVE:
      actionCode = CODE_ARCHIVE_PAGE;
      break;
    case ACTION_DELETE:
      actionCode = CODE_DELETE_PAGE;
      break;
    case ACTION_FAVORITE:
      actionCode = CODE_FAVORITE_PAGE;
      break;
    case ACTION_CLEAR_STORAGE:
      actionCode = CODE_CLEAR_LOCAL_STORAGE;
      break;
    case ACTION_UPDATE_PAGES:
      actionCode = CODE_UPDATE_TITLES;
      break;
    case ACTION_REMOVE_BOOKMARK:
      actionCode = CODE_REMOVE_BOOKMARK;
      break;
    default:
      MSG_ERROR("send_page_action: Invalid action code %d",action);
      return;
  }
  uint8_t buf[PEBBLE_DICT_SIZE] = {0};//default buffer values to 0 to avoid 
  DictionaryIterator iter;
  dict_write_begin(&iter,buf,PEBBLE_DICT_SIZE);
  dict_write_int8(&iter, KEY_MESSAGE_CODE, actionCode);
  dict_write_end(&iter);
  add_message(buf);
}

/**
*Saves the currently loaded page to the phone
*subpage: currently viewed subpage
*scrollOffset: amount scrolled past the current subpage's start
*/
void bookmark_current_page(int subPage, int scrollOffset){
  uint8_t buf[PEBBLE_DICT_SIZE] = {0};//default buffer values to 0 to avoid 
    //junk data overwriting legitimate keys
  DictionaryIterator iter;
  dict_write_begin(&iter,buf,PEBBLE_DICT_SIZE);
  dict_write_int8(&iter, KEY_MESSAGE_CODE, CODE_BOOKMARK_PAGE);
  dict_write_int16(&iter, KEY_INDEX, subPage);
  dict_write_int16(&iter, KEY_SCROLL_OFFSET, scrollOffset);
  dict_write_end(&iter);
  MSG_DEBUG("get_page_titles:Attempting to send request");
  add_message(buf); 
  
}


//----------STATIC FUNCTIONS----------

static void process_message(DictionaryIterator *iterator){
  Tuple *message_code = dict_find(iterator,KEY_MESSAGE_CODE);
  Tuple *message_text = dict_find(iterator,KEY_MESSAGE_TEXT);
  Tuple *item_count = dict_find(iterator,KEY_NUM_ITEMS);
  Tuple *index = dict_find(iterator,KEY_INDEX);
  Tuple *pageState = dict_find(iterator,KEY_PAGE_STATE);
  Tuple *faveStatus = dict_find(iterator,KEY_FAVORITE);
  Tuple *scrollOffset = dict_find(iterator,KEY_SCROLL_OFFSET);
  Tuple * op = dict_find(iterator,KEY_OPCODE);
  if(message_code != NULL){
    switch((JSMessageCode) message_code->value->int32){
      case CODE_PAGE_TITLE_RESPONSE:{
        MSG_DEBUG("inbox_received_callback:Recieved CODE_PAGE_TITLE_RESPONSE");
        if((message_text != NULL)&&(item_count != NULL)&&(index != NULL)){
          update_titles(message_text->value->cstring, index->value->int16);
          MSG_DEBUG("Received new title list at index %d",index->value->int16);
          MSG_DEBUG("new titles:%s",message_text->value->cstring);
        }}
        break;
      case CODE_PAGE_TEXT_RESPONSE:{
        MSG_DEBUG("inbox_received_callback:Recieved CODE_PAGE_TEXT_RESPONSE");
        char * pageText = NULL;
        int subpageIndex = 0;
        int numSubpages = 1;
        int state = 0;
        int fave = 0;
        int bookmarkOffset = -1;
        if(message_text != NULL) pageText = message_text->value->cstring;
        if(index != NULL)subpageIndex = index->value->int16;
        if(item_count != NULL) numSubpages = item_count->value->int16;
        if(pageState != NULL) state = pageState->value->int16;
        if(faveStatus != NULL)fave = faveStatus->value->int16;
        if(scrollOffset != NULL)bookmarkOffset = scrollOffset->value->int16;
        load_page_text(pageText,subpageIndex,numSubpages,state,fave,bookmarkOffset);
        }
        break;
      case CODE_INIT_SIGNAL:{
        MSG_DEBUG("inbox_received_callback:Recieved CODE_INIT_SIGNAL");
        toggle_message_sending(true);
        }
        break;
      case CODE_RESET_PAGE_DATA:{
        MSG_DEBUG("inbox_received_callback:Recieved CODE_RESET_PAGE_DATA");
        unload_page();
        destroy_page_menu();
        init_page_menu();
      }
      break;
      case CODE_OP_FAILED:{
        MSG_DEBUG("inbox_received_callback:Recieved CODE_OP_FAILED, %s",message_text->value->cstring);
        if(op != NULL){
          switch((OpCode) op->value->int16){
            case OP_LOGIN:
              show_notification(message_text->value->cstring, -1, GColorRed,
                                (NotifyCallbacks){.onSuddenClose=closeApp});
              break;
            case OP_LOAD_PAGES:
              show_notification(message_text->value->cstring, 3, GColorRed,
                                (NotifyCallbacks){.onSuddenClose=closeList});
              break;
            case OP_LOAD_TEXT:
              show_notification(message_text->value->cstring, 3, GColorRed,
                                (NotifyCallbacks){.onSuddenClose=closeText});
              break;
            case OP_TOGGLE_FAVE:
              show_notification(message_text->value->cstring, 3, GColorRed,
                              (NotifyCallbacks){0});
              break;
            case OP_TOGGLE_ARCHIVE:
              show_notification(message_text->value->cstring, 3, GColorRed,
                              (NotifyCallbacks){0});
              break;
            case OP_DELETE_PAGE:
              show_notification(message_text->value->cstring, 3, GColorRed,
                                (NotifyCallbacks){0});
              break;
          }
        }
        else show_notification(message_text->value->cstring, 3, GColorRed,
                              (NotifyCallbacks){0});
      }
      break;
      case CODE_OP_SUCCEEDED:{
        MSG_DEBUG("inbox_received_callback:Recieved CODE_OP_SUCCEEDED, %s",message_text->value->cstring);
        show_notification(message_text->value->cstring, 3, GColorGreen,
                         (NotifyCallbacks){.onDisappear = hide_notification});
      }
    }return;
  }
  MSG_ERROR("inbox_dropped_callback:Received message with no message code!");
}
