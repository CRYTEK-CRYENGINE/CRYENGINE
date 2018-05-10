// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:
   Console implementation for iOS, reports back to the main interface.
   -------------------------------------------------------------------------
   History:
   - Jul 19,2013:	Created by Leander Beernaert

*************************************************************************/

#ifndef _IOSCONSOLE_H_
#define _IOSCONSOLE_H_

#include <CrySystem/IConsole.h>
#include <CrySystem/ITextModeConsole.h>
#include <CryNetwork/INetwork.h>

class CIOSConsole : public ISystemUserCallback,
	                  public IOutputPrintSink,
	                  public ITextModeConsole
{

	CIOSConsole(const CIOSConsole&);
	CIOSConsole& operator=(const CIOSConsole&);

	bool m_isInitialized;
public:
	static CryCriticalSectionNonRecursive s_lock;
public:
	CIOSConsole();
	~CIOSConsole();

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

#endif // _IOSCONSOLE_H_
