#include <pebble.h>
#include "subpage.h"
#include "util.h"
#include "options.h"

//#define SUBPAGE_DEBUG_ENABLED//comment out to disable page debug logs
#ifdef SUBPAGE_DEBUG_ENABLED
#define SUBPAGE_DEBUG(fmt, ...) APP_LOG(APP_LOG_LEVEL_DEBUG,fmt,##__VA_ARGS__);
#define SUBPAGE_ERROR(fmt, ...) APP_LOG(APP_LOG_LEVEL_ERROR,fmt,##__VA_ARGS__);
#else
#define SUBPAGE_DEBUG(fmt, args...) 
#define SUBPAGE_ERROR(fmt, args...) 
#endif

#define MEMORY_THRESHOLD 1500
//if memory drops below this level, unload subpages



//----------SUBPAGE DATA----------
//Each subpage contains a portion of the saved page text, subpages are 
//of similar but not identical length
typedef struct sub{
  int pageIndex;
  char * pageString;
  TextLayer * pageText;
  struct sub * nextPage;
}Subpage;

Subpage * firstSubpage = NULL;
ScrollLayer * parentLayer = NULL;
//----------STATIC FUNCTION DECLARATIONS----------
//creates a new subpage
static Subpage * subpage_create(char * subpageText, int subpageIndex);
//deallocates a given subpage
static void subpage_destroy(Subpage * subpage);
//get the last subpage in the list
static Subpage * getLastSubpage();
//add a subpage to the front of the list
static void subpage_push_front(Subpage * subpage);
//add a subpage to the end of the list
static void subpage_push_end(Subpage * subpage);
//remove and return the first subpage from the list
static Subpage * subpage_pop_front();
//remove and return the last subpage from the list
static Subpage * subpage_pop_end();
//resize a subpage's text layer to fit its content
//return: the new text layer height
static int resize_textLayer_to_content(TextLayer * textlayer);
//destroy the subpage at the front of the list
static void subpage_destroy_front();
//destroy the subpage at the end of the list
static void subpage_destroy_end();
//----------PUBLIC FUNCTIONS----------

//Sets a scroll layer as the new parent layer for all subpages
void subpage_set_parent(ScrollLayer * parent){
  if(parentLayer == parent){
    SUBPAGE_DEBUG("subpage_set_parent:provided layer is already the parent layer");
  }else{
    SUBPAGE_DEBUG("subpage_set_parent:new parent layer set, removing old sublayers");
    subpage_destroy_all();
    parentLayer = parent;
  }
}

/**
*Creates and adds a new subpage with the given parameters
*pageText: subpage display text
*pageIndex: subpage index
*/
void subpage_init(char * pageText, int pageIndex){
  if(parentLayer == NULL){
    SUBPAGE_ERROR("subpage_init: can't create subpages without first setting a parent layer");
    return;
  }
  if(pageIndex == first_page_index()-1 || firstSubpage == NULL)
    subpage_push_front(subpage_create(pageText,pageIndex));
  else if(pageIndex == last_page_index()+1)
    subpage_push_end(subpage_create(pageText,pageIndex));
  else{
    SUBPAGE_ERROR("subpage_init: received invalid page index %d, expected %d or %d",pageIndex,first_page_index()-1,last_page_index()+1);
  }
}

/**
*Gets a bookmark marking the parent layer's current
*scroll offset
*/
Bookmark bookmark_from_parent_offset(){
  //find the right subpage
  GPoint offset = scroll_layer_get_content_offset(parentLayer);
  return bookmark_from_offset(offset.y);
}

/**
*Gets a bookmark marking the given vertical scroll offset
*/
Bookmark bookmark_from_offset(int offset){
  SUBPAGE_DEBUG("bookmark_from_offset:finding bookmark for offset=%d",offset);
  if(firstSubpage == NULL) return (Bookmark){-1,-1};
  Subpage * index = firstSubpage;
  int subpage = index->pageIndex;
  GRect lastFrame = GRect(0,0,0,0);
  while(index != NULL){
    GRect subFrame = layer_get_frame(text_layer_get_layer(index->pageText));
    if(-offset <= subFrame.origin.y)break;
    lastFrame = subFrame;
    subpage = index->pageIndex;
    index = index->nextPage;
  }
  if(lastFrame.origin.y <= -offset){
    //last subpage was the current one, find & return percent
    int pageOffset = -offset - lastFrame.origin.y;
    //pageOffset/pageHeight = percentOffset/100
    //pageOffset*100/pageHeight = percentOffset;
    int percentOffset = pageOffset * 100 / lastFrame.size.h;
    if(percentOffset > 100)percentOffset = 100;
    SUBPAGE_DEBUG("bookmark_from_offset:offset:%d = pg%d,%d%%" ,offset,subpage,pageOffset * 100 / lastFrame.size.h);
    return (Bookmark){subpage, pageOffset * 100 / lastFrame.size.h};
  }
  else return (Bookmark){firstSubpage->pageIndex,0};
}

