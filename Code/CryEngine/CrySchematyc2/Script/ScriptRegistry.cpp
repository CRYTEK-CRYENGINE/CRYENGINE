// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Script/ScriptRegistry.h"

#include <CryCore/CryCrc32.h>
#include <CrySerialization/Forward.h>
#include <CrySerialization/IArchiveHost.h>
#include <CrySystem/File/ICryPak.h>

#include "CVars.h"
#include "Script/ScriptSerializationUtils.h"
#include "Script/Elements/ScriptFunction.h"
#include "Script/Elements/ScriptModule.h"
#include "Script/Elements/ScriptRoot.h"
#include "Serialization/SerializationContext.h"
#include "Utils/FileUtils.h"

namespace Schematyc2
{
	namespace ScriptRegistryUtils
	{
		static CScriptFile g_dummyScriptFile("", SGUID(), EScriptFileFlags::None); // #SchematycTODO : Can we avoid creating a dummy script file just to reference from the root element?

		inline bool IsFileElement(EScriptElementType elementType)
		{
			switch(elementType)
			{
			case EScriptElementType::Module:
			case EScriptElementType::Class:
				{
					return true;
				}
			default:
				{
					return false;
				}
			}
		}

		void SaveAllScriptFilesCommand(IConsoleCmdArgs* pArgs)
		{
			gEnv->pSchematyc2->GetScriptRegistry().Save(true);
		}
	}

	//////////////////////////////////////////////////

	CNewScriptFile::CNewScriptFile(const SGUID& guid, const char* szName)
		: m_guid(guid)
		, m_name(szName)
		, m_timeStamp(gEnv->pTimer->GetAsyncTime())
	{}

	SGUID CNewScriptFile::GetGUID() const
	{
		return m_guid;
	}

	const char* CNewScriptFile::GetName() const
	{
		return m_name;
	}

	const CTimeValue& CNewScriptFile::GetTimeStamp() const
	{
		return m_timeStamp;
	}

	void CNewScriptFile::AddElement(IScriptElement& element)
	{
		m_elements.push_back(&element);
	}

	void CNewScriptFile::RemoveElement(IScriptElement& element)
	{
		m_elements.erase(std::remove(m_elements.begin(), m_elements.end(), &element), m_elements.end());
	}

	uint32 CNewScriptFile::GetElementCount() const
	{
		return m_elements.size();
	}

	//////////////////////////////////////////////////

	CScriptRegistry::CScriptRegistry()
	{
		m_pRoot.reset(new CScriptRoot(ScriptRegistryUtils::g_dummyScriptFile));
		REGISTER_COMMAND("sc_SaveAllScriptFiles", ScriptRegistryUtils::SaveAllScriptFilesCommand, VF_NULL, "Save all Schematyc script file regardless of whether they have been modified");
	}

	IScriptFile* CScriptRegistry::LoadFile(const char* szFileName)
	{
		SCHEMATYC2_SYSTEM_ASSERT(szFileName);
		if(szFileName)
		{
			string fileNameFormatted = szFileName;
			FormatFileName(fileNameFormatted);
			string fileNameLowerCase = fileNameFormatted;
			fileNameLowerCase.MakeLower();
			const uint32         crc32 = CCrc32::Compute(fileNameFormatted.c_str());
			FilesByCRC::iterator itFile = m_filesByCRC.find(crc32);
			if(itFile != m_filesByCRC.end())
			{
				return itFile->second.get();
			}
			if(gEnv->pCryPak->IsFileExist(fileNameFormatted.c_str()))
			{
				const bool     bOnDisk = gEnv->pCryPak->IsFileExist(fileNameFormatted.c_str(), ICryPak::eFileLocation_OnDisk);
				CScriptFilePtr pFile = CreateFile(fileNameFormatted.c_str(), SGUID(), bOnDisk ? EScriptFileFlags::OnDisk : EScriptFileFlags::None);
				if(pFile)
				{
					pFile->Load();
					RegisterFileByGuid(pFile);
					return pFile.get();
				}
			}
		}
		return nullptr;
	}

