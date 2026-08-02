#pragma once


class EventHandler {

public:
	virtual bool WindowHasFocus() = 0;

};


extern EventHandler * g_eventHandler;


