// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "PluginUtils.h"

#include <IActionMapManager.h> // #SchematycTODO : Remove dependency on CryAction!
#include <CryEntitySystem/IEntityClass.h>
#include <CryEntitySystem/IEntitySystem.h>
#include <CryString/CryStringUtils.h>
#include <IResourceSelectorHost.h>
#include <ISourceControl.h>

#include <Util/XmlArchive.h> // XmlArchive.h must be included before including EntityObject.h.
class CTrackViewAnimNode; // Forward declaration must be made before including EntityObject.h.
#include <Objects/EntityObject.h>

#include "QuickSearchDlg.h"

#include <CrySchematyc2/TemplateUtils/TemplateUtils_Delegate.h>
#include <CrySystem/File/ICryPak.h>

#include <vector>

namespace Schematyc2
{
	namespace FileUtils
	{
		enum class EFileEnumFlags
		{
			None = 0,
			IgnoreUnderscoredFolders = BIT(0),
			Recursive = BIT(1),
		};

		DECLARE_ENUM_CLASS_FLAGS(EFileEnumFlags)

		typedef TemplateUtils::CDelegate<void(const char*, unsigned)> FileEnumCallback;

		void EnumFilesInFolder(const char* szFolderName, const char* szExtension, FileEnumCallback callback, EFileEnumFlags flags = EFileEnumFlags::Recursive)
		{
			LOADING_TIME_PROFILE_SECTION_ARGS(szFolderName);
			if (szFolderName)
			{
				SCHEMATYC2_SYSTEM_ASSERT(callback);
				if (callback)
				{
					if ((flags & EFileEnumFlags::Recursive) != 0)
					{
						stack_string	searchPath = szFolderName;
						searchPath.append("/*.*");
						_finddata_t	findData;
						intptr_t		handle = gEnv->pCryPak->FindFirst(searchPath.c_str(), &findData);
						if (handle >= 0)
						{
							do
							{
								if ((findData.name[0] != '.') && ((findData.attrib & _A_SUBDIR) != 0))
								{
									if (((flags & EFileEnumFlags::IgnoreUnderscoredFolders) == 0) || (findData.name[0] != '_'))
									{
										stack_string	subName = szFolderName;
										subName.append("/");
										subName.append(findData.name);
										EnumFilesInFolder(subName.c_str(), szExtension, callback, flags);
									}
								}
							} while (gEnv->pCryPak->FindNext(handle, &findData) >= 0);
						}
					}
					stack_string	searchPath = szFolderName;
					searchPath.append("/");
					searchPath.append(szExtension ? szExtension : "*.*");
					_finddata_t	findData;
					intptr_t		handle = gEnv->pCryPak->FindFirst(searchPath.c_str(), &findData);
					if (handle >= 0)
					{
						do
						{
							if ((findData.name[0] != '.') && ((findData.attrib & _A_SUBDIR) == 0))
							{
								stack_string	subName = szFolderName;
								subName.append("/");
								subName.append(findData.name);
								callback(subName.c_str(), findData.attrib);
							}
						} while (gEnv->pCryPak->FindNext(handle, &findData) >= 0);
					}
				}
			}
		}
	}
}

namespace
{
	class CSimpleQuickSearchOptions : public Schematyc2::IQuickSearchOptions
	{
	public:

		inline CSimpleQuickSearchOptions(const char* szToken)
			: m_szToken(szToken)
		{}

		// IQuickSearchOptions

		virtual const char* GetToken() const override
		{
			return m_szToken;
		}

		virtual size_t GetCount() const override
		{
			return m_options.size();
		}

		virtual const char* GetLabel(size_t optionIdx) const override
		{
			return optionIdx < m_options.size() ? m_options[optionIdx].c_str() : "";
		}

		virtual const char* GetDescription(size_t optionIdx) const override
		{
			return nullptr;
		}

		virtual const char* GetWikiLink(size_t optionIdx) const override
		{
			return nullptr;
		}

		// ~IQuickSearchOptions

		void AddOption(const char* szOption)
		{
			m_options.push_back(szOption);
		}

	private:

		typedef std::vector<string> StringVector;

		const char*  m_szToken;
		StringVector m_options;
	};

	class CStringListStaticQuickSearchOptions : public Schematyc2::IQuickSearchOptions
	{
	public:

		CStringListStaticQuickSearchOptions(const Serialization::StringListStatic& stringList)
			: m_stringList(stringList)
		{}

