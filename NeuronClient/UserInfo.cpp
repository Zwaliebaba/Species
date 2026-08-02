#include "pch.h"
#include "TextStreamReaders.h"
#include "Debug.h"

#include <string.h>

// Parses the user id file and fills in those fields
// Which must be allocated beforehand
bool GetUserInfoData( char const *_userInfoFilename, char const **_username, char const **_email )
{
    static char *s_username = nullptr;
    static char *s_email = nullptr;

    if( !s_username || !s_email )
    {
        TextFileReader fileReader( _userInfoFilename );
        if( fileReader.IsOpen() )
        {
            fileReader.ReadLine();

            ASSERT_TEXT( stricmp( fileReader.GetNextToken(), "Username" ) == 0, "Failed to parse %s", _userInfoFilename );

            s_username = strdup( fileReader.GetRestOfLine() );

            fileReader.ReadLine();

            ASSERT_TEXT( stricmp( fileReader.GetNextToken(), "Email" ) == 0, "Failed to parse %s", _userInfoFilename );

            s_email = strdup( fileReader.GetRestOfLine() );
        }
    }


    *_username = s_username;
    *_email = s_email;

    return( s_username && s_email );
}