	IScriptFile* CScriptRegistry::CreateFile(const char* szFileName, EScriptFileFlags flags)
	{
		SCHEMATYC2_SYSTEM_ASSERT(gEnv->IsEditor());
		if(gEnv->IsEditor())
		{
			const SGUID    guid = gEnv->pSchematyc2->CreateGUID();
			CScriptFilePtr pFile = CreateFile(szFileName, guid, flags | EScriptFileFlags::OnDisk);
			if(pFile)
			{
				m_filesByGUID.insert(FilesByGUID::value_type(guid, pFile));
				return pFile.get();
			}
		}
		return nullptr;
	}

	IScriptFile* CScriptRegistry::GetFile(const SGUID& guid)
	{
		FilesByGUID::iterator itFile = m_filesByGUID.find(guid);
		return itFile != m_filesByGUID.end() ? itFile->second.get() : nullptr;
	}

	IScriptFile* CScriptRegistry::GetFile(const char* szFileName)
	{
		SCHEMATYC2_SYSTEM_ASSERT(szFileName);
		if(szFileName)
		{
			string fileNameFormatted = szFileName;
			FormatFileName(fileNameFormatted);
			fileNameFormatted.MakeLower();
			const uint32         crc32 = CCrc32::Compute(fileNameFormatted.c_str());
			FilesByCRC::iterator itFile = m_filesByCRC.find(crc32);
			if(itFile != m_filesByCRC.end())
			{
				const CScriptFilePtr& pFile = itFile->second;
				if(stricmp(fileNameFormatted.c_str(), pFile->GetFileName()) == 0)
				{
					return pFile.get();
				}
			}
		}
		return nullptr;
	}

	void CScriptRegistry::VisitFiles(const ScriptFileVisitor& visitor, const char* szFilePath)
	{
		SCHEMATYC2_SYSTEM_ASSERT(visitor);
		if(visitor)
		{
			for(FilesByCRC::iterator itFile = m_filesByCRC.begin(), itEndFile = m_filesByCRC.end(); itFile != itEndFile; ++ itFile)
			{
				IScriptFile& file = *itFile->second;
				if(!szFilePath || CryStringUtils::stristr(file.GetFileName(), szFilePath))
				{
					if(visitor(file) != EVisitStatus::Continue)
					{
						return;
					}
				}
			}
		}
	}

	void CScriptRegistry::VisitFiles(const ScriptFileConstVisitor& visitor, const char* szFilePath) const
	{
		SCHEMATYC2_SYSTEM_ASSERT(visitor);
		if(visitor)
		{
			for(FilesByCRC::const_iterator itFile = m_filesByCRC.begin(), itEndFile = m_filesByCRC.end(); itFile != itEndFile; ++ itFile)
			{
				const IScriptFile& file = *itFile->second;
				if(!szFilePath || CryStringUtils::stristr(file.GetFileName(), szFilePath))
				{
					if(visitor(file) != EVisitStatus::Continue)
					{
						return;
					}
				}
			}
		}
	}

	void CScriptRegistry::RefreshFiles(const SScriptRefreshParams& params)
	{
		for(FilesByCRC::iterator itFile = m_filesByCRC.begin(), itEndFile = m_filesByCRC.end(); itFile != itEndFile; ++ itFile)
		{
			itFile->second->Refresh(params);
		}
	}

	//////////
	
	struct SScriptInputElement;

	typedef std::vector<SScriptInputElement>  ScriptInputElements;
	typedef std::vector<SScriptInputElement*> ScriptInputElementPtrs;
	
	struct SScriptInputElement
	{
		inline SScriptInputElement()
			: sortPriority(0)
		{}

		void Serialize(Serialization::IArchive& archive)
		{
			if(!ptr)
			{
				EScriptElementType elementType = EScriptElementType::None;
				archive(elementType, "elementType");
				ptr = DocSerializationUtils::g_scriptElementFactory.CreateElement(ScriptRegistryUtils::g_dummyScriptFile, elementType);
				if(ptr)
				{
					children.reserve(20);
					archive(children, "children");
					for(SScriptInputElement& child : children)
					{
						if(child.ptr)
						{
							ptr->AttachChild(*child.ptr);
						}
					}
				}
			}
		}

		Serialization::SBlackBox blackBox;
		IScriptElementPtr        ptr;
		ScriptInputElements      children;
		ScriptInputElementPtrs   dependencies;
		uint32                   sortPriority;
	};