		// IQuickSearchOptions

		virtual const char* GetToken() const override
		{
			return "::";
		}

		virtual size_t GetCount() const
		{
			return m_stringList.size();
		}

		virtual const char* GetLabel(size_t optionIdx) const
		{
			return optionIdx < m_stringList.size() ? m_stringList[optionIdx] : "";
		}

		virtual const char* GetDescription(size_t optionIdx) const
		{
			return nullptr;
		}

		virtual const char* GetWikiLink(size_t optionIdx) const
		{
			return nullptr;
		}

		// ~IQuickSearchOptions

	private:

		const Serialization::StringListStatic& m_stringList;
	};

	class CActionMapActionQuickSearchOptions : public CSimpleQuickSearchOptions, public IActionMapPopulateCallBack
	{
	public:

		inline CActionMapActionQuickSearchOptions()
			: CSimpleQuickSearchOptions(".")
		{
			IActionMapManager* pActionMapManager = gEnv->pGameFramework->GetIActionMapManager();
			if(pActionMapManager && (pActionMapManager->GetActionsCount() > 0))
			{
				pActionMapManager->EnumerateActions(this);
			}
		}

		// IActionMapPopulateCallBack
		virtual void AddActionName(const char* const szName) override
		{
			CSimpleQuickSearchOptions::AddOption(szName);
		}
		// ~IActionMapPopulateCallBack
	};

	dll_string EntityClassNameSelector(const SResourceSelectorContext& context, const char* szPreviousValue, Serialization::StringListValue* pStringListValue)
	{
		SET_LOCAL_RESOURCE_SCOPE

		CPoint cursorPos;
		GetCursorPos(&cursorPos);

		IEntityClassRegistry& entityClassRegistry = *gEnv->pEntitySystem->GetClassRegistry();
		entityClassRegistry.IteratorMoveFirst();

		CSimpleQuickSearchOptions quickSearchOptions("::");
		while(IEntityClass* pClass = entityClassRegistry.IteratorNext())
		{
			quickSearchOptions.AddOption(pClass->GetName());
		}
		
		Schematyc2::CQuickSearchDlg quickSearchDlg(CWnd::FromHandle(reinterpret_cast<HWND>(context.parentWidget->winId())), CPoint(cursorPos.x - 10, cursorPos.y - 10), "Entity Class", nullptr, szPreviousValue, quickSearchOptions);
		if(quickSearchDlg.DoModal() == IDOK)
		{
			return quickSearchOptions.GetLabel(quickSearchDlg.GetResult());
		}
		else
		{
			return "";
		}
	}

	dll_string ActionMapNameSelector(const SResourceSelectorContext& context, const char* szPreviousValue, Serialization::StringListValue* pStringListValue)
	{
		SET_LOCAL_RESOURCE_SCOPE

		CPoint cursorPos;
		GetCursorPos(&cursorPos);

		CSimpleQuickSearchOptions quickSearchOptions("::");
		IActionMapIteratorPtr     itActionMap = gEnv->pGameFramework->GetIActionMapManager()->CreateActionMapIterator();
		while(IActionMap* pActionMap = itActionMap->Next())
		{
			quickSearchOptions.AddOption(pActionMap->GetName());
		}
		
		Schematyc2::CQuickSearchDlg quickSearchDlg(CWnd::FromHandle(reinterpret_cast<HWND>(context.parentWidget->winId())), CPoint(cursorPos.x - 10, cursorPos.y - 10), "Action Map", nullptr, szPreviousValue, quickSearchOptions);
		if(quickSearchDlg.DoModal() == IDOK)
		{
			return quickSearchOptions.GetLabel(quickSearchDlg.GetResult());
		}
		else
		{
			return "";
		}
	}

	dll_string ActionMapActionNameSelector(const SResourceSelectorContext& context, const char* szPreviousValue, Serialization::StringListValue* pStringListValue)
	{
		SET_LOCAL_RESOURCE_SCOPE

		CPoint cursorPos;
		GetCursorPos(&cursorPos);

		CActionMapActionQuickSearchOptions quickSearchOptions;
		Schematyc2::CQuickSearchDlg         quickSearchDlg(CWnd::FromHandle(reinterpret_cast<HWND>(context.parentWidget->winId())), CPoint(cursorPos.x - 10, cursorPos.y - 10), "Action Map Action", nullptr, szPreviousValue, quickSearchOptions);
		if(quickSearchDlg.DoModal() == IDOK)
		{
			return quickSearchOptions.GetLabel(quickSearchDlg.GetResult());
		}
		else
		{
			return "";
		}
	}

