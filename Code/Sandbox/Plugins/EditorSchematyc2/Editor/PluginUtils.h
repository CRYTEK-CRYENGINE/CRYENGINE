// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// #SchematycTODO : Merge with DragAndDrop, EditorGUID and String util headers?

// This macro is used to create a scope, in which MFC can access resources bundled inside 
// this Dll. If you get errors like a Menu cannot be created by resource Id - check that
// the creation happens inside this scope.
#define SET_LOCAL_RESOURCE_SCOPE Schematyc2::PluginUtils::CLocalResourceScope localResourceScope;

namespace Schematyc2
{
	typedef std::vector<string> TStringVector;

	namespace PluginUtils
	{
		class CLocalResourceScope
		{
		public:

			CLocalResourceScope();

			~CLocalResourceScope();

		private:

			HINSTANCE	m_hPrevInstance;
		};

		struct SDragAndDropData
		{
			SDragAndDropData();

			SDragAndDropData(size_t _icon, const SGUID& _guid = SGUID());

			static inline const UINT GetClipboardFormat();

			size_t	icon;
			SGUID		guid;
		};

		bool BeginDragAndDrop(const SDragAndDropData &data);
		bool GetDragAndDropData(COleDataObject* pDataObject, SDragAndDropData &data);

		bool LoadTrueColorImageList(UINT nIDResource, int iconWidth, COLORREF maskColor, CImageList &imageList);

		void GetSubFoldersAndFileNames(const char* szFolderName, const char* szExtension, bool bIgnorePakFiles, TStringVector& subFolderNames, TStringVector& fileNames);
		
		bool IsFileNameUnique(const char* szFileName);
		void MakeFileNameUnique(stack_string& fileName);

		bool IsNumericChar(char x);
		bool IsAlphanumericChar(char x);
		bool StringContainsNonAlphaNumericChars(const char* input, const char* exceptions);

		bool IsValidName(const char* name, stack_string& errorMessage);
		bool IsValidFilePath(const char* fileName, stack_string& errorMessage);
		bool IsValidFileName(const char* fileName, stack_string& errorMessage);

		bool ValidateFilePath(HWND hWnd, const char* szFilePath, bool displayErrorMessages);
		bool ValidateScriptElementName(HWND hWnd, const TScriptFile& file, const SGUID& scopeGUID, const char* szName, bool displayErrorMessages);

		void ViewWiki(const char* wikiLink);

		void WriteToClipboard(const char* text);
		void WriteXmlToClipboard(XmlNodeRef xml);
		stack_string ReadFromClipboard();
		XmlNodeRef ReadXmlFromClipboard(const char* tag);

		void ShowInExplorer(const char* path);
		void DiffAgainstHaveVersion(const char* path);

		EntityId GetSelectedEntityId();
	}
}