	class CScriptInputElementSerializer
	{
	public:

		inline CScriptInputElementSerializer(IScriptElement& element, ESerializationPass serializationPass)
			: m_element(element)
			, m_serializationPass(serializationPass)
		{}

		void Serialize(Serialization::IArchive& archive)
		{
			CSerializationContext serializationContext(SSerializationContextParams(archive, m_serializationPass));
			m_element.Serialize(archive);
		}

	private:

		IScriptElement&    m_element;
		ESerializationPass m_serializationPass;
	};

	bool Serialize(Serialization::IArchive& archive, SScriptInputElement& value, const char* szName, const char*)
	{
		archive(value.blackBox, szName);
		Serialization::LoadBlackBox(value, value.blackBox);
		return true;
	}

	struct SScriptInputBlock
	{
		SGUID               guid;
		SGUID               scopeGUID;
		SScriptInputElement rootElement;
	};

	typedef std::vector<SScriptInputBlock> ScriptInputBlocks;

	class CScriptInputBlockSerializer
	{
	public:

		inline CScriptInputBlockSerializer(SScriptInputBlock& block)
			: m_block(block)
		{}

		void Serialize(Serialization::IArchive& archive)
		{
			uint32 versionNumber = 0;
			archive(versionNumber, "version");
			// #SchematycTODO : Check version number!
			{
				archive(m_block.guid, "guid");
				archive(m_block.scopeGUID, "scope");
				archive(m_block.rootElement, "root");
			}
		}

	private:

		SScriptInputBlock& m_block;
	};

	void UnrollScriptInputElementsRecursive(ScriptInputElementPtrs& output, SScriptInputElement &element)
	{
		if(element.ptr)
		{
			output.push_back(&element);
		}
		for(SScriptInputElement& childElement : element.children)
		{
			UnrollScriptInputElementsRecursive(output, childElement);
		}
	}

	bool SortScriptInputElementsByDependency(ScriptInputElementPtrs& elements)
	{
		typedef std::unordered_map<SGUID, SScriptInputElement*> ElementsByGUID;

		ElementsByGUID elementsByGUID;
		const size_t   elementCount = elements.size();
		elementsByGUID.reserve(elementCount);
		for(SScriptInputElement* pElement : elements)
		{
			elementsByGUID.insert(ElementsByGUID::value_type(pElement->ptr->GetGUID(), pElement));
		}

		for(SScriptInputElement* pElement : elements)
		{
			auto visitElementDependency = [&elementsByGUID, pElement] (const SGUID& guid)
			{
				ElementsByGUID::const_iterator itDependency = elementsByGUID.find(guid);
				if(itDependency != elementsByGUID.end())
				{
					pElement->dependencies.push_back(itDependency->second);
				}
			};
			pElement->ptr->EnumerateDependencies(ScriptDependancyEnumerator::FromLambdaFunction(visitElementDependency));
		}

		size_t recursiveDependencyCount = 0;
		for(SScriptInputElement* pElement : elements)
		{
			for(SScriptInputElement* pDependencyElement : pElement->dependencies)
			{
				if(std::find(pDependencyElement->dependencies.begin(), pDependencyElement->dependencies.end(), pElement) != pDependencyElement->dependencies.end())
				{
					++ recursiveDependencyCount;
				}
			}
		}
		if(recursiveDependencyCount)
		{
			SCHEMATYC2_SYSTEM_CRITICAL_ERROR("%d recursive dependencies detected!", recursiveDependencyCount);
			return false;
		}

		bool bShuffle;
		do
		{
			bShuffle = false;
			for(SScriptInputElement* pElement : elements)
			{
				for(SScriptInputElement* pDependencyElement : pElement->dependencies)
				{
					if(pDependencyElement->sortPriority <= pElement->sortPriority)
					{
						pDependencyElement->sortPriority = pElement->sortPriority + 1;
						bShuffle                         = true;
					}
				}
			}
		} while(bShuffle);

		auto compareElements = [] (const SScriptInputElement* lhs, const SScriptInputElement* rhs) -> bool
		{
			return lhs->sortPriority > rhs->sortPriority;
		};
		std::sort(elements.begin(), elements.end(), compareElements);
		return true;
	}
	//////////