	dll_string QuickSearchSelector(const SResourceSelectorContext& context, const char* szPreviousValue, const Schematyc2::IQuickSearchOptions* pQuickSearchOptions)
	{
		SET_LOCAL_RESOURCE_SCOPE

		CRY_ASSERT(pQuickSearchOptions);
		if(pQuickSearchOptions)
		{
			CPoint cursorPos;
			GetCursorPos(&cursorPos);

			Schematyc2::CQuickSearchDlg quickSearchDlg(CWnd::FromHandle(reinterpret_cast<HWND>(context.parentWidget->winId())), CPoint(cursorPos.x - 100, cursorPos.y - 100), nullptr, nullptr, szPreviousValue, *pQuickSearchOptions);
			if(quickSearchDlg.DoModal() == IDOK)
			{
				return pQuickSearchOptions->GetLabel(quickSearchDlg.GetResult());
			}
		}
		return "";
	}

	dll_string StringListStaticQuickSearchSelector(const SResourceSelectorContext& context, const char* szPreviousValue, const Serialization::StringListStatic* pStringList)
	{
		SET_LOCAL_RESOURCE_SCOPE

		SCHEMATYC2_SYSTEM_ASSERT(pStringList);
		if(pStringList)
		{
			CPoint cursorPos;
			GetCursorPos(&cursorPos);

			CStringListStaticQuickSearchOptions quickSearchOptions(*pStringList);
			Schematyc2::CQuickSearchDlg          quickSearchDlg(CWnd::FromHandle(reinterpret_cast<HWND>(context.parentWidget->winId())), CPoint(cursorPos.x - 100, cursorPos.y - 100), nullptr, nullptr, szPreviousValue, quickSearchOptions);
			if(quickSearchDlg.DoModal() == IDOK)
			{
				return quickSearchOptions.GetLabel(quickSearchDlg.GetResult());
			}
		}
		return "";
	}

	dll_string LensFlareSelector(const SResourceSelectorContext& context, const char* szPreviousValue)
	{
		SET_LOCAL_RESOURCE_SCOPE

		string flareLibPath = gEnv->pCryPak->GetGameFolder() + string("/") + string(FLARE_LIBS_PATH);
		std::vector<string> flareItems;

		Serialization::StringListStatic stringList;

		auto visitFiles = [&flareItems](const char* fileName, unsigned)
		{
			string fullFlareItemName;
			string fileNameOnly = PathUtil::GetFileName(fileName);
			if (XmlNodeRef flaresRoot = gEnv->pSystem->LoadXmlFromFile(fileName))
			{
				int flareItemsCount = flaresRoot->getChildCount();
				for (int i = 0; i < flareItemsCount; ++i)
				{
					XmlNodeRef flareItem = flaresRoot->getChild(i);
					string flareItemName;
					if (flareItem->getAttr("Name", flareItemName))
					{
						fullFlareItemName.Format("%s.%s", fileNameOnly.c_str(), flareItemName.c_str());
						flareItems.emplace_back((fullFlareItemName));
					}
				}
			}
		};

		Schematyc2::FileUtils::EnumFilesInFolder(flareLibPath.c_str(), "*.xml", Schematyc2::FileUtils::FileEnumCallback::FromLambdaFunction(visitFiles));

		for (const string& flareItemName : flareItems)
		{
			stringList.append(flareItemName.c_str());
		}

		CPoint cursorPos;
		GetCursorPos(&cursorPos);

		CStringListStaticQuickSearchOptions quickSearchOptions(stringList);
		Schematyc2::CQuickSearchDlg          quickSearchDlg(CWnd::FromHandle(reinterpret_cast<HWND>(context.parentWidget->winId())), CPoint(cursorPos.x - 450, cursorPos.y + 20), nullptr, nullptr, szPreviousValue, quickSearchOptions);
		if (quickSearchDlg.DoModal() == IDOK)
		{
			return quickSearchOptions.GetLabel(quickSearchDlg.GetResult());
		}

		return szPreviousValue;
	}

