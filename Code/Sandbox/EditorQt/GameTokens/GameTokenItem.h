// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __GameTokenItem_h__
#define __GameTokenItem_h__
#pragma once

#include "BaseLibraryItem.h"
#include <CryFlowGraph/IFlowSystem.h>

struct IGameToken;
struct IGameTokenSystem;

/*! CGameTokenItem contain definition of particle system spawning parameters.
 *
 */
class SANDBOX_API CGameTokenItem : public CBaseLibraryItem
{
public:
	CGameTokenItem();
	~CGameTokenItem();

	virtual EDataBaseItemType GetType() const { return EDB_TYPE_GAMETOKEN; };

	virtual void              SetName(const string& name);
	void                      Serialize(SerializeContext& ctx);

	//! Called after particle parameters where updated.
	void   Update(bool bRecreate = false);
	string GetTypeName() const;
	string GetValueString() const;
	void   SetValueString(const char* sValue);
	//! Retrieve value, return false if value cannot be restored
	bool    GetValue(TFlowInputData& data) const;
	//! Set value, if bUpdateGTS is true, also the GameTokenSystem's value is set
	void    SetValue(const TFlowInputData& data, bool bUpdateGTS = false);

	void    SetLocalOnly(bool localOnly)                { m_localOnly = localOnly; ApplyFlags(); }
	bool    GetLocalOnly() const                        { return m_localOnly; }
	bool    SetTypeName(const char* typeName);
	void    SetDescription(const char* sDescription)    { m_description = sDescription; }
	string  GetDescription() const                      { return m_description; }

private:
	void ApplyFlags();
	IGameTokenSystem* m_pTokenSystem;
	TFlowInputData    m_value;
	string            m_description;
	string            m_cachedFullName;
	bool              m_localOnly;
	// we cache the fullname otherwise our d'tor cannot delete the item from the GameTokenSystem
	// because before the d'tor is called the library smart ptr is set to zero sometimes
	// (see CBaseLibrary::RemoveAllItems())
	// this whole inconsistency exists because we have cyclic dependencies
	// CBaseLibraryItem and CBaseLibrary using smart pointers
};

#endif // __GameTokenItem_h__