	bool CScriptRegistry::Load()
	{
		LOADING_TIME_PROFILE_SECTION;
		// Load old script files.
		{
			stack_string extension = "*.";
			extension.append(gEnv->pSchematyc2->GetOldScriptExtension());
			FileUtils::EFileEnumFlags fileEnumFlags = FileUtils::EFileEnumFlags::Recursive;
			if(CVars::sc_IgnoreUnderscoredFolders)
			{
				fileEnumFlags |= FileUtils::EFileEnumFlags::IgnoreUnderscoredFolders;
			}
			FileUtils::EnumFilesInFolder(gEnv->pSchematyc2->GetOldScriptsFolder(), extension.c_str(), FileUtils::FileEnumCallback::FromMemberFunction<CScriptRegistry, &CScriptRegistry::EnumFile>(*this), fileEnumFlags);
			for(FilesByCRC::iterator itFile = m_filesByCRC.begin(), itEndFile = m_filesByCRC.end(); itFile != itEndFile; ++ itFile)
			{
				itFile->second->Refresh(SScriptRefreshParams(EScriptRefreshReason::Load));
			}
		}
		// Load new script files.
		// #SchematycTODO : How do we avoid loading files twice?
		// #SchematycTODO : Set file info for each element!
		if(gEnv->pSchematyc2->IsExperimentalFeatureEnabled("QtEditor"))
		{
			// Configure file enumeration flags.
			FileUtils::EFileEnumFlags fileEnumFlags = FileUtils::EFileEnumFlags::Recursive;
			if(CVars::sc_IgnoreUnderscoredFolders)
			{
				fileEnumFlags |= FileUtils::EFileEnumFlags::IgnoreUnderscoredFolders;
			}
			// Enumerate files and construct new elements.
			// #SchematycTODO : How can we minimize some of the duplication between loadJsonFile and loadXmlFile?
			ScriptInputBlocks blocks;
			auto loadJsonFile = [this, &blocks] (const char* szFileName, unsigned attributes)
			{
				SScriptInputBlock           block;
				CScriptInputBlockSerializer blockSerializer(block);
				Serialization::LoadJsonFile(blockSerializer, szFileName);
				if(block.guid && block.rootElement.ptr)
				{
					INewScriptFile* pNewFile = GetNewFile(block.guid);
					if(!pNewFile)
					{
						pNewFile = CreateNewFile(szFileName, block.guid);
						pNewFile->AddElement(*block.rootElement.ptr);
						block.rootElement.ptr->SetNewFile(pNewFile);
						blocks.push_back(std::move(block));
					}
				}
			};
			auto loadXmlFile = [this, &blocks] (const char* szFileName, unsigned attributes)
			{
				SScriptInputBlock           block;
				CScriptInputBlockSerializer blockSerializer(block);
				Serialization::LoadXmlFile(blockSerializer, szFileName);
				if(block.guid && block.rootElement.ptr)
				{
					INewScriptFile* pNewFile = GetNewFile(block.guid);
					if(!pNewFile)
					{
						pNewFile = CreateNewFile(szFileName, block.guid);
						pNewFile->AddElement(*block.rootElement.ptr);
						block.rootElement.ptr->SetNewFile(pNewFile);
						blocks.push_back(std::move(block));
					}
				}
			};
			const char* szScriptFolder = gEnv->pSchematyc2->GetScriptsFolder();
			FileUtils::EnumFilesInFolder(szScriptFolder, "*.json", FileUtils::FileEnumCallback::FromLambdaFunction(loadJsonFile), fileEnumFlags);
			FileUtils::EnumFilesInFolder(szScriptFolder, "*.xml", FileUtils::FileEnumCallback::FromLambdaFunction(loadXmlFile), fileEnumFlags);
			// #SchematycTODO : Create ProcessInputBlocks() function!
			{
				// Unroll element blocks.
				ScriptInputElementPtrs elements;
				elements.reserve(100);
				for(SScriptInputBlock& block : blocks)
				{
					UnrollScriptInputElementsRecursive(elements, block.rootElement);
				}
				// Pre-load elements.
				for(SScriptInputElement* pElement : elements)
				{
					CScriptInputElementSerializer elementSerializer(*pElement->ptr, ESerializationPass::PreLoad);
					Serialization::LoadBlackBox(elementSerializer, pElement->blackBox);
				}
				if(SortScriptInputElementsByDependency(elements))
				{
					// Load elements.
					for(SScriptInputElement* pElement : elements)
					{
						CScriptInputElementSerializer elementSerializer(*pElement->ptr, ESerializationPass::Load);
						Serialization::LoadBlackBox(elementSerializer, pElement->blackBox);
						m_elements.insert(Elements::value_type(pElement->ptr->GetGUID(), pElement->ptr)); // #SchematycTODO : Make sure guid isn't duplicate!!!
					}
					// Attach elements.
					for(SScriptInputBlock& block : blocks)
					{
						IScriptElement* pScope = block.scopeGUID ? GetElement(block.scopeGUID) : m_pRoot.get();
						if(pScope)
						{
							pScope->AttachChild(*block.rootElement.ptr);
						}
						else
						{
							// #SchematycTODO : Error!!!
						}
					}
					// Post-load elements.
					for(SScriptInputElement* pElement : elements)
					{
						CScriptInputElementSerializer elementSerializer(*pElement->ptr, ESerializationPass::PostLoad);
						Serialization::LoadBlackBox(elementSerializer, pElement->blackBox);
					}
				}
			}
		}
		return true;
	}

