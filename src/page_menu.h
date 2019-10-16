#pragma once
/**
*page_menu.h
*Handles any windows displaying a list of pocket saved pages
*/

/**
*Requests the first few titles on the list
*/
void requestInitialTitles();

/**
*Initialize the page title menu and menu window
*/
void init_page_menu();

/**
*Destroy the page title menu and menu window
*/
void destroy_page_menu();

/**
*Load new titles into the page list
*This will initialize the page menu if necessary
*newTitles: all new titles, separated by '\n'
*firstNewIndex: index of the first title received
*/
void update_titles(const char * newTitles, int firstNewIndex);

/**
*Removes a title from the list
*/
void remove_title(int titleIndex);
