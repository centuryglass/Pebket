#include <pebble.h>
#pragma once

//loads options menu data
void init_options();

//unloads options menu data
void destroy_options();

//shows the options menu
void open_options_menu();

/**
*Returns a status bar layer already set up
*with user selected colors, or NULL if the status
*bar is disabled.  Caller is responsible for destroying
*the status bar on close.
*/
StatusBarLayer * getStatusBar();

//Gets the font chosen for menu items
GFont getMenuFont();

//Gets the font chosen for saved page titles
GFont getTitleFont();

//Gets the font chosen for page text
GFont getPageFont();

//Gets the main backround color
GColor getBGColor();

//Gets the main text color
GColor getTextColor();

//Gets the background color for selected menu items
GColor getSelectedBGColor();

//Gets the text color for selected menu items
GColor getSelectedTextColor();

//true if pages should use paging, false if they use scrolling
bool getPagingEnabled();