	void CScriptRegistry::Save(bool bAlwaysSave)
	{
		// Save old script files.
		{
			for(FilesByCRC::iterator itFile = m_filesByCRC.begin(), itEndFile = m_filesByCRC.end(); itFile != itEndFile; ++ itFile)
			{
				CScriptFile& file = *itFile->second;
				if(bAlwaysSave || (((file.GetFlags() & EScriptFileFlags::OnDisk) != 0) && ((file.GetFlags() & EScriptFileFlags::Modified) != 0)))
				{
					file.Save();
				}
			}
		}
		// Save new script files.
		if(gEnv->pSchematyc2->IsExperimentalFeatureEnabled("QtEditor"))
		{
			const char* szPath = gEnv->pSchematyc2->GetScriptsFolder();
			for(IScriptElement* pElement = m_pRoot->GetFirstChild(); pElement; pElement = pElement->GetNextSibling())
			{
				SaveElementRecursive(*pElement, szPath);
			}
		}
	}

	IScriptModule* CScriptRegistry::AddModule(const char* szName, IScriptElement* pScope)
	{
		SCHEMATYC2_SYSTEM_ASSERT(gEnv->IsEditor() && szName);
		if(gEnv->IsEditor() && szName)
		{
			if(!pScope)
			{
				pScope = m_pRoot.get();
			}
			if(IsElementNameUnique(szName, pScope))
			{
				CScriptModulePtr pModule = std::make_shared<CScriptModule>(ScriptRegistryUtils::g_dummyScriptFile, gEnv->pSchematyc2->CreateGUID(), pScope->GetGUID(), szName);
				AddElement(pModule, *pScope);
				return pModule.get();
			}
		}
		return nullptr;
	}

	IScriptEnumeration* CScriptRegistry::AddEnumeration(const char* szName, IScriptElement* pScope)
	{
		SCHEMATYC2_SYSTEM_ASSERT(gEnv->IsEditor() && szName);
		if(gEnv->IsEditor() && szName)
		{
			if(!pScope)
			{
				pScope = m_pRoot.get();
			}
			if(IsElementNameUnique(szName, pScope))
			{
				CScriptEnumerationPtr pEnumeration = std::make_shared<CScriptEnumeration>(ScriptRegistryUtils::g_dummyScriptFile, gEnv->pSchematyc2->CreateGUID(), pScope->GetGUID(), szName);
				AddElement(pEnumeration, *pScope);
				return pEnumeration.get();
			}
		}
		return nullptr;
	}

