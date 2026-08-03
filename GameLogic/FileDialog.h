/*
 * ===========
 * FILE DIALOG
 * ===========
 *
 * A generic file dialog that allows the user
 * to select a file
 *
 */

#pragma once

#include <vector>

#include "DArray.h"
#include "SpeciesWindow.h"

class ScrollBar;



class FileDialog : public SpeciesWindow
{
public:
    char    *m_path;
    char    *m_filter;
    char    *m_parent;
	bool    m_allowMultiSelect;
    bool	m_okPressed;

    // Owned. Holds names from Resource::ListResources, so each one is a
    // `new char[]` and needs delete[].
    std::vector<char*>* m_files;
    LList   <int>     m_selected;

    ScrollBar   *m_scrollBar;

public:
    FileDialog( char const *name, char const *parent,
                char const *path=nullptr, char const *filter=nullptr,
                bool allowMultiSelect=false );
    ~FileDialog();

    void Create();
    void Remove();

    void SetDirectory   ( char const *path );
    void SetParent      ( char const *parent );
    void SetFilter      ( char const *filter );

    void FileClicked    ( int index );
    int  IsFileSelected ( int index );                                  // Returns index within m_selected, or -1 if not found

    void RefreshFileList();

    virtual void FileSelected( char *filename );                        // This will be called for all selected files
};


