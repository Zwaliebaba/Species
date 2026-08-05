#pragma once

#include <functional>
#include <memory>
#include <type_traits>


#include <limits.h>

#include "SpeciesWindow.h"


namespace Species
{
  class DropDownOptionData
  {
    public:
      DropDownOptionData(const char* _word, int _value);
      ~DropDownOptionData();

      char* m_word;
      int m_value;
  };


  // ****************************************************************************
  // Class DropDownMenu
  // ****************************************************************************

  class DropDownWindow : public SpeciesWindow
  {
    public:
      char m_parentName[256];
      static DropDownWindow* s_window;

    public:
      DropDownWindow(char* _name, char* _parentName);
      void Update();

      // There can be only one
      static DropDownWindow* CreateDropDownWindow(std::string_view _name, std::string_view _parentName);
      static void RemoveDropDownWindow();
  };


  class DropDownMenu : public SpeciesButton
  {
    protected:
      std::vector<std::unique_ptr<DropDownOptionData>> m_options;
      int m_currentOption;
      int* m_int;
      // Set by RegisterEnum instead of m_int. Exactly one of the two is ever
      // non-empty; SelectOption writes through whichever it has.
      std::function<void(int)> m_writeBack;
      bool m_sortItems;
      int m_nextValue; // Used by AddOption if a value isn't passed in as an argument

    public:
      DropDownMenu(bool _sortItems = false);
      ~DropDownMenu();

      void Empty();
      void AddOption(char const* _option, int _value = INT_MIN);
      int GetSelectionValue();
      char const* GetSelectionName();
      virtual void SelectOption(int _option);
      bool SelectOption2(char const* _option);

      void CreateMenu();
      void RemoveMenu();
      bool IsMenuVisible();

      void RegisterInt(int* _int);

      // Binds a scoped-enum variable, which cannot be reached through an int*.
      // The menu still deals in the enumerators' underlying values, so the
      // options' values have to be the enumerators -- AddOption's default
      // numbering does that when the options are added in declaration order.
      //
      // Added by language-hygiene T13 for CamAnimNode::Transition. Every other
      // caller here still binds a genuine int, and several of those ints are
      // enums that later tasks in that plan will scope; this is the hook they
      // will need.
      template <typename E>
        requires std::is_enum_v<E>
      void RegisterEnum(E* _enum)
      {
        m_writeBack = [_enum](int _value) { *_enum = static_cast<E>(_value); };
        SelectOption(static_cast<std::underlying_type_t<E>>(*_enum));
      }

      int FindValue(int _value); // Returns an index into m_options

      void Render(int realX, int realY, bool highlighted, bool clicked);
      void MouseUp();
  };


  // ****************************************************************************
  // Class DropDownMenuOption
  // ****************************************************************************

  class DropDownMenuOption : public BorderlessButton
  {
    public:
      char* m_parentWindowName;
      char* m_parentMenuName;
      //    int     m_menuIndex;
      int m_value; // An integer that the client specifies - often a value from an enum

    public:
      DropDownMenuOption();
      ~DropDownMenuOption();

      void SetParentMenu(EclWindow* _window, DropDownMenu* _menu, int _value);

      void Render(int realX, int realY, bool highlighted, bool clicked);
      void MouseUp();
  };
} // namespace Species
