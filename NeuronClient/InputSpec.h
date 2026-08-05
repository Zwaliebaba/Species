#pragma once

#include <string>
#include <vector>
#include <memory>
#include <iostream>

typedef int control_id_t;
typedef int inputtype_t;
// Driver-defined, and it stays an int deliberately. Three drivers put three
// unrelated things in InputSpec::condition: InputCondition's enumerators
// (the default driver), the anonymous {COND_TRUE, COND_FALSE} enum in
// InputDriverPrefs.cpp, and a millisecond count IdleInputDriver parses out of
// the binding string. Naming any one of those types here would be wrong for
// the other two. See language-hygiene T9's notes.
typedef int condition_t;
typedef int handler_id_t;

struct InputSpec {
	unsigned driver;         // ID of InputDriver which handles this input
	inputtype_t type;        // Type of input details to expect
	control_id_t control_id; // Keycode, button number, etc.
	handler_id_t handler_id; // Maybe the driver contains several input handling functions
	condition_t condition;   // Condition upon which this triggers (down, up, held, clicked, etc.)
};


// Class to tokenise a prefs string
class InputSpecTokens {

private:
	std::vector<std::string> m_tokens;
	InputSpecTokens( std::vector<std::string> _tokens );

public:
	InputSpecTokens( std::string _string );
	~InputSpecTokens();
	unsigned length() const;
	const std::string &operator[]( unsigned _index ) const;
	std::unique_ptr<InputSpecTokens> operator()( int _start, int _end ) const;

};


std::ostream &operator<<( std::ostream &stream, InputSpecTokens const &tokens );


