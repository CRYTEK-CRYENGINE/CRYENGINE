// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#pragma once

#include <CryString/HashedString.h>
#include <CryDynamicResponseSystem/IDynamicResponseAction.h>

namespace CryDRS
{
class CActionSetGameToken final : public DRS::IResponseAction
{
public:
	CActionSetGameToken() : m_bCreateTokenIfNotExisting(false) {}
	CActionSetGameToken(const string& tokenName, const string& stringValue, bool bCreate) : m_tokenName(tokenName), m_bCreateTokenIfNotExisting(bCreate) {}

	//////////////////////////////////////////////////////////
	// IResponseAction implementation
	virtual DRS::IResponseActionInstanceUniquePtr Execute(DRS::IResponseInstance* pResponseInstance) override;
	virtual string                                GetVerboseInfo() const override { return "Token: '" + m_tokenName + "' to value '" + m_valueToSet + "'"; }
	virtual void                                  Serialize(Serialization::IArchive& ar) override;
	virtual const char*                           GetType() const override        { return "Set GameToken Value"; }
	//////////////////////////////////////////////////////////

private:
	string m_tokenName;
	string m_valueToSet;
	bool   m_bCreateTokenIfNotExisting;
};
}  //namespace CryDRS
