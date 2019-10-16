#pragma once
/**
*@File message_handler.h
*Handles application-specific messaging
*functionality
*/
#pragma once
#include <pebble.h>







//All valid data update types
typedef enum{
  UPDATE_TYPE_PAGE_TITLES
} UpdateType;

/**
*Initializes AppMessage functionality
*/
void message_handler_init();

/**
*Shuts down AppMessage functionality
*/
void messaging_deinit();

//valid page states:
typedef enum{
  STATE_UNREAD,
  STATE_ARCHIVE,
  STATE_ALL
}PageState;
//sets the page state to request when getting pages
void setPageState(PageState newState);

//valid favorite statuses:
typedef enum{
  FAVE_FALSE,
  FAVE_TRUE,
  FAVE_ALL
}FavoriteStatus;
//sets the favorite status to request when getting pages
void setFavoriteStatus(FavoriteStatus newFavStatus);

//valid sort orders:
typedef enum{
  SORT_NEWEST,
  SORT_OLDEST,
  SORT_TITLE,
  SORT_URL
}SortType;
//sets the page sort order to request when getting pages
void setSortType(SortType newSortType);

/**
*Requests updated page titles
*@param updateType the appropriate update request code
*/
void get_page_titles(int firstTitleIndex, int numTitles);


/**
*Loads the text of a saved page
*pageIndex: the page to load
*/
void request_page(int pageIndex);

/**
*gets text from the loaded page
*subPage: index of the page section
*to load
*/
void get_page_text(int subPage);

//valid page actions
typedef enum{
  ACTION_ARCHIVE,
  ACTION_FAVORITE,
  ACTION_DELETE,
  ACTION_CLEAR_STORAGE,
  ACTION_UPDATE_PAGES,
  ACTION_REMOVE_BOOKMARK
}JSAction;

/**
*Sends a simple command to javascript
*action: the action to perform
*/
void send_action(JSAction action);

/**
*Saves the currently loaded page to the phone
*subpage: currently viewed subpage
*scrollOffset: amount scrolled past the current subpage's start
*/
void bookmark_current_page(int subPage, int scrollOffset);




