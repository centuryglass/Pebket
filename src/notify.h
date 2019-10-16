#include <pebble.h>

//
typedef void(*Callback)();

typedef struct NotifyCallStruct{
  //runs when the notification window appears
  Callback onAppear;
  //runs when the notification window times out
  Callback onTimeout;
  //runs when the notification window disappears,
  //but is still on the window stack
  Callback onDisappear;
  //runs when the notification window is closed
  //unexpectedly, like when the back button is pressed
  Callback onSuddenClose;
  //runs when the notification window is closed by
  //a call to hide_notification
  Callback onNormalClose;
}NotifyCallbacks;

/**
*Shows a new notification screen
*notifyMsg: notification display text
*duration: how long until the notification is dismissed
*stays until manually removed if duration < 0
*bgColor: notification background color
*nCallbacks: assigns callbacks to run during different
*notify window events
*Note: Callbacks may be null
*/
void show_notification(char * notifyMsg, int duration, GColor bgColor, NotifyCallbacks nCallbacks);

/**
*dismisses the notification screen
*/
void hide_notification();

//Predefined callbacks:

//Closes any pageView window
void closeText();

//Closes pageView and pageMenu
void closeList();

//closes the entire app
void closeApp();

//reloads and opens the page list
void reloadList();

