// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:

   -------------------------------------------------------------------------
   History:
   - 2:8:2004   10:59 : Created by Márcio Martins

*************************************************************************/
#ifndef __IGAMEMODPTR_H__
#define __IGAMEMODPTR_H__

#if _MSC_VER > 1000
	#pragma once
#endif

struct IGame;
struct IGameRef
{
	IGameRef() : m_ptr(0) {}
	IGameRef(IGame** ptr) : m_ptr(ptr) {};
	~IGameRef() {};

	IGame*    operator->() const     { return m_ptr ? *m_ptr : 0; };
	operator IGame*() const { return m_ptr ? *m_ptr : 0; };
	IGameRef& operator=(IGame** ptr) { m_ptr = ptr; return *this; };

private:
	IGame** m_ptr;
};

#endif //__IGAMEMODPTR_H__
