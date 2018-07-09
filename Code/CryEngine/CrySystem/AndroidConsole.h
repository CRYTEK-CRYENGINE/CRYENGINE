// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:
   Console implementation for Android, reports back to the main interface.
   -------------------------------------------------------------------------
   History:
   - Aug 26,2013:	Created by Leander Beernaert

*************************************************************************/

#ifndef _ANDROIDCONSOLE_H_
#define _ANDROIDCONSOLE_H_

#include <CrySystem/IConsole.h>
#include <CrySystem/ITextModeConsole.h>
#include <CryNetwork/INetwork.h>

class CAndroidConsole : public ISystemUserCallback,
	                      public IOutputPrintSink,
	                      public ITextModeConsole
{

	CAndroidConsole(const CAndroidConsole&);
	CAndroidConsole& operator=(const CAndroidConsole&);

	bool m_isInitialized;
public:
	static CryCriticalSectionNonRecursive s_lock;
public:
	CAndroidConsole();
	~CAndroidConsole();

	// Interface IOutputPrintSink /////////////////////////////////////////////
	DLL_EXPORT virtual void Print(const char* line);

	// Interface ISystemUserCallback //////////////////////////////////////////
	virtual bool          OnError(const char* errorString);
	virtual bool          OnSaveDocument()  { return false; }
	virtual void          OnProcessSwitch() {}
	virtual void          OnInitProgress(const char* sProgressMsg);
	virtual void          OnInit(ISystem*);
	virtual void          OnShutdown();
	virtual void          OnUpdate();
	virtual void          GetMemoryUsage(ICrySizer* pSizer);
	void                  SetRequireDedicatedServer(bool) {}
	virtual void          SetHeader(const char*)          {}
	// Interface ITextModeConsole /////////////////////////////////////////////
	virtual Vec2_tpl<int> BeginDraw();
	virtual void          PutText(int x, int y, const char* msg);
	virtual void          EndDraw();

};

#endif // _ANDROIDCONSOLE_H_
