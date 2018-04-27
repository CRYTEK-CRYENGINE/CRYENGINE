// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "DetailWidget.h"

#include <QBoxLayout>
#include <Util/QParentWndWidget.h>
#include <Serialization/QPropertyTree/QPropertyTree.h>
#include <CrySerialization/IArchiveHost.h>
#include <CrySchematyc2/Serialization/ISerializationContext.h>

namespace Schematyc2
{
	CEnvSettingsDetailItem::CEnvSettingsDetailItem(const IEnvSettingsPtr& pEnvSettings)
		: m_pEnvSettings(pEnvSettings)
	{}

	SGUID CEnvSettingsDetailItem::GetGUID() const
	{
		return SGUID();
	}

	Serialization::CContextList* CEnvSettingsDetailItem::GetContextList()
	{
		return nullptr;
	}

	void CEnvSettingsDetailItem::SetStatusFlags(EDetailItemStatusFlags statusFlags) {}

	void CEnvSettingsDetailItem::Serialize(Serialization::IArchive& archive)
	{
		if(m_pEnvSettings)
		{
			m_pEnvSettings->Serialize(archive);
		}
	}

	CScriptElementDetailItem::CScriptElementDetailItem(TScriptFile* pScriptFile, IScriptElement* pScriptElement)
		: m_pScriptElement(pScriptElement)
		, m_pContextList(new Serialization::CContextList())
	{
		m_pContextList->Update<TScriptFile>(pScriptFile); // #SchematycTODO : Do we still need this?
	}

	SGUID CScriptElementDetailItem::GetGUID() const
	{
		return m_pScriptElement ? m_pScriptElement->GetGUID() : SGUID();
	}

	Serialization::CContextList* CScriptElementDetailItem::GetContextList()
	{
		return m_pContextList.get();
	}

	void CScriptElementDetailItem::SetStatusFlags(EDetailItemStatusFlags statusFlags) {}

	void CScriptElementDetailItem::Serialize(Serialization::IArchive& archive)
	{
		if(m_pScriptElement)
		{
			ISerializationContextPtr pSerializationContext = GetSchematyc()->CreateSerializationContext(SSerializationContextParams(archive, &m_pScriptElement->GetFile(), ESerializationPass::Edit));
			m_pScriptElement->Serialize(archive);
		}
	}

	CDocGraphViewNodeDetailItem::CDocGraphViewNodeDetailItem(TScriptFile* pScriptFile, const CDocGraphViewNodePtr& pDocGraphViewNode)
		: m_pScriptFile(pScriptFile)
		, m_pDocGraphViewNode(pDocGraphViewNode)
		, m_pContextList(new Serialization::CContextList())
	{
		m_pContextList->Update<TScriptFile>(m_pScriptFile); // #SchematycTODO : Do we still need this?
	}

	SGUID CDocGraphViewNodeDetailItem::GetGUID() const
	{
		return m_pDocGraphViewNode ? m_pDocGraphViewNode->GetGUID() : SGUID();
	}

	Serialization::CContextList* CDocGraphViewNodeDetailItem::GetContextList()
	{
		return m_pContextList.get();
	}

	void CDocGraphViewNodeDetailItem::SetStatusFlags(EDetailItemStatusFlags statusFlags)
	{
		if(m_pDocGraphViewNode)
		{
			EGraphViewNodeStatusFlags graphViewNodeStatusFlags = m_pDocGraphViewNode->GetStatusFlags() & ~(EGraphViewNodeStatusFlags::ContainsWarnings | EGraphViewNodeStatusFlags::ContainsErrors);
			if((statusFlags & EDetailItemStatusFlags::ContainsWarnings) != 0)
			{
				graphViewNodeStatusFlags |= EGraphViewNodeStatusFlags::ContainsWarnings;
			}
			if((statusFlags & EDetailItemStatusFlags::ContainsErrors) != 0)
			{
				graphViewNodeStatusFlags |= EGraphViewNodeStatusFlags::ContainsErrors;
			}
			m_pDocGraphViewNode->SetStatusFlags(graphViewNodeStatusFlags);
		}
	}

	void CDocGraphViewNodeDetailItem::Serialize(Serialization::IArchive& archive)
	{
		if(m_pDocGraphViewNode)
		{
			ISerializationContextPtr pSerializationContext = GetSchematyc()->CreateSerializationContext(Schematyc2::SSerializationContextParams(archive, m_pScriptFile->GetDelegate(), ESerializationPass::Edit));
			m_pDocGraphViewNode->Serialize(archive);
		}
	}

	CDetailWidget::CDetailWidget(QWidget* pParent)
		: QWidget(pParent)
	{
		m_pLayout       = new QBoxLayout(QBoxLayout::TopToBottom);
		m_pPropertyTree = new QPropertyTree(this);
	}

