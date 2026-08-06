#pragma once


//*****************************************************************************
// Class LocaleInfo
//*****************************************************************************


namespace Neuron
{
  class LocaleInfo
  {
    public:
      char* m_language;
      char* m_country;

      LocaleInfo()
        : m_language(nullptr),
          m_country(nullptr)
      {
      }
      ~LocaleInfo()
      {
        delete[] m_language;
        delete[] m_country;
      }
  };


  //*****************************************************************************
  // Class SystemInfo
  //*****************************************************************************

  class SystemInfo
  {
    private:
      void GetLocaleDetails();
      void GetDirectXVersion();

    public:
      LocaleInfo m_localeInfo;
      int m_directXVersion;

      SystemInfo();
      ~SystemInfo();
  };


  extern SystemInfo* g_systemInfo;
} // namespace Neuron
