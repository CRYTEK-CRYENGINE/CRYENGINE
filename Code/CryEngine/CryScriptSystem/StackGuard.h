// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _STACKGUARD_H_
#define _STACKGUARD_H_

struct LuaStackGuard
{
	LuaStackGuard(lua_State* p)
	{
		m_pLS = p;
		m_nTop = lua_gettop(m_pLS);
	}
	~LuaStackGuard()
	{
		lua_settop(m_pLS, m_nTop);
	}
private:
	int        m_nTop;
	lua_State* m_pLS;
};

//////////////////////////////////////////////////////////////////////////
// Stack validator.
//////////////////////////////////////////////////////////////////////////
#ifdef _DEBUG
struct LuaStackValidator
{
	const char* text;
	lua_State*  L;
	int         top;
	LuaStackValidator(lua_State* pL, const char* sText)
	{
		text = sText;
		L = pL;
		top = lua_gettop(L);
	}
	~LuaStackValidator()
	{
		if (top != lua_gettop(L))
		{
			assert(0 && "Lua Stack Validation Failed");
			lua_settop(L, top);
		}
	}
};
	#define CHECK_STACK(L) LuaStackValidator __stackCheck__((L), __FUNCTION__);
#else //_DEBUG
//#define CHECK_STACK(L) StackGuard __stackGuard__(L);
	#define CHECK_STACK(L) (void)0;
#endif //_DEBUG

#endif // _STACKGUARD_H_
