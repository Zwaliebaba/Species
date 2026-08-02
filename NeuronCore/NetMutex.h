// ****************************************************************************
//  A platform independent mutex implementation
// ****************************************************************************

#pragma once


#include "NetLib.h"


class NetMutex
{
public:
	NetMutex();
	~NetMutex();
	
	void			Lock();
	void			Unlock();
	
	int 			IsLocked() { return m_locked; }
	
protected:
	int 			m_locked;
	NetMutexHandle 	m_mutex;
};


