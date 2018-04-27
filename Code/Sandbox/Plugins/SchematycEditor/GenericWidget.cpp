// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "GenericWidget.h"

#include <QLayout>
#include <QToolbar>
#include <QCheckBox>
#include <QBoxLayout>

#include <CryIcon.h>
#include <Controls/DictionaryWidget.h>

#include <ProxyModels/MergingProxyModel.h>
#include <ProxyModels/ItemModelAttribute.h>

#include <CrySchematyc2/GUID.h>
#include <CrySchematyc2/IFramework.h>
#include <CrySchematyc2/Script/IScriptRegistry.h>

#include "Util.h"
#include "GenericWidgetImpl.h"
#include "GenericWidgetModel.h"

namespace Cry {
namespace SchematycEd {

//////////////////////////////////////////////////////////////////////////
CGenericWidget::CGenericWidget()
	: m_pMainLayout(new QBoxLayout(QBoxLayout::TopToBottom))
	, m_pBodyLayout(new QBoxLayout(QBoxLayout::LeftToRight))
	, m_pGridLayout(new QGridLayout())
	, m_pDictionaryWidget(new CDictionaryWidget())
{
	m_pMainLayout->addLayout(m_pGridLayout);
	m_pMainLayout->addLayout(m_pBodyLayout);

	m_pBodyLayout->addWidget(CreateToolBar());
	m_pBodyLayout->addWidget(m_pDictionaryWidget);

	setLayout(m_pMainLayout);
}

//////////////////////////////////////////////////////////////////////////
CGenericWidget::~CGenericWidget()
{
	m_pDictionaryWidget->deleteLater();	
}

//////////////////////////////////////////////////////////////////////////
void CGenericWidget::LoadClass(const SGUID& fileGUID, const SGUID& classGUID)
{
	m_fileGUID  = fileGUID;
	m_classGUID = classGUID;

	LoadClassInternal();
}

//////////////////////////////////////////////////////////////////////////
QToolBar* CGenericWidget::CreateToolBar()
{
	QToolBar* pToolBar = new QToolBar("Tools");
	pToolBar->setOrientation(Qt::Vertical);
	{			
		QAction* pAction = pToolBar->addAction(CryIcon("icons:General/designer_rectangle.ico"), QString());
		pAction->setToolTip("");
		pAction->setShortcut(QObject::tr(""));
		//QObject::connect(pAction, &QAction::triggered, this, &CMainWindow::OnSave);
	}
	{
		QAction* pAction = pToolBar->addAction(CryIcon("icons:General/designer_rectangle.ico"), QString());
		pAction->setToolTip("");
		pAction->setShortcut(QObject::tr(""));
		//QObject::connect(pAction, &QAction::triggered, this, &CMainWindow::OnSave);
	}
	{
		QAction* pAction = pToolBar->addAction(CryIcon("icons:General/designer_rectangle.ico"), QString());
		pAction->setToolTip("");
		pAction->setShortcut(QObject::tr(""));
		//QObject::connect(pAction, &QAction::triggered, this, &CMainWindow::OnSave);
	}

	return pToolBar;
}

//////////////////////////////////////////////////////////////////////////
void CGenericWidget::AddCategory(CAbstractDictionary& category)
{
	m_pDictionaryWidget->AddDictionary(category);
}

//////////////////////////////////////////////////////////////////////////
void CGenericWidget::ReloadCategory(CAbstractDictionary& category, bool checked)
{
	CGenericWidgetDictionaryModel& categoryImpl = static_cast<CGenericWidgetDictionaryModel&>(category);
	if (Schematyc2::GUID::IsEmpty(m_classGUID) || Schematyc2::GUID::IsEmpty(m_fileGUID))
	{
		return;
	}	

	categoryImpl.ClearScriptElementModel();
	if (checked)
	{
		IScriptFile* pScriptFile = gEnv->pSchematyc2->GetScriptRegistry().GetFile(m_fileGUID);
		if (pScriptFile)
		{
			categoryImpl.BuildFromScriptClass(pScriptFile, m_classGUID);
		}
	}
	m_pDictionaryWidget->AddDictionary(categoryImpl);
}

} //namespace SchematycEd
} //namespace Cry
