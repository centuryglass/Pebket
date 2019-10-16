/*
*@File messaging_core.h
*Manages sending and receiving of messages
*/

#pragma once
#include <pebble.h>

#define PEBBLE_DICT_SIZE 128
#define JS_DICT_SIZE 2048//AppMessage dictionary size
typedef void (* InboxHandler)(DictionaryIterator *iterator);

/**
*Initialize messaging and open AppMessage
*/
void open_messaging();

/**
*Free up memory and disable messaging
*/
void close_messaging();

/**
*Removes all existing messages from the message queue
*/
void delete_all_messages();

/**
*Sets whether messages should be sent, or just
*held until later
*@param send_messages true to send, false to save
*/
void toggle_message_sending(bool send_messages);

/*
*Registers a function to pass incoming messages
*to be read
*@param handler the message reading function
*/
void register_inbox_handler(InboxHandler handler);

/**
*Adds a message to the outbox queue, to
*be sent soon
*@param dictBuf a dictionary message buffer to
*be copied
*/
void add_message(uint8_t dictBuf[PEBBLE_DICT_SIZE]);
