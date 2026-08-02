#pragma once

#include "MessageDialog.h"

class UpdateAvailableWindow : public MessageDialog {
public:
	UpdateAvailableWindow( const char *newVersion, const char *changeLog );
	void Create();
};
