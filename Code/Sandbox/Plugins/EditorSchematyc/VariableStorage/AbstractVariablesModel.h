// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "AbstractVariableTypesModel.h"

class CVariableItem
{
public:
	CVariableItem();
	~CVariableItem();

	QString GetName() const { return m_name; }
	void    SetName(QString name);
	//CTypeItem& GetTypeItem() const;

	bool IsConst() const  { m_isConst; }
	bool IsLocal() const  { m_isLocal; }
	bool IsGlobal() const { m_isGlobal; }

	void Serialize(Serialization::IArchive& archive);

private:
	QString m_name;

	bool    m_isConst  : 1;
	bool    m_isLocal  : 1;
	bool    m_isGlobal : 1;
};

class CAbstractVariablesViewModel
{
public:
	CAbstractVariablesViewModel() {}
	virtual ~CAbstractVariablesViewModel() {}

	virtual uint32         GetNumVariables() const              { return 0; }
	virtual CVariableItem* GetVariableItemByIndex(uint32 index) { return nullptr; }
	virtual CVariableItem* CreateVariable()                     { return nullptr; }
	virtual bool           RemoveVariable()                     { return false; }
};