	dll_string BehaviorTreeSelector(const SResourceSelectorContext& context, const char* szPreviousValue)
	{
		SET_LOCAL_RESOURCE_SCOPE

		string behaviorLibPath("Scripts/AI/BehaviorTrees/");
		std::vector<string> fileItems;

		Serialization::StringListStatic stringList;

		auto visitFiles = [&fileItems](const char* fileName, unsigned)
		{
			fileItems.emplace_back(PathUtil::GetFileName(fileName));
		};

		Schematyc2::FileUtils::EnumFilesInFolder(behaviorLibPath.c_str(), "*.xml", Schematyc2::FileUtils::FileEnumCallback::FromLambdaFunction(visitFiles));
		behaviorLibPath = "libs/ai/behavior_trees/";
		Schematyc2::FileUtils::EnumFilesInFolder(behaviorLibPath.c_str(), "*.xml", Schematyc2::FileUtils::FileEnumCallback::FromLambdaFunction(visitFiles));

		for (const string& behaviorTree : fileItems)
		{
			stringList.append(behaviorTree.c_str());
		}

		CPoint cursorPos;
		GetCursorPos(&cursorPos);

		CStringListStaticQuickSearchOptions quickSearchOptions(stringList);
		Schematyc2::CQuickSearchDlg          quickSearchDlg(CWnd::FromHandle(reinterpret_cast<HWND>(context.parentWidget->winId())), CPoint(cursorPos.x - 450, cursorPos.y + 20), nullptr, nullptr, szPreviousValue, quickSearchOptions);
		if (quickSearchDlg.DoModal() == IDOK)
		{
			return quickSearchOptions.GetLabel(quickSearchDlg.GetResult());
		}

		return szPreviousValue;
	}

	REGISTER_RESOURCE_SELECTOR("BehaviorTree", BehaviorTreeSelector, "")
	REGISTER_RESOURCE_SELECTOR("LensFlare", LensFlareSelector, "")
	REGISTER_RESOURCE_SELECTOR("EntityClassName", EntityClassNameSelector, "")
	REGISTER_RESOURCE_SELECTOR("SpawnableEntityClassName", EntityClassNameSelector, "")
	REGISTER_RESOURCE_SELECTOR("ActionMapName", ActionMapNameSelector, "")
	REGISTER_RESOURCE_SELECTOR("ActionMapActionName", ActionMapActionNameSelector, "")
	REGISTER_RESOURCE_SELECTOR("QuickSearch", QuickSearchSelector, "")
	REGISTER_RESOURCE_SELECTOR("StringListStaticQuickSearch", StringListStaticQuickSearchSelector, "")
}

namespace Schematyc2
{
	namespace PluginUtils
	{
		//////////////////////////////////////////////////////////////////////////
		CLocalResourceScope::CLocalResourceScope()
			: m_hPrevInstance(AfxGetResourceHandle())
		{
			AfxSetResourceHandle(GetHInstance());
		}

		//////////////////////////////////////////////////////////////////////////
		CLocalResourceScope::~CLocalResourceScope()
		{
			AfxSetResourceHandle(m_hPrevInstance);
		}

		//////////////////////////////////////////////////////////////////////////
		SDragAndDropData::SDragAndDropData()
			: icon(INVALID_INDEX)
		{}

		//////////////////////////////////////////////////////////////////////////
		SDragAndDropData::SDragAndDropData(size_t _icon, const SGUID& _guid)
			: icon(_icon)
			, guid(_guid)
		{}

		//////////////////////////////////////////////////////////////////////////
		const UINT SDragAndDropData::GetClipboardFormat()
		{
			static const UINT clipboardFormat = RegisterClipboardFormat("Schematyc2::DragAndDropItem");
			return clipboardFormat;
		}

		//////////////////////////////////////////////////////////////////////////
		bool BeginDragAndDrop(const SDragAndDropData &data)
		{
			CSharedFile	sharedFile(GMEM_MOVEABLE | GMEM_DDESHARE | GMEM_ZEROINIT);
			sharedFile.Write(&data, sizeof(data));
			if(HGLOBAL hData = sharedFile.Detach())
			{
				COleDataSource*	pDataSource = new COleDataSource();
				pDataSource->CacheGlobalData(SDragAndDropData::GetClipboardFormat(), hData);
				pDataSource->DoDragDrop(DROPEFFECT_MOVE | DROPEFFECT_LINK);
				return true;
			}
			return false;
		}

		//////////////////////////////////////////////////////////////////////////
		bool GetDragAndDropData(COleDataObject* pDataObject, SDragAndDropData &data)
		{
			if(HGLOBAL hData = pDataObject->GetGlobalData(SDragAndDropData::GetClipboardFormat()))
			{
				data = *static_cast<const SDragAndDropData*>(GlobalLock(hData));
				GlobalUnlock(hData);
				return true;
			}
			return false;
		}

