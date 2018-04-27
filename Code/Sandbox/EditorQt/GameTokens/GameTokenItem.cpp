// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "GameTokenItem.h"
#include "GameEngine.h"
#include "GameTokenLibrary.h"
#include "GameTokenManager.h"
#include <CryGame/IGameTokens.h>

class CFlowDataReadVisitorEditor
{
public:
	CFlowDataReadVisitorEditor(const char* data) : m_data(data), m_ok(false) {}

	void Visit(int& i)
	{
		m_ok = 1 == sscanf(m_data, "%i", &i);
	}

	void Visit(float& i)
	{
		m_ok = 1 == sscanf(m_data, "%f", &i);
	}

	void Visit(EntityId& i)
	{
		m_ok = 1 == sscanf(m_data, "%u", &i);
	}

	void Visit(Vec3& i)
	{
		m_ok = 3 == sscanf(m_data, "%g,%g,%g", &i.x, &i.y, &i.z);
	}

	void Visit(string& i)
	{
		i = m_data;
		m_ok = true;
	}

	void Visit(bool& b)
	{
		int i;
		m_ok = 1 == sscanf(m_data, "%i", &i);
		if (m_ok)
			b = (i != 0);
		else
		{
			if (stricmp(m_data, "true") == 0)
			{
				m_ok = b = true;
			}
			else if (stricmp(m_data, "false") == 0)
			{
				m_ok = true;
				b = false;
			}
		}
	}

	void Visit(SFlowSystemVoid&)
	{
	}

	template<class T>
	void operator()(T& type)
	{
		Visit(type);
	}

	void operator()(TFlowInputDataVariant& var)
	{
		VisitVariant(var);
	}

	bool operator!() const
	{
		return !m_ok;
	}

private:
	template<size_t I = 0>
	void VisitVariant(TFlowInputDataVariant& var)
	{
		if (var.index() == I)
		{
			Visit(stl::get<I>(var));
		}
		else
		{
			VisitVariant<I + 1>(var);
		}
	}

	template<>
	void VisitVariant<stl::variant_size<TFlowInputDataVariant>::value>(TFlowInputDataVariant& var)
	{
		CRY_ASSERT_MESSAGE(false, "Invalid variant index.");
	}

	const char* m_data;
	bool        m_ok;
};

//////////////////////////////////////////////////////////////////////////
class CFlowDataWriteVisitorEditor
{
public:
	CFlowDataWriteVisitorEditor(string& out) : m_out(out) {}

	void Visit(int i)
	{
		m_out.Format("%i", i);
	}

	void Visit(float i)
	{
		m_out.Format("%f", i);
	}

	void Visit(EntityId i)
	{
		m_out.Format("%u", i);
	}

	void Visit(Vec3 i)
	{
		m_out.Format("%g,%g,%g", i.x, i.y, i.z);
	}

	void Visit(const string& i)
	{
		m_out = i;
	}

	void Visit(bool b)
	{
		m_out = (b) ? "true" : "false";
	}

	void Visit(SFlowSystemVoid)
	{
		m_out = "";
	}

	template<class T>
	void operator()(T& type)
	{
		Visit(type);
	}

	void operator()(const TFlowInputDataVariant& var)
	{
		VisitVariant(var);
	}

private:
	template<size_t I = 0>
	void VisitVariant(const TFlowInputDataVariant& var)
	{
		if (var.index() == I)
		{
			Visit(stl::get<I>(var));
		}
		else
		{
			VisitVariant<I + 1>(var);
		}
	}

	template<>
	void VisitVariant<stl::variant_size<TFlowInputDataVariant>::value>(const TFlowInputDataVariant& var)
	{
		CRY_ASSERT_MESSAGE(false, "Invalid variant index.");
	}

	string& m_out;
};

//////////////////////////////////////////////////////////////////////////
inline string ConvertFlowDataToString(const TFlowInputData& value)
{
	string out;
	CFlowDataWriteVisitorEditor writer(out);
	value.Visit(writer);
	return out;
}

//////////////////////////////////////////////////////////////////////////
inline bool SetFlowDataFromString(TFlowInputData& value, const char* str)
{
	CFlowDataReadVisitorEditor visitor(str);
	value.Visit(visitor);
	return !!visitor;
}

//////////////////////////////////////////////////////////////////////////
CGameTokenItem::CGameTokenItem()
	: m_localOnly(true)
{
	m_pTokenSystem = GetIEditorImpl()->GetGameEngine()->GetIGameTokenSystem();
	m_value = TFlowInputData((bool)0, true);
}

//////////////////////////////////////////////////////////////////////////
CGameTokenItem::~CGameTokenItem()
{
	if (m_pTokenSystem)
	{
		IGameToken* pToken = m_pTokenSystem->FindToken(m_cachedFullName);
		if (pToken)
			m_pTokenSystem->DeleteToken(pToken);
	}
}

