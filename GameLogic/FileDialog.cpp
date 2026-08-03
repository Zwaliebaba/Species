#include "pch.h"

#include <string.h>

#include "FilesysUtils.h"
#include "HiResTime.h"
#include "TextRenderer.h"
#include "Input.h"
#include "Resource.h"
#include "LanguageTable.h"

#include "Eclipse.h"

#include "ScrollBar.h"
#include "FileDialog.h"


namespace
{
  // the legacy list's ValidIndex, spelled out. Worth keeping rather than dropping: every
  // index below arrives from the scroll bar or from a stale selection, so
  // these really are range tests and not redundant assertions. the legacy list's GetData
  // also returned a null T() out of range, which std::vector will not do.
  bool ValidIndex(const std::vector<char*>* _files, int _index) { return _index >= 0 && _index < static_cast<int>(_files->size()); }
} // namespace


//*****************************************************************************
// Class FileOKButton
//*****************************************************************************

class FileOKButton : public SpeciesButton
{
  public:
    void MouseUp()
    {
      FileDialog* fd = (FileDialog*)m_parent;

      for (int i = 0; i < static_cast<int>(fd->m_selected.size()); ++i)
      {
        int index = fd->m_selected[i];
        DEBUG_ASSERT(ValidIndex(fd->m_files, index));
        char* filename = (*fd->m_files)[index];
        fd->FileSelected(filename);
      }

      EclRemoveWindow(m_parent->m_name);
    }
};


//*****************************************************************************
// Class FileButton
//*****************************************************************************

class FileButton : public EclButton
{
  public:
    int m_index;
    double m_lastClickTime;

  public:
    FileButton(int _index)
      : m_index(_index),
        m_lastClickTime(-1.0)
    {
    }

    void MouseUp()
    {
      FileDialog* fd = (FileDialog*)m_parent;
      int index = m_index + fd->m_scrollBar->m_currentValue;

      if (fd->m_files && ValidIndex(fd->m_files, index))
      {
        fd->FileClicked(index);
      }

      double timeNow = GetHighResTime();
      double delta = timeNow - m_lastClickTime;
      if (delta < 0.2)
      {
        FileOKButton* ok = (FileOKButton*)fd->GetButton(LANGUAGEPHRASE("dialog_ok"));
        ok->MouseUp();
        return;
      }
      m_lastClickTime = timeNow;
    }

    void Render(int realX, int realY, bool highlighted, bool clicked)
    {
      FileDialog* fd = (FileDialog*)m_parent;
      int index = m_index + fd->m_scrollBar->m_currentValue;

      if (fd->m_files && ValidIndex(fd->m_files, index))
      {
        if (fd->IsFileSelected(index) != -1)
        {
          glColor4f(0.3f, 0.3f, 1.0f, 0.5f);
          glBegin(GL_QUADS);
          glVertex2i(realX, realY);
          glVertex2i(realX + m_w, realY);
          glVertex2i(realX + m_w, realY + m_h);
          glVertex2i(realX, realY + m_h);
          glEnd();
        }

        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

        if (clicked || highlighted)
        {
          glBegin(GL_LINE_LOOP);
          glVertex2i(realX, realY);
          glVertex2i(realX + m_w, realY);
          glVertex2i(realX + m_w, realY + m_h);
          glVertex2i(realX, realY + m_h);
          glEnd();
        }

        char* fileName = (*fd->m_files)[index];
        g_editorFont.DrawText2D(realX + 30, realY + 8, 11, fileName);
      }
    }
};


class FileCancelButton : public SpeciesButton
{
    void MouseUp() { EclRemoveWindow(m_parent->m_name); }
};


//*****************************************************************************
// Class SelectedButton
//*****************************************************************************

class SelectedButton : public SpeciesButton
{
    void Render(int realX, int realY, bool highlighted, bool clicked)
    {
      FileDialog* fd = (FileDialog*)m_parent;
      if (static_cast<int>(fd->m_selected.size()) > 1)
      {
        SetCaption(LANGUAGEPHRASE("dialog_multiplefiles"));
      }
      else if (static_cast<int>(fd->m_selected.size()) == 1)
      {
        int index = fd->m_selected[0];
        // RefreshFileList empties m_selected, so this index should be live.
        // The guard is what the legacy list's GetData used to do for free.
        char* filename = ValidIndex(fd->m_files, index) ? (*fd->m_files)[index] : nullptr;
        SetCaption(filename);
      }
      else
      {
        SetCaption(" ");
      }

      SpeciesButton::Render(realX, realY, highlighted, clicked);
    }
};


