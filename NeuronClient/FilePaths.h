#pragma once

#include <string>

class InputPrefs {

private:
	InputPrefs(); // Don't instantiate
public:
	static const std::string & GetSystemPrefsPath();
	static const std::string & GetLocalePrefsPath();
	static const std::string & GetUserPrefsPath();

};

