// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "PreferencesDialog.h"

#include "Controls/QObjectTreeWidget.h"
#include "EditorFramework/Preferences.h"
#include "QAdvancedPropertyTreeLegacy.h"
#include "QControls.h"
#include <IEditor.h>

#include <QHBoxLayout>
#include <QPushButton>
#include <QSplitter>
#include <QVBoxLayout>

struct SPreferencePagesContainer
{
	SPreferencePagesContainer(const std::vector<SPreferencePage*>& preferences)
		: m_preferences(preferences)
	{
	}

	virtual ~SPreferencePagesContainer() {}

	virtual bool Serialize(yasli::Archive& ar)
	{
		for (SPreferencePage* pPreferencePage : m_preferences)
			pPreferencePage->Serialize(ar);

		return true;
	}

private:
	std::vector<SPreferencePage*> m_preferences;
};

QPreferencePage::QPreferencePage(std::vector<SPreferencePage*> preferencesPages, const char* path, QWidget* pParent /* = nullptr*/)
	: QWidget(pParent)
	, m_preferencePages(preferencesPages)
	, m_path(path)
{
	InitUI();

	SPreferencePagesContainer* pContainer = new SPreferencePagesContainer(preferencesPages);
	m_pPropertyTree->attach(yasli::Serializer(*pContainer));
	m_pPropertyTree->expandAll();
}

QPreferencePage::QPreferencePage(SPreferencePage* pPreferencePage, QWidget* pParent /*= nullptr*/)
	: QWidget(pParent)
	, m_preferencePages({ pPreferencePage })
	, m_path(pPreferencePage->GetPath())
{
	InitUI();

	m_pPropertyTree->attach(yasli::Serializer(*pPreferencePage));
	m_pPropertyTree->expandAll();
}

QPreferencePage::~QPreferencePage()
{
	DisconnectPreferences();
}

void QPreferencePage::InitUI()
{
	QVBoxLayout* pMainLayout = new QVBoxLayout();
	pMainLayout->setMargin(0);
	pMainLayout->setSpacing(0);

	m_pPropertyTree = new QAdvancedPropertyTreeLegacy("Properties", this);
	m_pPropertyTree->setShowContainerIndices(true);
	connect(m_pPropertyTree, &QPropertyTreeLegacy::signalChanged, this, &QPreferencePage::OnPropertyChanged);
	pMainLayout->addWidget(m_pPropertyTree);
	ConnectPreferences();

	QHBoxLayout* pActionLayout = new QHBoxLayout();
	pActionLayout->setSpacing(0);
	pActionLayout->setContentsMargins(0, 5, 0, 0);
	pActionLayout->addItem(new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));
	QPushButton* pResetButton = new QPushButton("Reset to Default");
	connect(pResetButton, &QPushButton::clicked, this, &QPreferencePage::OnResetToDefault);
	pActionLayout->addWidget(pResetButton);
	pMainLayout->addLayout(pActionLayout);

	setLayout(pMainLayout);
}

void QPreferencePage::OnPreferenceChanged()
{
	m_pPropertyTree->revertNoninterrupting();
}

void QPreferencePage::ConnectPreferences()
{
	for (SPreferencePage* pPreferencePage : m_preferencePages)
	{
		pPreferencePage->signalSettingsChanged.Connect(this, &QPreferencePage::OnPreferenceChanged);
	}
}

void QPreferencePage::DisconnectPreferences()
{
	for (SPreferencePage* pPreferencePage : m_preferencePages)
	{
		pPreferencePage->signalSettingsChanged.DisconnectObject(this);
	}
}

void QPreferencePage::OnPropertyChanged()
{
	GetIEditor()->GetPreferences()->Save();
}

void QPreferencePage::OnResetToDefault()
{
	GetIEditor()->GetPreferences()->Reset(m_path);
}

QPreferencesDialog::QPreferencesDialog(QWidget* pParent /*= nullptr*/)
	: CDockableWidget(pParent)
{
	QVBoxLayout* pMainLayout = new QVBoxLayout();

	m_pTreeView = new QObjectTreeWidget();
	m_pTreeView->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);

	const char* szGeneral = "General";
	CPreferences* pPreferences = GetIEditor()->GetPreferences();
	const std::map<string, std::vector<SPreferencePage*>>& preferences = pPreferences->GetPages();
	for (auto ite = preferences.begin(); ite != preferences.end(); ++ite)
	{
		auto moduleIdx = ite->first.find_last_of("/");
		string module = ite->first.Mid(moduleIdx + 1, ite->first.size() - moduleIdx);
		m_pTreeView->AddEntry(ite->first, ite->first, module == szGeneral ? "aGeneral" : "z" + module);
	}
	m_pTreeView->ExpandAll();

	// Create empty preference page container
	m_pContainer = new QContainer();
	m_pSplitter = new QSplitter(Qt::Horizontal);
	m_pSplitter->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	m_pSplitter->addWidget(m_pTreeView);
	m_pSplitter->addWidget(m_pContainer);
	pMainLayout->addWidget(m_pSplitter);
	m_pSplitter->setSizes({ m_pTreeView->sizeHint().width(), GetPaneRect().width() });

	m_pTreeView->signalOnClickFile.Connect(this, &QPreferencesDialog::OnPreferencePageSelected);
	pPreferences->signalSettingsReset.Connect(this, &QPreferencesDialog::OnPreferencesReset);

	setLayout(pMainLayout);
}

QPreferencesDialog::~QPreferencesDialog()
{
	m_pTreeView->signalOnClickFile.DisconnectObject(this);
	GetIEditor()->GetPreferences()->signalSettingsReset.DisconnectObject(this);
}

void QPreferencesDialog::OnPreferencePageSelected(const char* path)
{
	m_pContainer->SetChild(GetIEditor()->GetPreferences()->GetPageWidget(path));
	m_currPath = path;
}

void QPreferencesDialog::OnPreferencesReset()
{
	m_pContainer->SetChild(GetIEditor()->GetPreferences()->GetPageWidget(m_currPath));
}