	IScriptFunction* CScriptRegistry::AddFunction(const char* szName, IScriptElement* pScope)
	{
		SCHEMATYC2_SYSTEM_ASSERT(gEnv->IsEditor() && szName);
		if(gEnv->IsEditor() && szName)
		{
			if(!pScope)
			{
				pScope = m_pRoot.get();
			}
			if(IsElementNameUnique(szName, pScope))
			{
				CScriptFunctionPtr pFunction = std::make_shared<CScriptFunction>(ScriptRegistryUtils::g_dummyScriptFile, gEnv->pSchematyc2->CreateGUID(), pScope->GetGUID(), szName);
				AddElement(pFunction, *pScope);
				return pFunction.get();
			}
		}
		return nullptr;
	}

	IScriptClass* CScriptRegistry::AddClass(const char* szName, const SGUID& foundationGUID, IScriptElement* pScope)
	{
		SCHEMATYC2_SYSTEM_ASSERT(gEnv->IsEditor() && szName);
		if(gEnv->IsEditor() && szName)
		{
			if(!pScope)
			{
				pScope = m_pRoot.get();
			}
			if(IsElementNameUnique(szName, pScope))
			{
				CScriptClassPtr pClass = std::make_shared<CScriptClass>(ScriptRegistryUtils::g_dummyScriptFile, gEnv->pSchematyc2->CreateGUID(), pScope->GetGUID(), szName, foundationGUID);
				AddElement(pClass, *pScope);
				return pClass.get();
			}
		}
		return nullptr;
	}

	IScriptElement* CScriptRegistry::GetElement(const SGUID& guid)
	{
		Elements::iterator itElement = m_elements.find(guid);
		return itElement != m_elements.end() ? itElement->second.get() : nullptr;
	}

	void CScriptRegistry::RemoveElement(const SGUID& guid)
	{
		Elements::iterator itElement = m_elements.find(guid);
		if(itElement != m_elements.end())
		{
			RemoveElement(*itElement->second);
		}
	}

	EVisitStatus CScriptRegistry::VisitElements(const ScriptElementVisitor& visitor, IScriptElement* pScope, EVisitFlags flags)
	{
		SCHEMATYC2_SYSTEM_ASSERT(visitor);
		if(visitor)
		{
			if(!pScope)
			{
				pScope = m_pRoot.get();
			}
			for(IScriptElement* pElement = pScope->GetFirstChild(); pElement; pElement = pElement->GetNextSibling())
			{
				const EVisitStatus visitStatus = visitor(*pElement);
				if(visitStatus != EVisitStatus::Continue)
				{
					return visitStatus;
				}
				if((flags & EVisitFlags::RecurseHierarchy) != 0)
				{
					const EVisitStatus recurseStatus = VisitElements(visitor, pElement, flags);
					if(recurseStatus != EVisitStatus::End)
					{
						return recurseStatus;
					}
				}
			}
		}
		return EVisitStatus::End;
	}
	
	EVisitStatus CScriptRegistry::VisitElements(const ScriptElementConstVisitor& visitor, const IScriptElement* pScope, EVisitFlags flags) const
	{
		SCHEMATYC2_SYSTEM_ASSERT(visitor);
		if(visitor)
		{
			if(!pScope)
			{
				pScope = m_pRoot.get();
			}
			for(const IScriptElement* pElement = pScope->GetFirstChild(); pElement; pElement = pElement->GetNextSibling())
			{
				const EVisitStatus visitStatus = visitor(*pElement);
				if(visitStatus != EVisitStatus::Continue)
				{
					return visitStatus;
				}
				if((flags & EVisitFlags::RecurseHierarchy) != 0)
				{
					const EVisitStatus recurseStatus = VisitElements(visitor, pElement, flags);
					if(recurseStatus != EVisitStatus::End)
					{
						return recurseStatus;
					}
				}
			}
		}
		return EVisitStatus::End;
	}

	bool CScriptRegistry::IsElementNameUnique(const char* szName, IScriptElement* pScope) const
	{
		SCHEMATYC2_SYSTEM_ASSERT(szName);
		if(szName)
		{
			if(!pScope)
			{
				pScope = m_pRoot.get();
			}
			for(const IScriptElement* pElement = pScope->GetFirstChild(); pElement; pElement = pElement->GetNextSibling())
			{
				if(strcmp(pElement->GetName(), szName) == 0)
				{
					return false;
				}
			}
			return true;
		}
		return false;
	}

	SScriptRegistrySignals& CScriptRegistry::Signals()
	{
		return m_signals;
	}