		//////////////////////////////////////////////////////////////////////////
		bool LoadTrueColorImageList(UINT nIDResource, int iconWidth, COLORREF maskColor, CImageList &imageList)
		{
			if(HANDLE hImage = LoadImage(AfxGetResourceHandle(), MAKEINTRESOURCE(nIDResource), IMAGE_BITMAP, 0, 0, LR_DEFAULTSIZE | LR_CREATEDIBSECTION))
			{
				CBitmap bitmap;
				if(bitmap.Attach(hImage))
				{
					BITMAP	bitmapData;
					if(bitmap.GetBitmap(&bitmapData))
					{
						const CSize	size(bitmapData.bmWidth, bitmapData.bmHeight); 
						const int		imageCount = size.cx / iconWidth;
						if(imageList || (imageList.Create(iconWidth, size.cy, ILC_COLOR32 | ILC_MASK, imageCount, 0) == TRUE))
						{
							return imageList.Add(&bitmap, maskColor) != -1;
						}
					}
				}
			}
			return false;
		}

		//////////////////////////////////////////////////////////////////////////
		void GetSubFoldersAndFileNames(const char* szFolderName, const char* szExtension, bool bIgnorePakFiles, TStringVector& subFolderNames, TStringVector& fileNames)
		{
			CRY_ASSERT(szFolderName);
			if(szFolderName)
			{
				string searchPath = szFolderName;
				searchPath.append("/");
				searchPath.append(szExtension ? szExtension : "*.*");
				_finddata_t findData;
				intptr_t    handle = gEnv->pCryPak->FindFirst(searchPath.c_str(), &findData);
				if(handle >= 0)
				{
					do
					{
						if(findData.name[0] != '.')
						{
							if(findData.attrib & _A_SUBDIR)
							{
								bool bIgnoreSubDir = false;
								if(bIgnorePakFiles)
								{
									stack_string fileName = gEnv->pCryPak->GetGameFolder();
									fileName.append("/");
									fileName.append(szFolderName);
									fileName.append("/");
									fileName.append(findData.name);
									if(GetFileAttributes(fileName.c_str()) == INVALID_FILE_ATTRIBUTES)
									{
										bIgnoreSubDir = true;
									}
								}
								if(!bIgnoreSubDir)
								{
									subFolderNames.push_back(findData.name);
								}
							}
							else if(!bIgnorePakFiles || ((findData.attrib & _A_IN_CRYPAK) == 0))
							{
								fileNames.push_back(findData.name);
							}
						}
					} while(gEnv->pCryPak->FindNext(handle, &findData) >= 0);
				}
			}
		}

		//////////////////////////////////////////////////////////////////////////
		bool IsFileNameUnique(const char* szFileName)
		{
			return !gEnv->pCryPak->IsFileExist(szFileName);
		}

		//////////////////////////////////////////////////////////////////////////
		void MakeFileNameUnique(stack_string& fileName)
		{
			if(!IsFileNameUnique(fileName.c_str()))
			{
				char                          stringBuffer[StringUtils::s_uint32StringBufferSize] = "";
				const stack_string::size_type counterPos = fileName.find(".");
				stack_string::size_type       counterLength = 0;
				uint32                        counter = 1;
				do
				{
					fileName.replace(counterPos, counterLength, StringUtils::UInt32ToString(counter, stringBuffer));
					counterLength = strlen(stringBuffer);
					++ counter;
				} while(!IsFileNameUnique(fileName.c_str()));
			}
		}

		//////////////////////////////////////////////////////////////////////////
		bool IsNumericChar(char x)
		{
			return ((x >= '0') && (x <= '9'));
		}

		//////////////////////////////////////////////////////////////////////////
		bool IsAlphanumericChar(char x)
		{
			return ((x >= 'a') && (x <= 'z')) || ((x >= 'A') && (x <= 'Z')) || ((x >= '0') && (x <= '9'));
		}

		//////////////////////////////////////////////////////////////////////////
		bool StringContainsNonAlphaNumericChars(const char* input, const char* exceptions)
		{
			for(const char* pos = input; *pos != '\0'; ++ pos)
			{
				if(!IsAlphanumericChar(*pos) && !strchr(exceptions, *pos))
				{
					return true;
				}
			}
			return false;
		}