/**
*Given a bookmark, return the corresponding offset
*/
GPoint offset_from_bookmark(Bookmark bookmark){
  //find the marked subpage
  Subpage * index = firstSubpage;
  while(index != NULL){
    if(index->pageIndex == bookmark.subpage)break;
    index = index->nextPage;
  }
  if(index == NULL){
    SUBPAGE_ERROR("offset_from_bookmark:failed to find subpage %d",bookmark.subpage);
    return GPoint(-1,-1);
  }
  //get subpage frame
  GRect frame = layer_get_frame(text_layer_get_layer(index->pageText));
  int percentHeight = 0;
  if(bookmark.offsetPercent != 0)
    percentHeight = frame.size.h * bookmark.offsetPercent / 100;
  return GPoint(0,-(frame.origin.y + percentHeight));
}

/**
*Vertically moves the start of the text to a given offset
*offset: amount to move subpages
*/
void subpage_translate_all(int offset){
  if(firstSubpage == NULL){
    SUBPAGE_ERROR("subpage_translate_all:no subpages found!");
    return;
  }
  SUBPAGE_DEBUG("subpage_translate_all:moving by %d",offset);
  //move first subpage by offset,each other subpage to align with the previous one
  Subpage * pageIndex = firstSubpage;
  int nextLayerOrigin = get_text_top() + offset;
  while(pageIndex != NULL){
    Layer * pageLayer = text_layer_get_layer(pageIndex->pageText);
    GRect pageFrame = layer_get_frame(pageLayer);
    pageFrame.origin.y = nextLayerOrigin;
    layer_set_frame(pageLayer,pageFrame);
    nextLayerOrigin = pageFrame.origin.y + resize_textLayer_to_content(pageIndex->pageText);
    pageIndex = pageIndex->nextPage;
  }
}

/**
*destroy all subpages
*/
void subpage_destroy_all(){
  //PAGE_DEBUG("subpage_destroy_all: begin.");
  Subpage * removedPage = subpage_pop_front();
  int pagesRemoved = 0;
  while(removedPage != NULL){
    subpage_destroy(removedPage);
    removedPage = subpage_pop_front();
    pagesRemoved++;
  }
  parentLayer = NULL;
  SUBPAGE_DEBUG("subpage_destroy_all:destroyed %d pages", pagesRemoved);
}

/**
*gets the y position of the top of all loaded text
*/
int get_text_top(){
  //PAGE_DEBUG("getTextTop: finding highest text coord");
  if(firstSubpage == NULL)return 0;
  int textTop = layer_get_frame(text_layer_get_layer(firstSubpage->pageText)).origin.y;
  //PAGE_DEBUG("getTextTop: top is at %d", textTop);
  return textTop;
}

/**
*gets the y position of the bottom of all loaded text
*/
int get_text_bottom(){
  //PAGE_DEBUG("getTextBottom: finding lowest text coord.");
  if(firstSubpage == NULL)return 0;
  GRect lastPageFrame = layer_get_frame(text_layer_get_layer(getLastSubpage()->pageText));
  int textBottom = lastPageFrame.origin.y + lastPageFrame.size.h;
  //PAGE_DEBUG("getTextBottom: bottom is at %d", textBottom);
  return textBottom;
}

/**
*returns the index of the first subpage
*/
int first_page_index(){
  if(firstSubpage == NULL)return 0;
  else return firstSubpage->pageIndex;
}

/**
*returns the index of the last subpage
*/
int last_page_index(){
  if(firstSubpage == NULL)return 0;
  else return getLastSubpage()->pageIndex;
}


//----------STATIC FUNCTIONS----------

/**
*creates a new subpage
*/
static Subpage * subpage_create(char * subpageText, int subpageIndex){
  Subpage * newSub = malloc(sizeof(Subpage));//allocate page
  //allocate textLayer and string
  newSub->pageText = text_layer_create(GRect(0,0,SCREEN_WIDTH,30000));
  newSub->pageString = malloc_set_text(newSub->pageText,NULL, subpageText, strlen(subpageText));
  //set text layer properties
  text_layer_set_font(newSub->pageText,getPageFont());
  text_layer_set_background_color(newSub->pageText, getBGColor());
  text_layer_set_text_color(newSub->pageText, getTextColor());
  //initialize index and nextpage to default values
  newSub->nextPage = NULL;
  newSub->pageIndex = subpageIndex;
  return newSub;
}

/**
*deallocates a given subpage
*/
static void subpage_destroy(Subpage * subpage){
  //PAGE_DEBUG("subpage_destroy: subpage destruction begin.");
  if(subpage != NULL){
    if(subpage->pageString != NULL)
      free(subpage->pageString);
    if(subpage->pageText != NULL)
      text_layer_destroy(subpage->pageText);
    free(subpage);
  }
}

/**
*get the last subpage in the list
*/
static Subpage * getLastSubpage(){
  //PAGE_DEBUG("getLastSubpage: finding last.");
  Subpage * index = firstSubpage;
  while(index != NULL && index->nextPage != NULL){
    index = index->nextPage;
  }
  return index;
}

