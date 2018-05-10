// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc/Script/IScript.h>
#include <CrySchematyc/Utils/StackString.h>

namespace Schematyc
{
class CScript : public IScript
{
	typedef std::vector<IScriptElement*> Elements;

public:
	CScript(const CryGUID& guid, const char* szFilePath);
	CScript(const char* szFilePath);

	// IScript
	virtual CryGUID           GetGUID() const override;

	virtual const char*       SetFilePath(const char* szFilePath) override;
	virtual const char*       GetFilePath() const override { return m_filePath.c_str(); }

	virtual const CTimeValue& GetTimeStamp() const override;

	virtual void              SetRoot(IScriptElement* pRoot) override;
	virtual IScriptElement*   GetRoot() override;

	virtual EVisitStatus      VisitElements(const ScriptElementVisitor& visitor) override;
	// ~IScript

private:
	EVisitStatus VisitElementsRecursive(const ScriptElementVisitor& visitor, IScriptElement& element);
	void         SetNameFromRootRecursive(CStackString& name, IScriptElement& element);

	CryGUID         m_guid;
	string          m_filePath;
	CTimeValue      m_timeStamp;
	IScriptElement* m_pRoot;
};

DECLARE_SHARED_POINTERS(CScript)
}