		//////////////////////////////////////////////////////////////////////////
		bool IsValidName(const char* name, stack_string& errorMessage)
		{
			if(!name || !name[0])
			{
				errorMessage = "Name cannot be empty.";
				return false;
			}
			else if(IsNumericChar(name[0]))
			{
				errorMessage = "Name cannot start with a number.";
				return false;
			}
			else if(StringContainsNonAlphaNumericChars(name, "_"))
			{
				errorMessage = "Name can only contain alphanumeric characters and underscores.";
				return false;
			}
			return true;
		}

		//////////////////////////////////////////////////////////////////////////
		bool IsValidFilePath(const char* filePath, stack_string& errorMessage)
		{
			if(!filePath || !filePath[0])
			{
				errorMessage = "File path cannot be empty.";
				return false;
			}
			else if(IsNumericChar(filePath[0]))
			{
				errorMessage = "File path cannot start with a number.";
				return false;
			}
			else if(StringContainsNonAlphaNumericChars(filePath, "_/\\"))
			{
				errorMessage = "File path can only contain alphanumeric characters, underscores and slashes.";
				return false;
			}
			return true;
		}

		//////////////////////////////////////////////////////////////////////////
		bool IsValidFileName(const char* fileName, stack_string& errorMessage)
		{
			if(!fileName || !fileName[0])
			{
				errorMessage = "File name cannot be empty.";
				return false;
			}
			else if(IsNumericChar(fileName[0]))
			{
				errorMessage = "File name cannot start with a number.";
				return false;
			}
			else if(StringContainsNonAlphaNumericChars(fileName, "_"))
			{
				errorMessage = "File name can only contain alphanumeric characters and underscores.";
				return false;
			}
			return true;
		}

		//////////////////////////////////////////////////////////////////////////
		bool ValidateFilePath(HWND hWnd, const char* szFilePath, bool displayErrorMessages)
		{
			stack_string	errorMessage;
			if(IsValidFilePath(szFilePath, errorMessage))
			{
				return true;
			}
			if(displayErrorMessages)
			{
				::MessageBoxA(hWnd, errorMessage.c_str(), "Invalid File Path", MB_OK | MB_ICONEXCLAMATION);
			}
			return false;
		}

		//////////////////////////////////////////////////////////////////////////
		bool ValidateScriptElementName(HWND hWnd, const TScriptFile& scriptFile, const SGUID& scopeGUID, const char* szName, bool displayErrorMessages)
		{
			stack_string	errorMessage;
			if(IsValidName(szName, errorMessage))
			{
				if(scriptFile.IsElementNameUnique(scopeGUID, szName))
				{
					return true;
				}
				else
				{
					errorMessage = "Name is not unique.";
				}
			}
			if(displayErrorMessages)
			{
				::MessageBoxA(hWnd, errorMessage.c_str(), "Invalid Name", MB_OK | MB_ICONEXCLAMATION);
			}
			return false;
		}

		//////////////////////////////////////////////////////////////////////////
		void ViewWiki(const char* wikiLink)
		{
			ShellExecute(NULL, "open", wikiLink, NULL, NULL, SW_SHOWDEFAULT);
		}

		//////////////////////////////////////////////////////////////////////////
		void WriteToClipboard(const char* text)
		{
			if(!OpenClipboard(NULL))
			{
				AfxMessageBox("Cannot open the Clipboard");
				return;
			}
			if(!EmptyClipboard())
			{
				AfxMessageBox("Cannot empty the Clipboard");
				return;
			}
			CSharedFile	sharedFile(GMEM_MOVEABLE | GMEM_DDESHARE | GMEM_ZEROINIT);
			sharedFile.Write(text, strlen(text));
			HGLOBAL	hSharedFileMemory = sharedFile.Detach();
			if(!hSharedFileMemory)
			{
				return;
			}
			if(!::SetClipboardData(CF_TEXT, hSharedFileMemory))
			{
				AfxMessageBox("Unable to set Clipboard data");
			}
			CloseClipboard();
		}

		//////////////////////////////////////////////////////////////////////////
		void WriteXmlToClipboard(XmlNodeRef xml)
		{
			if(xml)
			{
				WriteToClipboard(xml->getXMLData()->GetString());
			}
		}

