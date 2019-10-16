#pragma once
/**
*Loads new page text
*pageText: the new page text
*subpageIndex: index of the section of the page being sent
*pageSize:total number of subpages available
*pageState:enum PageState value, enum defined in message_handler.h
*faveStatus:enum FavoriteStatus value, enum defined in message_handler.h
*bookmarkOffset: if a bookmark is being loaded, bookmarkOffset is the marked
*page offset, as a percentage of the subpage height. otherwise,
*bookmarkOffset is -1
*If no page is open, a new page is created
*/
void load_page_text(char * pageText,int subpageIndex,int pageSize,
                    int pageState,int faveStatus,int bookmarkOffset);

/**
*Initializes a new page
*the page window isn't added to the stack until text is provided
*/
void init_page();

/**
*Removes the currently loaded page
*/
void unload_page();

