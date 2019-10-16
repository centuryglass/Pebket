#pragma once
#include <pebble.h>
//subpage.h handles a collection of subpages, small ordered
//text display elements that are added to a scrollLayer

//Bookmark: marks a place in the list of subpages
typedef struct bookmarkStruct{
  //currently viewed subpage
  int subpage;
  //the exact percent of the subpage height where
  //the view is found
  int offsetPercent;
}Bookmark;

/**
*Sets a scroll layer as the new parent layer for all subpages
*/
void subpage_set_parent(ScrollLayer * parent);

/**
*Creates and adds a new subpage with the given parameters
*pageText: subpage display text
*pageIndex: subpage index
*/
void subpage_init(char * pageText, int pageIndex);

/**
*Gets a bookmark marking the parent layer's current
*scroll offset
*/
Bookmark bookmark_from_parent_offset();

/**
*Gets a bookmark marking the given vertical scroll offset
*/
Bookmark bookmark_from_offset(int offset);

/**
*Given a bookmark, return the corresponding offset
*/
GPoint offset_from_bookmark(Bookmark bookmark);

/**
*Vertically moves the start of the text to a given offset
*offset: amount to move subpages
*/
void subpage_translate_all(int offset);
  
//destroys all subpages
void subpage_destroy_all();

//return the y coordinate of the top of all subpages
int get_text_top();

//return the y coordinate of the bottom of all subpages
int get_text_bottom();

//returns the index of the first subpage
int first_page_index();

//returns the index of the last subpage
int last_page_index();

