// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryGame/IGame.h>

struct IGameRef
{
	CRY_DEPRECATED_GAME_DLL IGameRef() : m_ptr(0) {}
	IGameRef(IGame** ptr) : m_ptr(ptr) {};
	~IGameRef() {};

	IGame*    operator->() const     { return m_ptr ? *m_ptr : 0; };
	operator IGame*() const { return m_ptr ? *m_ptr : 0; };
	IGameRef& operator=(IGame** ptr) { m_ptr = ptr; return *this; };

private:
	IGame** m_ptr;
};