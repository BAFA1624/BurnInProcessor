#pragma once

// Exclude rarely used stuff
#define WIN32_LEAN_AND_MEAN
// Windows header files
#include <Windows.h>   // Windows functions & BSTR
#include <atlsafe.h>   // CComSafeArray
#include <atlcomcli.h> // CComVARIANT
#include <OleAuto.h>   // SAFEARRAY, etc.
#include <comdef.h>    // _bstr_t

// Standalone C++ std libraries here

// External Libraries

// Spreadsheet Functionality
#include "S__Spreadsheet_Classes/BIDR_Spreadsheet.h"
