// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc2/Script/IScriptRegistry.h>

#include "ScriptFile.h"

namespace Schematyc2
{
	class CScriptRoot;

	//////////////////////////////////////////////////

	class CNewScriptFile : public INewScriptFile // #SchematycTODO : Move to separate file?
	{
	private:

		typedef std::vector<IScriptElement*> Elements;

	public:

		CNewScriptFile(const SGUID& guid, const char* szName);

		// INewScriptFile
		virtual SGUID GetGUID() const override;
		virtual const char* GetName() const override;
		virtual const CTimeValue& GetTimeStamp() const override;
		virtual void AddElement(IScriptElement& element) override;
		virtual void RemoveElement(IScriptElement& element) override;
		virtual uint32 GetElementCount() const override;
		// ~INewScriptFile

	private:

		SGUID      m_guid;
		string     m_name;
		CTimeValue m_timeStamp;
		Elements   m_elements;
	};

	DECLARE_SHARED_POINTERS(CNewScriptFile)

	//////////////////////////////////////////////////

	class CScriptRegistry : public IScriptRegistry
	{
	public:

		CScriptRegistry();

		// IScriptRegistry

		// Compatibility interface.
		//////////////////////////////////////////////////
		virtual IScriptFile* LoadFile(const char* szFileName) override;
		virtual IScriptFile* CreateFile(const char* szFileName, EScriptFileFlags flags = EScriptFileFlags::None) override;
		virtual IScriptFile* GetFile(const SGUID& guid) override;
		virtual IScriptFile* GetFile(const char* szFileName) override;
		virtual void VisitFiles(const ScriptFileVisitor& visitor, const char* szFilePath = nullptr) override;
		virtual void VisitFiles(const ScriptFileConstVisitor& visitor, const char* szFilePath = nullptr) const override;
		virtual void RefreshFiles(const SScriptRefreshParams& params) override;
		virtual bool Load() override;
		virtual void Save(bool bAlwaysSave = false) override;

		// New interface.
		//////////////////////////////////////////////////
		virtual IScriptModule* AddModule(const char* szName, IScriptElement* pScope = nullptr) override;
		virtual IScriptEnumeration* AddEnumeration(const char* szName, IScriptElement* pScope = nullptr) override;
		virtual IScriptFunction* AddFunction(const char* szName, IScriptElement* pScope = nullptr) override;
		virtual IScriptClass* AddClass(const char* szName, const SGUID& foundationGUID, IScriptElement* pScope = nullptr) override;
		virtual IScriptElement* GetElement(const SGUID& guid) override;
		virtual void RemoveElement(const SGUID& guid) override;
		virtual EVisitStatus VisitElements(const ScriptElementVisitor& visitor, IScriptElement* pScope = nullptr, EVisitFlags flags = EVisitFlags::None) override;
		virtual EVisitStatus VisitElements(const ScriptElementConstVisitor& visitor, const IScriptElement* pScope = nullptr, EVisitFlags flags = EVisitFlags::None) const override;
		virtual bool IsElementNameUnique(const char* szName, IScriptElement* pScope = nullptr) const override;
		virtual SScriptRegistrySignals& Signals() override;

		// ~IScriptRegistry

	private:

		typedef std::unordered_map<uint32, CScriptFilePtr>   FilesByCRC;
		typedef std::unordered_map<SGUID, CScriptFilePtr>    FilesByGUID;
		typedef std::unordered_map<SGUID, CNewScriptFilePtr> NewFiles;
		typedef std::unordered_map<SGUID, IScriptElementPtr> Elements; // #SchematycTODO : Would it make more sense to store by raw pointer here and make ownership exclusive to script file?

		CScriptFilePtr CreateFile(const char* szFileName, const SGUID& guid, EScriptFileFlags flags);
		void FormatFileName(string& fileName);
		void EnumFile(const char* szFileName, unsigned attributes);
		void RegisterFileByGuid(const CScriptFilePtr& pFile);
		
		INewScriptFile* CreateNewFile(const char* szFileName, const SGUID& guid);
		INewScriptFile* CreateNewFile(const char* szFileName);
		INewScriptFile* GetNewFile(const SGUID& guid);

		void AddElement(const IScriptElementPtr& pElement, IScriptElement& scope);
		void RemoveElement(IScriptElement& element);
		void SaveElementRecursive(IScriptElement& element, const char* szPath);

		FilesByCRC                   m_filesByCRC;
		FilesByGUID                  m_filesByGUID;
		NewFiles                     m_newFiles;
		Elements                     m_elements;
		std::shared_ptr<CScriptRoot> m_pRoot; // #SchematycTODO : Why can't we use std::unique_ptr?
		SScriptRegistrySignals       m_signals;
	};
}
