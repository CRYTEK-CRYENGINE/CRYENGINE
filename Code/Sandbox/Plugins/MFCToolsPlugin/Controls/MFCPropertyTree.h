// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QObject>

#include <CrySerialization/Forward.h>
using Serialization::IArchive;
class QPropertyTree;
class CMFCPropertyTree;

class CMFCPropertyTreeSignalHandler : public QObject
{
	Q_OBJECT

public:

	CMFCPropertyTreeSignalHandler(CMFCPropertyTree* propertyTree);

protected slots:

	void OnSizeChanged();
	void OnPropertyChanged();

private:

	CMFCPropertyTree* m_propertyTree;
};

class PLUGIN_API CMFCPropertyTree : public CWnd
{
	DECLARE_DYNCREATE(CMFCPropertyTree)
public:

	CMFCPropertyTree();
	~CMFCPropertyTree();

	BOOL               Create(CWnd* parent, const RECT& rect);
	void               Release();

	void               Serialize(IArchive& ar);
	void               Attach(Serialization::SStruct& ser);
	void               Detach();
	void               Revert();
	void               SetExpandLevels(int expandLevels);
	void               SetCompact(bool compact);
	void               SetArchiveContext(Serialization::SContextLink* context);
	void               SetHideSelection(bool hideSelection);
	void               SetPropertyChangeHandler(const Functor0& callback);
	void               SetSizeChangeHandler(const Functor0& callback);
	SIZE               ContentSize() const;

	static const char* ClassName() { return "PropertyTreeMFCAdapter"; }
	static bool        RegisterWindowClass();
	static void        UnregisterWindowClass();

	DECLARE_MESSAGE_MAP()
protected:
	afx_msg int  OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnSetFocus(CWnd* oldWnd);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);

private:
	friend class CMFCPropertyTreeSignalHandler;
	QPropertyTree*                 m_propertyTree;
	CMFCPropertyTreeSignalHandler* m_signalHandler;
	Functor0                       m_sizeChangeCallback;
	Functor0                       m_propertyChangeCallback;
};

