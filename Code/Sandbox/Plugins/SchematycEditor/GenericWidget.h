// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QWidget>

#include <CrySchematyc2/GUID.h>

#include "GenericWidgetModel.h"

//////////////////////////////////////////////////////////////////////////
class QToolBar;
class QCheckBox;
class QBoxLayout;
class QGridLayout;
class CAbstractDictionary;

//////////////////////////////////////////////////////////////////////////
namespace Cry {
namespace SchematycEd {

class CGenericWidget : public QWidget
{
	Q_OBJECT;

public:
	template <class T> struct Unit;
	template <class T> struct Impl;

public:
	CGenericWidget();
	virtual ~CGenericWidget();

	void LoadClass(const SGUID& fileGUID, const SGUID& scopeGUID);

public:
	static CGenericWidget* Create();

protected:
	virtual void LoadClassInternal() = 0;

private:
	QToolBar* CreateToolBar();

	void      AddCategory(CAbstractDictionary& category);
	void      ReloadCategory(CAbstractDictionary& category, bool checked);

protected:
	SGUID              m_fileGUID;
	SGUID              m_classGUID;

	QBoxLayout*        m_pMainLayout;	
	QBoxLayout*        m_pBodyLayout;
	QGridLayout*       m_pGridLayout;
	CDictionaryWidget* m_pDictionaryWidget;
};


} //namespace SchematycEd
} //namespace Cry
