// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   Description:  Interface for context view extensions
   -------------------------------------------------------------------------
   History:
   - 02/14/2007   12:34 : Created by Stas Spivakov
*************************************************************************/
#ifndef __ICONTEXTVIEWEXTENSION_H__
#define __ICONTEXTVIEWEXTENSION_H__

class CContextView;

struct IContextViewExtension : public INetMessageSink
{
	virtual void SetParent(CContextView*) = 0;
	virtual void OnWitnessDeclared(EntityId) = 0;
	virtual bool EnterState(EContextViewState state) = 0;
	virtual void Die() = 0;
	virtual void Release() = 0;
};

#endif //__ICONTEXTVIEWEXTENSION_H__