		//////////////////////////////////////////////////////////////////////////
		stack_string ReadFromClipboard()
		{
			string	text;
			if(OpenClipboard(NULL))
			{
				text = static_cast<const char*>(GetClipboardData(CF_TEXT));
			}
			CloseClipboard();
			return text;
		}

		//////////////////////////////////////////////////////////////////////////
		XmlNodeRef ReadXmlFromClipboard(const char* tag)
		{
			CRY_ASSERT(tag);
			if(tag)
			{
				const stack_string			clipboardText = ReadFromClipboard();
				stack_string::size_type	pos = clipboardText.find_first_of("<");
				if(pos != stack_string::npos)
				{
					pos = clipboardText.find_first_not_of(" \t", pos + 1);
					if((pos != stack_string::npos) && (strncmp(clipboardText.c_str() + pos, tag, strlen(tag)) == 0))
					{
						return gEnv->pSystem->LoadXmlFromBuffer(clipboardText.c_str(), clipboardText.length());
					}
				}
			}
			return XmlNodeRef();
		}

		//////////////////////////////////////////////////////////////////////////
		void ShowInExplorer(const char* path)
		{
			CRY_ASSERT(path);
			if(path)
			{
				char	currentDirectory[512];
				GetCurrentDirectory(sizeof(currentDirectory) - 1, currentDirectory);

				stack_string	fullPath = currentDirectory;
				fullPath.append("\\");
				fullPath.append(path);
				fullPath.replace("/", "\\");

				ShellExecute(NULL, "open", fullPath.c_str(), NULL, NULL, SW_SHOWDEFAULT);
			}
		}

		//////////////////////////////////////////////////////////////////////////
		void DiffAgainstHaveVersion(const char* path)
		{
			/*CRY_ASSERT(path);
			if(path)
			{
				char	currentDirectory[512];
				GetCurrentDirectory(sizeof(currentDirectory) - 1, currentDirectory);

				stack_string	fullPath = currentDirectory;
				fullPath.append("\\game_hunt\\");
				fullPath.append(path);
				fullPath.replace("/", "\\");

				stack_string	fullDiffPath = currentDirectory;
				fullDiffPath.append("\\Tools\\GameHunt\\Diff\\");

				stack_string	tempLocalFilefullPath = fullDiffPath;
				tempLocalFilefullPath.append("P4HaveRevisionLocalFile.xml");
				fullDiffPath.append("Diff.exe");

				stack_string	parameters = "Diff ";
				parameters.append(fullPath);
				parameters.append(" ");
				parameters.append(tempLocalFilefullPath);

				ISourceControl* pSourceControl = GetIEditor()->GetSourceControl();
				if (pSourceControl)
				{
					int64 HaveRev = 0;
					int64 HeadRev = 0;
					pSourceControl->GetFileRev(fullPath, &HaveRev, &HeadRev);

					bool bRunDiffTool = false;
					if (HaveRev > 0)
					{
						char depotFileHaveRev[MAX_PATH];
						pSourceControl->GetInternalPath( fullPath, depotFileHaveRev, MAX_PATH);
						
						const size_t length = strlen(depotFileHaveRev);
						cry_sprintf(depotFileHaveRev + length, MAX_PATH - length, "#%I64d", HaveRev);

						// Download the depot file and save into a temp file.
						if (pSourceControl->CopyDepotFile(depotFileHaveRev, tempLocalFilefullPath))
						{
							// Launch Diff tool to compare between the original local file and the temp file for the current version.
							ShellExecute(NULL, "open", fullDiffPath.c_str(), parameters.c_str(), NULL, SW_SHOWDEFAULT);
							bRunDiffTool = true;
						}
					}
					
					if (!bRunDiffTool)
					{
						// Pop up message box to tell we can't diff because of version error.
						AfxMessageBox("We can't run Diff tool since the file is not in perforce or its revision number is invalid(<=0).");
					}
				}					
			}*/
		}

		//////////////////////////////////////////////////////////////////////////
		EntityId GetSelectedEntityId()
		{
			CBaseObject*	pSelectedObject = ::GetIEditor()->GetSelectedObject();
			if(pSelectedObject != NULL)
			{
				if(pSelectedObject->IsKindOf(RUNTIME_CLASS(CEntityObject)) == TRUE)
				{
					IEntity*	pSelectedEntity = static_cast<CEntityObject*>(pSelectedObject)->GetIEntity();
					if(pSelectedEntity != NULL)
					{
						return pSelectedEntity->GetId();
					}
				}
			}
			return 0;
		}
	}
}