/**
*add the first page to the list
*/
static void subpage_add_first(Subpage * subpage){
  firstSubpage = subpage;
  Layer * textLayer = text_layer_get_layer(subpage->pageText);
  if(parentLayer != NULL){
    scroll_layer_add_child(parentLayer, textLayer);
    resize_textLayer_to_content(subpage->pageText);
    SUBPAGE_DEBUG("subpage_add_first:Adding page to index 0, height:%d",get_text_bottom());
  }
}

/**
*add a subpage to the front of the list
*/
static void subpage_push_front(Subpage * subpage){
  //PAGE_DEBUG("subpage_push_front: begin.");
  if(firstSubpage == NULL){
    subpage_add_first(subpage);
    return;
  }
  Layer * textLayer = text_layer_get_layer(subpage->pageText);
  //add text layer to scrollLayer
  scroll_layer_add_child(parentLayer, textLayer);
  //position new layer over old first layer
  resize_textLayer_to_content(subpage->pageText);
  GRect newPageFrame = layer_get_frame(textLayer);
  newPageFrame.origin.y = get_text_top() - newPageFrame.size.h;
  layer_set_frame(textLayer,newPageFrame);
  if(getPagingEnabled()){
    //adjust position to correct for changed page height
    newPageFrame = layer_get_frame(textLayer);
    //if page is too high, move down until the page is a bit too low
    while(newPageFrame.origin.y + newPageFrame.size.h <= get_text_top()){
      newPageFrame.origin.y++;
      layer_set_frame(textLayer,newPageFrame);
      resize_textLayer_to_content(subpage->pageText);
      newPageFrame = layer_get_frame(textLayer);
    }
    //page is now definitely too low, move up by one until it isn't
    while(newPageFrame.origin.y + newPageFrame.size.h > get_text_top()){
      newPageFrame.origin.y--;
      layer_set_frame(textLayer,newPageFrame);
      resize_textLayer_to_content(subpage->pageText);
      newPageFrame = layer_get_frame(textLayer);
    }
  }
  SUBPAGE_DEBUG("subpage_push_front:Adding page to index %d, position %d, height %d",
                    subpage->pageIndex,newPageFrame.origin.y,newPageFrame.size.h);
  //add subpage to list
  subpage->nextPage = firstSubpage;
  firstSubpage = subpage;
  if(heap_bytes_free() < MEMORY_THRESHOLD){
    SUBPAGE_DEBUG("subpage_push_front:memory limits reached, removing bottom subpage");
    subpage_destroy_end();
  }
}

/**
*add a subpage to the end of the list
*/
static void subpage_push_end(Subpage * subpage){
  //PAGE_DEBUG("subpage_push_end: begin.");
  if(firstSubpage == NULL){
    subpage_add_first(subpage);
    return;
  }
  Subpage * lastPage = getLastSubpage();
  Layer * textLayer = text_layer_get_layer(subpage->pageText);
  //add text layer to scrollLayer
  scroll_layer_add_child(parentLayer, textLayer);
  //move textLayer to after the last new page
  GRect newPageFrame = layer_get_frame(textLayer);
  newPageFrame.origin.y = get_text_bottom();
  layer_set_frame(textLayer,newPageFrame);
  resize_textLayer_to_content(subpage->pageText);
  //add subpage to list
  lastPage->nextPage = subpage;
  if(heap_bytes_free() < MEMORY_THRESHOLD){
    SUBPAGE_DEBUG("subpage_push_end:memory limits reached, removing top subpage");
    subpage_destroy_front();
  }
}

/**
*remove and return the first subpage from the list
*/
static Subpage * subpage_pop_front(){
  Subpage * front = firstSubpage;
  if(front != NULL)firstSubpage = front->nextPage;
  return front;
}

/**
*remove and return the last subpage from the list
*/
static Subpage * subpage_pop_end(){
  Subpage * index = firstSubpage;
  while(index != NULL && 
        index->nextPage != NULL &&
       index->nextPage->nextPage != NULL){
    index = index->nextPage;
  }
  //if 0 or 1 pages found,
  if(index == firstSubpage){
    firstSubpage = NULL;
    return index;
  }
  //if more than 1 page found, index = new end page
  Subpage * removedPage = index->nextPage;
  index->nextPage = NULL;
  return removedPage;
}

/**
*destroy the subpage at the front of the list
*/
static void subpage_destroy_front(){
  subpage_destroy(subpage_pop_front());
}

/**
*destroy the subpage at the end of the list
*/
static void subpage_destroy_end(){
  subpage_destroy(subpage_pop_end());
}



/**
*resize the text layer to fit the page content
*return: the new text layer height
*/
static int resize_textLayer_to_content(TextLayer * textlayer){
  if(getPagingEnabled()){
    text_layer_enable_screen_text_flow_and_paging(textlayer, 3);
    text_layer_set_text_alignment(textlayer, GTextAlignmentCenter);
  }
  GSize layerSize = layer_get_frame(text_layer_get_layer(textlayer)).size;
  layerSize.h = 30000;
  text_layer_set_size(textlayer,layerSize);
  layerSize.h = text_layer_get_content_size(textlayer).h;
  text_layer_set_size(textlayer,layerSize);
  return layerSize.h;
}
