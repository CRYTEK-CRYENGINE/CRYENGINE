// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include <CryCore/ToolsHelpers/ResourceCompilerHelper.h>
#include <CryThreading/IThreadManager.h>

class CResourceCompilerDialog : public CXTResizeDialog, public IThread, public IResourceCompilerListener
{
public:
	CResourceCompilerDialog(const CString& fileName, const CString& additionalSettings,
	                        std::function<void(const CResourceCompilerHelper::ERcCallResult)> finishedCallback);

	virtual ~CResourceCompilerDialog();

	virtual BOOL OnInitDialog() override;

private:
	virtual void DoDataExchange(CDataExchange* pDX);

	// Start accepting work on thread
	virtual void ThreadEntry() override;
	virtual void OnRCMessage(MessageSeverity severity, const char* pText);

	void         AppendError(const char* pText);
	void         AppendToOutput(CString string, bool bUseDefaultColor, COLORREF color);
	int          GetNumVisibleLines();

	afx_msg void OnClose();
	afx_msg void OnShowLog();

	CString                                                           m_fileName;
	CString                                                           m_additionalSettings;

	CButton                                                           m_closeButton;
	CFont                                                             m_outputFont;
	CRichEditCtrl                                                     m_outputCtrl;
	CProgressCtrl                                                     m_progressCtrl;

	CResourceCompilerHelper                                           m_rcHelper;
	volatile bool                                                     m_bRcCanceled;
	volatile bool                                                     m_bRcFinished;
	CResourceCompilerHelper::ERcCallResult                            m_rcCallResult;

	std::function<void(const CResourceCompilerHelper::ERcCallResult)> m_finishedCallback;

	DECLARE_MESSAGE_MAP()
};