//////////////////////////////////////////////////////////////////////////
void CGameTokenItem::SetName(const string& name)
{
	IGameToken* pToken = NULL;
	if (m_pTokenSystem)
		pToken = m_pTokenSystem->FindToken(GetFullName());
	__super::SetName(name);
	if (m_pTokenSystem)
	{
		if (pToken)
			m_pTokenSystem->RenameToken(pToken, GetFullName());
		else
			m_pTokenSystem->SetOrCreateToken(GetFullName(), m_value);
	}
	m_cachedFullName = GetFullName();
}

//////////////////////////////////////////////////////////////////////////
void CGameTokenItem::Serialize(SerializeContext& ctx)
{
	//__super::Serialize( ctx );
	XmlNodeRef node = ctx.node;
	if (ctx.bLoading)
	{
		const char* sTypeName = node->getAttr("Type");
		const char* sDefaultValue = node->getAttr("Value");

		if (!SetTypeName(sTypeName))
			return;
		SetFlowDataFromString(m_value, sDefaultValue);

		string name = m_name;
		// Loading
		node->getAttr("Name", name);

		// SetName will also set new value
		if (!ctx.bUniqName)
		{
			SetName(name);
		}
		else
		{
			SetName(GetLibrary()->GetManager()->MakeUniqItemName(name));
		}
		node->getAttr("Description", m_description);

		int localOnly = 0;
		node->getAttr("LocalOnly", localOnly);
		SetLocalOnly(localOnly != 0);
	}
	else
	{
		node->setAttr("Name", m_name);
		// Saving.
		const char* sTypeName = FlowTypeToName((EFlowDataTypes)m_value.GetType());
		if (*sTypeName != 0)
			node->setAttr("Type", sTypeName);
		string sValue = ConvertFlowDataToString(m_value);
		node->setAttr("Value", sValue);
		if (!m_description.IsEmpty())
			node->setAttr("Description", m_description);

		int localOnly = m_localOnly ? 1 : 0;
		node->setAttr("LocalOnly", localOnly);
	}
	m_bModified = false;
}

//////////////////////////////////////////////////////////////////////////
string CGameTokenItem::GetTypeName() const
{
	return FlowTypeToName((EFlowDataTypes)m_value.GetType());
}

//////////////////////////////////////////////////////////////////////////
string CGameTokenItem::GetValueString() const
{
	return (const char*)ConvertFlowDataToString(m_value);
}

//////////////////////////////////////////////////////////////////////////
void CGameTokenItem::SetValueString(const char* sValue)
{
	// Skip if the same type already.
	if (0 == strcmp(GetValueString(), sValue))
		return;

	SetFlowDataFromString(m_value, sValue);
}

//////////////////////////////////////////////////////////////////////////
bool CGameTokenItem::GetValue(TFlowInputData& data) const
{
	data = m_value;
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CGameTokenItem::SetValue(const TFlowInputData& data, bool bUpdateGTS)
{
	m_value = data;
	if (bUpdateGTS && m_pTokenSystem)
	{
		IGameToken* pToken = m_pTokenSystem->FindToken(m_cachedFullName);
		if (pToken)
			pToken->SetValue(m_value);
	}
}

//////////////////////////////////////////////////////////////////////////
bool CGameTokenItem::SetTypeName(const char* typeName)
{
	EFlowDataTypes tokenType = FlowNameToType(typeName);

	// Skip if the same type already.
	if (tokenType == m_value.GetType())
		return true;

	string prevVal = GetValueString();
	switch (tokenType)
	{
	case eFDT_Int:
		m_value = TFlowInputData((int)0, true);
		break;
	case eFDT_Float:
		m_value = TFlowInputData((float)0, true);
		break;
	case eFDT_Vec3:
		m_value = TFlowInputData(Vec3(0, 0, 0), true);
		break;
	case eFDT_String:
		m_value = TFlowInputData(string(""), true);
		break;
	case eFDT_Bool:
		m_value = TFlowInputData((bool)false, true);
		break;
	default:
		// Unknown type.
		return false;
	}
	SetValueString(prevVal);
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CGameTokenItem::Update(bool bRecreate)
{
	// Mark library as modified.
	SetModified();

	if (m_pTokenSystem)
	{
		// Recreate the game token with new default value, and set flags
		const string& fullName = GetFullName();
		if (bRecreate)
		{
			if (IGameToken *pToken = m_pTokenSystem->FindToken(fullName))
			{
				m_pTokenSystem->DeleteToken(pToken);
			}
		}

		if (IGameToken *pToken=m_pTokenSystem->SetOrCreateToken(fullName, m_value))
		{
			ApplyFlags();
		}
	}
}

void CGameTokenItem::ApplyFlags()
{
	if (IGameToken *pToken = m_pTokenSystem->FindToken(GetFullName()))
	{
		if (m_localOnly)
			pToken->SetFlags(pToken->GetFlags()|EGAME_TOKEN_LOCALONLY);
		else
			pToken->SetFlags(pToken->GetFlags()&~EGAME_TOKEN_LOCALONLY);
	}
}