//*****************************************************************************
// Class FileDialog
//*****************************************************************************

FileDialog::FileDialog(char const* name, char const* parent, char const* path, char const* filter, bool allowMultiSelect)
  : SpeciesWindow(name),
    m_files(nullptr),
    m_path(nullptr),
    m_filter(nullptr),
    m_parent(nullptr),
    m_scrollBar(nullptr),
    m_allowMultiSelect(allowMultiSelect)
{
  SetFilter(filter ? filter : "*");
  SetDirectory(path ? path : "c:\\");
  SetParent(parent);

  m_scrollBar = new ScrollBar(this);
}


FileDialog::~FileDialog()
{
  free(m_path);
  free(m_filter);
  free(m_parent);

  if (m_files)
  {
    // ListResources hands back names allocated with `new char[]`.
    // the legacy list's EmptyAndDelete used plain `delete`, which was the wrong form;
    // writing the loop out makes the right one visible.
    for (char* file : *m_files)
      delete[] file;
    delete m_files;
    m_files = nullptr;
  }

  m_selected.clear();

  delete m_scrollBar;
}


void FileDialog::Create()
{
  SpeciesWindow::Create();

  int numRows = (m_h - 60) / 13;

  for (int i = 0; i < numRows; ++i)
  {
    char name[32];
    sprintf(name, "File %d", i);
    FileButton* button = new FileButton(i);
    button->SetProperties(name, 5, 25 + i * 13, m_w - 25, 12, " ", " ");
    RegisterButton(button);
  }

  SelectedButton* selected = new SelectedButton();
  selected->SetProperties("Selected", 10, m_h - 30, m_w - 140, 20, "", " ");
  RegisterButton(selected);

  FileCancelButton* cancel = new FileCancelButton();
  cancel->SetProperties(LANGUAGEPHRASE("dialog_cancel"), m_w - 60, m_h - 30, 55, 20, LANGUAGEPHRASE("dialog_cancel"));
  RegisterButton(cancel);

  FileOKButton* ok = new FileOKButton();
  ok->SetProperties(LANGUAGEPHRASE("dialog_ok"), m_w - 120, m_h - 30, 55, 20, LANGUAGEPHRASE("dialog_ok"));
  RegisterButton(ok);

  m_scrollBar->Create("FileScroll", m_w - 20, 25, 15, numRows * 13, static_cast<int>(m_files->size()), numRows);
}


void FileDialog::Remove()
{
  SpeciesWindow::Remove();

  m_scrollBar->Remove();
}


void FileDialog::SetDirectory(char const* path)
{
  free(m_path);
  m_path = strdup(path);
  SetTitle((char*)path);
  RefreshFileList();
}


void FileDialog::SetFilter(char const* filter)
{
  free(m_filter);
  m_filter = strdup(filter);
}


void FileDialog::SetParent(char const* parent)
{
  free(m_parent);
  m_parent = strdup(parent);
}


void FileDialog::FileSelected(char* filename) {}


void FileDialog::RefreshFileList()
{
  if (m_files)
  {
    // delete[], not delete — see the destructor.
    for (char* file : *m_files)
      delete[] file;
    delete m_files;
    m_files = nullptr;
  }

  m_selected.clear();

  m_files = g_resource->ListResources(m_path, m_filter, false);

  EclDirtyWindow(m_name);
}

void FileDialog::FileClicked(int index)
{
  bool ctrlKey = g_inputManager->controlEvent(ControlFileMultiSelect);

  if (!m_allowMultiSelect || !ctrlKey)
  {
    m_selected.clear();
  }

  int alreadySelected = IsFileSelected(index);
  if (alreadySelected != -1 && m_allowMultiSelect)
  {
    m_selected.erase(m_selected.begin() + alreadySelected);
  }
  else
  {
    m_selected.push_back(index);
  }
}


int FileDialog::IsFileSelected(int index)
{
  for (int i = 0; i < static_cast<int>(m_selected.size()); ++i)
  {
    if (m_selected[i] == index)
    {
      return i;
    }
  }

  return -1;
}
