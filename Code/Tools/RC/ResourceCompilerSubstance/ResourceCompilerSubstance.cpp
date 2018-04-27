// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// ResourceCompilerImage.cpp : Defines the entry point for the DLL application.
//

#include "stdafx.h"

#include <CryCore/Assert/CryAssert_impl.h>
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")

#include <assert.h>
#include "ResourceCompilerSubstance.h"
#include "IResCompiler.h"
#include "SubstanceConverter.h"             // CImageConverter




// Must be included only once in DLL module.
#include <platform_implRC.inl>


#include "IResCompiler.h"


extern "C" IMAGE_DOS_HEADER __ImageBase;
HINSTANCE g_hInst = (HINSTANCE)&__ImageBase;

void __stdcall RegisterConverters(IResourceCompiler* pRC)
{
	SetRCLog(pRC->GetIRCLog());

	// image formats
	{
		pRC->RegisterConverter("SubstanceConverter", new CSubstanceConverter(pRC));
		// there are some issues with target and source root with nested calls, so the safest is no not support directdds, since even sandbox doesn't support it.
		//pRC->RegisterKey("directdds", "[Substance] 0/1 to write directly dds and not tif");
	
	}
}
