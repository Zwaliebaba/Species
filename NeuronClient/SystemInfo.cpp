#include "pch.h"

//#include <dxdiag.h>
#include <stdio.h>

#include "Debug.h"
#include "SystemInfo.h"


namespace Neuron
{
  SystemInfo* g_systemInfo = nullptr;


  SystemInfo::SystemInfo()
  {
    GetLocaleDetails();
    GetDirectXVersion();
  }


  SystemInfo::~SystemInfo() {}


  void SystemInfo::GetLocaleDetails()
  {
    int size;
    bool languageSuccess = false;

    if( !languageSuccess )
    {
	    size = GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_SENGLANGUAGE, nullptr, 0);
	    m_localeInfo.m_language = new char[size + 1];
	    ASSERT_TEXT(GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_SENGLANGUAGE, m_localeInfo.m_language, size),
				      "Couldn't get locale details");
    }


	size = GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_SENGCOUNTRY, nullptr, 0);
	m_localeInfo.m_country = new char[size + 1];
	ASSERT_TEXT(GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_SENGCOUNTRY, m_localeInfo.m_country, size),
				  "Couldn't get country details");
  }


void SystemInfo::GetDirectXVersion()
{
	HKEY hkey;
	long errCode;
	unsigned long bufLen = 256;
	unsigned char buf[256];

	m_directXVersion = -1;

	errCode = RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\DirectX", 0, KEY_READ, &hkey);
	ASSERT_TEXT(errCode == ERROR_SUCCESS, "Failed to get DirectX Version");
	errCode = RegQueryValueEx(hkey, "InstalledVersion", nullptr, nullptr, buf, &bufLen);

    if( errCode == ERROR_SUCCESS )
    {
		m_directXVersion = buf[3];
	}
	else
	{
		// NOTE by Chris : This value doesn't exist on Windows98
		// However the key "Version" does exist
		errCode = RegQueryValueEx(hkey, "Version", nullptr, nullptr, buf, &bufLen );
		if( errCode == ERROR_SUCCESS )
		{
			m_directXVersion = buf[3];
		}
    }

	RegCloseKey(hkey);

//	if (m_directXVersion == -1)
//	{
//		long hr;
//		bool bCleanupCOM = false;
//		bool bSuccessGettingMajor = false;
//
//		// Init COM.  COM may fail if its already been inited with a different
//		// concurrency model.  And if it fails you shouldn't release it.
//		hr = CoInitialize(nullptr);
//		bCleanupCOM = SUCCEEDED(hr);
//
//		// Get an IDxDiagProvider
//		bool bGotDirectXVersion = false;
//		IDxDiagProvider* pDxDiagProvider = nullptr;
//		hr = CoCreateInstance( CLSID_DxDiagProvider,
//							   nullptr,
//							   CLSCTX_INPROC_SERVER,
//							   IID_IDxDiagProvider,
//							   (LPVOID*) &pDxDiagProvider );
//		if( SUCCEEDED(hr) )
//		{
//			// Fill out a DXDIAG_INIT_PARAMS struct
//			DXDIAG_INIT_PARAMS dxDiagInitParam;
//			ZeroMemory( &dxDiagInitParam, sizeof(DXDIAG_INIT_PARAMS) );
//			dxDiagInitParam.dwSize                  = sizeof(DXDIAG_INIT_PARAMS);
//			dxDiagInitParam.dwDxDiagHeaderVersion   = DXDIAG_DX9_SDK_VERSION;
//			dxDiagInitParam.bAllowWHQLChecks        = false;
//			dxDiagInitParam.pReserved               = nullptr;
//
//			// Init the m_pDxDiagProvider
//			hr = pDxDiagProvider->Initialize( &dxDiagInitParam );
//			if( SUCCEEDED(hr) )
//			{
//				IDxDiagContainer* pDxDiagRoot = nullptr;
//				IDxDiagContainer* pDxDiagSystemInfo = nullptr;
//
//				// Get the DxDiag root container
//				hr = pDxDiagProvider->GetRootContainer( &pDxDiagRoot );
//				if( SUCCEEDED(hr) )
//				{
//					// Get the object called DxDiag_SystemInfo
//					hr = pDxDiagRoot->GetChildContainer( L"DxDiag_SystemInfo", &pDxDiagSystemInfo );
//					if( SUCCEEDED(hr) )
//					{
//						VARIANT var;
//						VariantInit( &var );
//
//						// Get the "dwDirectXVersionMajor" property
//						hr = pDxDiagSystemInfo->GetProp( L"dwDirectXVersionMajor", &var );
//						if( SUCCEEDED(hr) && var.vt == VT_UI4 )
//						{
//							m_directXVersion = var.ulVal;
//							bSuccessGettingMajor = true;
//						}
//						VariantClear( &var );
//
//						// If it all worked right, then mark it down
//						if( bSuccessGettingMajor )
//							bGotDirectXVersion = true;
//
//						pDxDiagSystemInfo->Release();
//					}
//
//					pDxDiagRoot->Release();
//				}
//			}
//
//			pDxDiagProvider->Release();
//		}
//
//		if( bCleanupCOM )
//			CoUninitialize();
//	}
}
} // namespace Neuron
