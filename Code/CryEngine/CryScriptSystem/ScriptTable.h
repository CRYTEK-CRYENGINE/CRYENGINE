// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#if !defined(AFX_SCRIPTOBJECT_H__6EA3E6D6_4FF9_4709_BD62_D5A97C40DB68__INCLUDED_)
#define AFX_SCRIPTOBJECT_H__6EA3E6D6_4FF9_4709_BD62_D5A97C40DB68__INCLUDED_

#if _MSC_VER > 1000
	#pragma once
#endif // _MSC_VER > 1000

#include <CryScriptSystem/IScriptSystem.h>
#pragma warning(push) // Because lua.h touches warning C4996
extern "C" {
#include <lua.h>
}
#pragma warning(pop)
class CScriptSystem;

enum
{
	DELETED_REF = -1,
	NULL_REF    = 0,
};

#ifdef DEBUG_LUA_STATE
extern std::set<class CScriptTable*> gAllScriptTables;
#endif

/*! IScriptTable implementation
   @see IScriptTable
 */
class CScriptTable : public IScriptTable
{
public:
	//! constructor
	CScriptTable() { m_nRef = NULL_REF; m_nRefCount = 0; }

	// interface IScriptTable ----------------------------------------------------------------
	virtual void           AddRef()  { m_nRefCount++; }
	virtual void           Release() { if (--m_nRefCount <= 0) DeleteThis(); };

	virtual IScriptSystem* GetScriptSystem() const;
	virtual void           Delegate(IScriptTable* pMetatable);

	virtual void*          GetUserDataValue();

	//////////////////////////////////////////////////////////////////////////
	// Set/Get chain.
	//////////////////////////////////////////////////////////////////////////
	virtual bool BeginSetGetChain();
	virtual void EndSetGetChain();

	//////////////////////////////////////////////////////////////////////////
	virtual void SetValueAny(const char* sKey, const ScriptAnyValue& any, bool bChain = false);
	virtual bool GetValueAny(const char* sKey, ScriptAnyValue& any, bool bChain = false);

	//////////////////////////////////////////////////////////////////////////
	virtual void          SetAtAny(int nIndex, const ScriptAnyValue& any);
	virtual bool          GetAtAny(int nIndex, ScriptAnyValue& any);

	virtual ScriptVarType GetValueType(const char* sKey);
	virtual ScriptVarType GetAtType(int nIdx);

	//////////////////////////////////////////////////////////////////////////
	// Iteration.
	//////////////////////////////////////////////////////////////////////////
	virtual IScriptTable::Iterator BeginIteration(bool resolvePrototypeTableAsWell = false);
	virtual bool                   MoveNext(Iterator& iter);
	virtual void                   EndIteration(const Iterator& iter);
	//////////////////////////////////////////////////////////////////////////

	virtual void Clear();
	virtual int  Count();
	virtual bool Clone(IScriptTable* pSrcTable, bool bDeepCopy = false, bool bCopyByReference = false);
	virtual void Dump(IScriptTableDumpSink* p);

	virtual bool AddFunction(const SUserFunctionDesc& fd);

	// --------------------------------------------------------------------------
	void CreateNew();

	int  GetRef();
	void Attach();
	void AttachToObject(IScriptTable* so);
	void DeleteThis();

	// Create object from pool.
	void Recreate() { m_nRef = NULL_REF; m_nRefCount = 1; };
	// Assign a metatable to a table.
	void SetMetatable(IScriptTable* pMetatable);
	// Push reference of this object to the stack.
	void PushRef();
	// Push reference to specified script table to the stack.
	void PushRef(IScriptTable* pObj);

	//////////////////////////////////////////////////////////////////////////
	// Custom new/delete.
	//////////////////////////////////////////////////////////////////////////
	void* operator new(size_t nSize);
	void  operator delete(void* ptr);

public:
	// Lua state, set by CScriptSystem::Init
	static lua_State*     L;
	// Pointer to ScriptSystem, set by CScriptSystem::Init
	static CScriptSystem* m_pSS;

private:
	static int  StdCFunction(lua_State* L);
	static int  StdCUserDataFunction(lua_State* L);

	static void CloneTable(int srcTable, int trgTable);
	static void CloneTable_r(int srcTable, int trgTable);
	static void ReferenceTable_r(int scrTable, int trgTable);

private:
	int m_nRefCount;
	int m_nRef;
};

#endif // !defined(AFX_SCRIPTOBJECT_H__6EA3E6D6_4FF9_4709_BD62_D5A97C40DB68__INCLUDED_)
