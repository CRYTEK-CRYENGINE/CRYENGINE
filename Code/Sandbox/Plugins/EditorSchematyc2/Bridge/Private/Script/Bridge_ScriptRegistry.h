// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Bridge_ScriptFile.h"
#include "Bridge_IScriptRegistry.h"

namespace Bridge {

class CScriptRegistry : public IScriptRegistry
{
public:

	// IScriptRegistry

	// Compatibility interface.
	//////////////////////////////////////////////////        
	virtual IScriptFile* LoadFile(const char* szFileName) override;
	virtual IScriptFile* CreateFile(const char* szFileName, Schematyc2::EScriptFileFlags flags = Schematyc2::EScriptFileFlags::None) override;
	virtual IScriptFile* GetFile(const Schematyc2::SGUID& guid) override;
	virtual IScriptFile* GetFile(const char* szFileName) override;
	virtual void VisitFiles(const ScriptFileVisitor& visitor, const char* szFilePath = nullptr) override;
	virtual void VisitFiles(const ScriptFileConstVisitor& visitor, const char* szFilePath = nullptr) const override;
	virtual void RefreshFiles(const Schematyc2::SScriptRefreshParams& params) override;
	virtual bool Load() override;
	virtual void Save(bool bAlwaysSave = false) override;

	// New interface.
	//////////////////////////////////////////////////
	virtual Schematyc2::IScriptModule* AddModule(const char* szName, Schematyc2::IScriptElement* pScope = nullptr) override;
	virtual Schematyc2::IScriptEnumeration* AddEnumeration(const char* szName, Schematyc2::IScriptElement* pScope = nullptr) override;
	virtual Schematyc2::IScriptFunction* AddFunction(const char* szName, Schematyc2::IScriptElement* pScope = nullptr) override;
	virtual Schematyc2::IScriptClass* AddClass(const char* szName, const Schematyc2::SGUID& foundationGUID, Schematyc2::IScriptElement* pScope = nullptr) override;
	virtual Schematyc2::IScriptElement* GetElement(const Schematyc2::SGUID& guid) override;
	virtual void RemoveElement(const Schematyc2::SGUID& guid) override;
	virtual Schematyc2::EVisitStatus VisitElements(const Schematyc2::ScriptElementVisitor& visitor, Schematyc2::IScriptElement* pScope = nullptr, Schematyc2::EVisitFlags flags = Schematyc2::EVisitFlags::None) override;
	virtual Schematyc2::EVisitStatus VisitElements(const Schematyc2::ScriptElementConstVisitor& visitor, const Schematyc2::IScriptElement* pScope = nullptr, Schematyc2::EVisitFlags flags = Schematyc2::EVisitFlags::None) const override;
	virtual bool IsElementNameUnique(const char* szName, Schematyc2::IScriptElement* pScope = nullptr) const override;
	virtual Schematyc2::SScriptRegistrySignals& Signals() override;
	virtual Bridge::IScriptFile* Wrapfile(Schematyc2::IScriptFile* file) override;
	// ~IScriptRegistry

private:
		
	Schematyc2::IScriptRegistry* Delegate() const;
		
private:
	typedef std::unordered_map<Schematyc2::IScriptFile*, CScriptFilePtr> FilesBySchematycFiles;
	FilesBySchematycFiles m_filesBySchematycFiles;
};

}