	CScriptFilePtr CScriptRegistry::CreateFile(const char* szFileName, const SGUID& guid, EScriptFileFlags flags)
	{
		LOADING_TIME_PROFILE_SECTION(szFileName);
		SCHEMATYC2_SYSTEM_ASSERT(szFileName);
		if(szFileName)
		{
			string fileNameFormatted = szFileName;
			FormatFileName(fileNameFormatted);
			string fileNameLowerCase = fileNameFormatted;
			fileNameLowerCase.MakeLower();
			const uint32 crc32 = CCrc32::Compute(fileNameLowerCase.c_str());
			if(m_filesByCRC.find(crc32) == m_filesByCRC.end())
			{
				CScriptFilePtr pFile(new CScriptFile(fileNameFormatted.c_str(), guid, flags));
				m_filesByCRC.insert(FilesByCRC::value_type(crc32, pFile));
				return pFile;
			}
		}
		return CScriptFilePtr();
	}

	void CScriptRegistry::FormatFileName(string& fileName)
	{
		stack_string      path = gEnv->pSchematyc2->GetOldScriptsFolder();
		string::size_type pathPos = fileName.find(gEnv->pSchematyc2->GetOldScriptsFolder());
		if(pathPos == string::npos)
		{
			fileName.insert(0, "/");
			fileName.insert(0, path);
		}
		string::size_type extensionPos = fileName.rfind('.');
		if(extensionPos == string::npos)
		{
			fileName.append(".");
			fileName.append(gEnv->pSchematyc2->GetOldScriptExtension());
		}
		fileName.replace("\\", "/");
	}

	void CScriptRegistry::EnumFile(const char* szFileName, unsigned attributes)
	{
		LOADING_TIME_PROFILE_SECTION;
		const bool bOnDisk = (attributes & _A_IN_CRYPAK) == 0;
		if(bOnDisk || !CVars::sc_IgnorePAKFiles)
		{
			CScriptFilePtr pFile = CreateFile(szFileName, SGUID(), bOnDisk ? EScriptFileFlags::OnDisk : EScriptFileFlags::None);
			if(pFile)
			{
				pFile->Load();
				RegisterFileByGuid(pFile);
			}
		}
	}

	void CScriptRegistry::RegisterFileByGuid(const CScriptFilePtr& pFile)
	{
		const SGUID guid = pFile->GetGUID();
		if(guid)
		{
			FilesByGUID::const_iterator itOtherFile = m_filesByGUID.find(guid);
			if(itOtherFile == m_filesByGUID.end())
			{
				m_filesByGUID.insert(FilesByGUID::value_type(guid, pFile));
			}
			else
			{
				SCHEMATYC2_SYSTEM_CRITICAL_ERROR("Script files have duplicate guids: %s, %s", pFile->GetFileName(), itOtherFile->second->GetFileName());
			}
		}
		else
		{
			SCHEMATYC2_SYSTEM_CRITICAL_ERROR("Script file has no guid: %s", pFile->GetFileName());
		}
	}

	INewScriptFile* CScriptRegistry::CreateNewFile(const char* szFileName, const SGUID& guid)
	{
		// #SchematycTODO : Should we also take steps to avoid name collisions?
		CNewScriptFilePtr pNewFile = std::make_shared<CNewScriptFile>(guid, szFileName);
		m_newFiles.insert(NewFiles::value_type(guid, pNewFile));
		return pNewFile.get();
	}

	INewScriptFile* CScriptRegistry::CreateNewFile(const char* szFileName)
	{
		SCHEMATYC2_SYSTEM_ASSERT(gEnv->IsEditor());
		if(gEnv->IsEditor())
		{
			return CreateNewFile(szFileName, gEnv->pSchematyc2->CreateGUID());
		}
		return nullptr;
	}

	INewScriptFile* CScriptRegistry::GetNewFile(const SGUID& guid)
	{
		NewFiles::iterator itNewFile = m_newFiles.find(guid);
		return itNewFile != m_newFiles.end() ? itNewFile->second.get() : nullptr;
	}

	void CScriptRegistry::AddElement(const IScriptElementPtr& pElement, IScriptElement& scope)
	{
		scope.AttachChild(*pElement);
		m_elements.insert(Elements::value_type(pElement->GetGUID(), pElement));
		m_signals.change.Send(EScriptRegistryChange::ElementAdded, pElement.get());
	}

