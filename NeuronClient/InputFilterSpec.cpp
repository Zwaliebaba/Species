#include "pch.h"

#include "InputFilterSpec.h"


unsigned long newFilterSpecID() {
	static unsigned long nextID = 0;
	return nextID++;
}