	void CDetailWidget::InitLayout()
	{
		QWidget::setLayout(m_pLayout);
		m_pLayout->setSpacing(2);
		m_pLayout->setMargin(4);
		m_pLayout->addWidget(m_pPropertyTree, 1);

		m_pPropertyTree->setSizeHint(QSize(250, 400));
		m_pPropertyTree->setExpandLevels(3);
		m_pPropertyTree->setSliderUpdateDelay(5);
		m_pPropertyTree->setValueColumnWidth(0.6f);
		m_pPropertyTree->setUndoEnabled(true);

		// TODO pavloi 2016.05.02: propertySplitter breaks inlining of property rows. Until it's fixed - apply custom style.
		PropertyTreeStyle treeStyle(QPropertyTree::defaultTreeStyle());
		treeStyle.propertySplitter = false;
		m_pPropertyTree->setTreeStyle(treeStyle);

		connect(m_pPropertyTree, SIGNAL(signalChanged()), SLOT(OnPropertyChanged()));
	}

	void CDetailWidget::LoadSettings(const XmlNodeRef& xml) {}

	XmlNodeRef CDetailWidget::SaveSettings(const char* szName)
	{
		return XmlNodeRef();
	}

	void CDetailWidget::AttachItem(const IDetailItemPtr& pItem)
	{
		Detach();
		m_pItem = pItem;
		Serialization::CContextList* pContextList = m_pItem->GetContextList();
		if(pContextList)
		{
			m_pPropertyTree->setArchiveContext(pContextList->Tail());
		}
		m_pPropertyTree->attach(Serialization::SStruct(*m_pItem));
		m_pPropertyTree->setExpandLevels(3);
		RefreshStatusFlags();
	}

	void CDetailWidget::Revert()
	{
		if(m_pItem)
		{
			m_pPropertyTree->revert();
			RefreshStatusFlags();
		}
	}

	void CDetailWidget::Detach()
	{
		Revert();
		if(m_pItem)
		{
			m_pPropertyTree->detach();
			m_pPropertyTree->setArchiveContext(nullptr);
			m_pItem = nullptr;
		}
	}

	SDetailWidgetSignals& CDetailWidget::Signals()
	{
		return m_signals;
	}

	void CDetailWidget::OnPropertyChanged()
	{
		RefreshStatusFlags();
		if (m_pItem.get())
		{
			m_signals.modification.Send(*m_pItem);
		}
	}

	void CDetailWidget::RefreshStatusFlags()
	{
		if(m_pItem)
		{
			EDetailItemStatusFlags statusFlags = EDetailItemStatusFlags::None;
			if(m_pPropertyTree->containsWarnings())
			{
				statusFlags |= EDetailItemStatusFlags::ContainsWarnings;
			}
			if(m_pPropertyTree->containsErrors())
			{
				statusFlags |= EDetailItemStatusFlags::ContainsErrors;
			}
			m_pItem->SetStatusFlags(statusFlags);
		}
	}

	BEGIN_MESSAGE_MAP(CDetailWnd, CWnd)
		ON_WM_SHOWWINDOW()
		ON_WM_SIZE()
	END_MESSAGE_MAP()

	CDetailWnd::CDetailWnd()
		: m_pParentWndWidget(nullptr)
		, m_pDetailWidget(nullptr)
		, m_pLayout(nullptr)
	{}

	CDetailWnd::~CDetailWnd()
	{
		SAFE_DELETE(m_pLayout);
		SAFE_DELETE(m_pDetailWidget);
		SAFE_DELETE(m_pParentWndWidget);
	}

	void CDetailWnd::Init()
	{
		LOADING_TIME_PROFILE_SECTION;
		m_pParentWndWidget	= new QParentWndWidget(GetSafeHwnd());
		m_pDetailWidget			= new CDetailWidget(m_pParentWndWidget);
		m_pLayout						= new QBoxLayout(QBoxLayout::TopToBottom);
	}

	void CDetailWnd::InitLayout()
	{
		LOADING_TIME_PROFILE_SECTION;
		m_pLayout->setContentsMargins(0, 0, 0, 0);
		m_pLayout->addWidget(m_pDetailWidget, 1);
		m_pParentWndWidget->setLayout(m_pLayout);
		m_pParentWndWidget->show();
		m_pDetailWidget->InitLayout();
	}

	void CDetailWnd::LoadSettings(const XmlNodeRef& xml)
	{
		m_pDetailWidget->LoadSettings(xml);
	}

	XmlNodeRef CDetailWnd::SaveSettings(const char* szName)
	{
		return m_pDetailWidget->SaveSettings(szName);
	}

	void CDetailWnd::AttachItem(const IDetailItemPtr& pItem)
	{
		m_pDetailWidget->AttachItem(pItem);
	}

	void CDetailWnd::Revert()
	{
		m_pDetailWidget->Revert();
	}

	void CDetailWnd::Detach()
	{
		m_pDetailWidget->Detach();
	}

	void CDetailWnd::UpdateView()
	{
		m_pDetailWidget->OnPropertyChanged();
	}

	SDetailWidgetSignals& CDetailWnd::Signals()
	{
		return m_pDetailWidget->Signals();
	}

	void CDetailWnd::OnShowWindow(BOOL bShow, UINT nStatus)
	{
		if(m_pDetailWidget)
		{
			m_pDetailWidget->setVisible(bShow);
		}
	}

	void CDetailWnd::OnSize(UINT nType, int cx, int cy)
	{
		if(m_pParentWndWidget)
		{
			CRect	rect; 
			CWnd::GetClientRect(&rect);
			m_pParentWndWidget->setGeometry(0, 0, rect.Width(), rect.Height());
		}
	}
}