	void CScriptRegistry::RemoveElement(IScriptElement& element)
	{
		while(IScriptElement* pChildElement = element.GetFirstChild())
		{
			RemoveElement(*pChildElement);
		}

		m_signals.change.Send(EScriptRegistryChange::ElementRemoved, &element);

		INewScriptFile* pNewFile = element.GetNewFile();
		if(pNewFile)
		{
			pNewFile->RemoveElement(element);
			element.SetNewFile(nullptr);
			if(!pNewFile->GetElementCount())
			{
				m_newFiles.erase(pNewFile->GetGUID());
			}
		}

		m_elements.erase(element.GetGUID());
	}

	void CScriptRegistry::SaveElementRecursive(IScriptElement& element, const char* szPath)
	{
		class CElementSerializer
		{
		public:

			CElementSerializer(IScriptElement* pElement = nullptr)
				: m_pElement(pElement)
			{}

			void Serialize(Serialization::IArchive& archive)
			{
				if(m_pElement)
				{
					m_pElement->Serialize(archive);

					std::vector<CElementSerializer> children;
					children.reserve(20);
					for(IScriptElement* pChildElement = m_pElement->GetFirstChild(); pChildElement; pChildElement = pChildElement->GetNextSibling())
					{
						if(!ScriptRegistryUtils::IsFileElement(pChildElement->GetElementType()))
						{
							children.push_back(CElementSerializer(pChildElement));
						}
					}
					archive(children, "children");
				}
			}

		private:

			IScriptElement* m_pElement;
		};

		class CFileSerializer
		{
		public:

			CFileSerializer(INewScriptFile* pNewFile, IScriptElement* pRootElement)
				: m_pNewFile(pNewFile)
				, m_pRootElement(pRootElement)
			{}

			void Serialize(Serialization::IArchive& archive)
			{
				uint32 versionNumber = 1;
				archive(versionNumber, "version");

				SGUID guid = m_pNewFile->GetGUID();
				archive(guid, "guid");
				
				IScriptElement* pScopeElement = m_pRootElement->GetParent();
				if(pScopeElement)
				{
					SGUID scopeGUID = pScopeElement->GetGUID();
					archive(scopeGUID, "scope");
				}

				archive(CElementSerializer(m_pRootElement), "root");
			}

		private:

			INewScriptFile* m_pNewFile; // #SchematycTODO : Can this be a reference?
			IScriptElement* m_pRootElement; // #SchematycTODO : Can this be a reference?
		};

		const char* szElementName = element.GetName();
		if(ScriptRegistryUtils::IsFileElement(element.GetElementType()))
		{
			stack_string fullPath = gEnv->pCryPak->GetGameFolder();
			fullPath.append("/");
			fullPath.append(szPath);
			gEnv->pCryPak->MakeDir(fullPath.c_str());

			stack_string    fileName;
			INewScriptFile* pNewFile = element.GetNewFile();
			if(pNewFile)
			{
				fileName = pNewFile->GetName();
			}
			else
			{
				fileName = szPath;
				fileName.append("/");
				fileName.append(szElementName);
				fileName.append(".");
				fileName.append(gEnv->pSchematyc2->GetFileFormat());
				fileName.MakeLower();

				pNewFile = CreateNewFile(fileName.c_str());
				pNewFile->AddElement(element);
				element.SetNewFile(pNewFile);
			}

			const stack_string extension = fileName.substr(fileName.rfind('.'));
			if(extension == ".json")
			{
				Serialization::SaveJsonFile(fileName.c_str(), CFileSerializer(pNewFile, &element));
			}
			else if(extension == ".xml")
			{
				Serialization::SaveXmlFile(fileName.c_str(), CFileSerializer(pNewFile, &element), "schematycScript");
			}
		}

		stack_string childPath = szPath;
		childPath.append("/");
		childPath.append(szElementName);
		childPath.MakeLower();
		for(IScriptElement* pChildElement = element.GetFirstChild(); pChildElement; pChildElement = pChildElement->GetNextSibling())
		{
			SaveElementRecursive(*pChildElement, childPath.c_str());
		}
	}
}
