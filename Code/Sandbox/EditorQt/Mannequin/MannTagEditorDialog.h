// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __MannTagEditorDialog_h__
#define __MannTagEditorDialog_h__

#if _MSC_VER > 1000
	#pragma once
#endif

#include "Controls/TagSelectionControl.h"

class CTagDefinition;
struct SMannequinContexts;

class CMannTagEditorDialog : public CXTResizeDialog
{
public:
	CMannTagEditorDialog(CWnd* pParent, IAnimationDatabase* animDB, FragmentID fragmentID);
	virtual ~CMannTagEditorDialog();

	enum { IDD = IDD_MANN_TAG_EDITOR };

	const CString& FragmentName() const { return m_fragmentName; }

protected:
	DECLARE_MESSAGE_MAP()

	virtual void  DoDataExchange(CDataExchange* pDX);
	virtual BOOL  OnInitDialog();
	virtual void  OnOK();

	const CString GetCurrentADBFileSel() const;
	void          ProcessFragmentADBChanges();
	void          ProcessFragmentTagChanges();
	void          ProcessFlagChanges();
	void          ProcessScopeTagChanges();

	void          InitialiseFragmentTags(const CTagDefinition* pTagDef);
	void          AddTagDef(const CString& filename);
	void          AddTagDefInternal(const CString& tagDefDisplayName, const CString& normalizedFilename);
	void          AddFragADB(const CString& filename);
	void          AddFragADBInternal(const CString& fragFileDisplayName, const CString& normalizedFilename);
	void          ResetFragmentADBs();
	bool          ContainsTagDefDisplayName(const CString& tagDefDisplayNameToSearch) const;
	const CString GetSelectedTagDefFilename() const;
	void          SelectTagDefByTagDef(const CTagDefinition* pTagDef);
	void          SelectTagDefByFilename(const CString& filename);

	afx_msg void  OnEditTagDefs();
	afx_msg void  OnCbnSelchangeFragfileCombo();
	afx_msg void  OnBnClickedCreateAdbFile();

private:
	void InitialiseFragmentADBsRec(const CString& baseDir);
	void InitialiseFragmentTagsRec(const CString& baseDir);
	void InitialiseFragmentADBs();

	CEdit                m_nameEdit;
	CButton              m_btnIsPersistent;
	CButton              m_btnAutoReinstall;
	CStatic              m_scopeTagsGroupBox;
	CTagSelectionControl m_scopeTagsControl;
	CComboBox            m_tagDefComboBox;
	CComboBox            m_FragFileComboBox;
	int                  m_nOldADBFileIndex;

	SMannequinContexts*  m_contexts;
	IAnimationDatabase*  m_animDB;
	FragmentID           m_fragmentID;
	CString              m_fragmentName;

	struct SFragmentTagEntry
	{
		CString displayName;
		CString filename;

		SFragmentTagEntry(const CString& displayName_, const CString& filename_)
			: displayName(displayName_), filename(filename_)  {}
	};
	typedef std::vector<SFragmentTagEntry> TEntryContainer;
	TEntryContainer m_entries;
	TEntryContainer m_vFragADBFiles;
};

#endif

