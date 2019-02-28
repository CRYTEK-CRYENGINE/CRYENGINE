// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DisplayOptions.h"
#include "SandboxPlugin.h"
#include <Serialization/QPropertyTree/QPropertyTree.h>
#include <QVBoxLayout>
#include <QVariant>
#include <QJsonDocument>

#include <CrySerialization/yasli/Archive.h>
#include <CrySerialization/yasli/STL.h>
#include <CrySerialization/yasli/Enum.h>
#include <CrySerialization/yasli/JSONIArchive.h>
#include <CrySerialization/yasli/JSONOArchive.h>

YASLI_ENUM_BEGIN_NESTED(SViewSettings, EViewportMode, "ViewportMode")
YASLI_ENUM(SViewSettings::eViewportMode_RcOutput, "targetView", "Game");
YASLI_ENUM(SViewSettings::eViewportMode_SourceView, "sourceView", "Source");
YASLI_ENUM(SViewSettings::eViewportMode_Split, "splitView", "Game and Source");
YASLI_ENUM_END()

YASLI_ENUM_BEGIN_NESTED(SViewSettings, EShadingMode, "ShadingMode")
YASLI_ENUM(SViewSettings::eShadingMode_Default, "default", "Default");
YASLI_ENUM(SViewSettings::eShadingMode_VertexColors, "vertexColors", "VertexColors");
YASLI_ENUM(SViewSettings::eShadingMode_VertexAlphas, "vertexAlphas", "VertexAlphas");
YASLI_ENUM_END()

namespace Private_DisplayOptions
{

static const char* s_settingsPropertyName = "displayOptions";

} // namespace Private_DisplayOptions

void SViewSettings::Serialize(Serialization::IArchive& ar)
{
	ar(bShowEdges, "showEdges", "Show Edges");
	ar(bShowProxies, "showProxies", "Show Proxies");
	ar(bShowSize, "showSize", "Show Size");
	ar(bShowPivots, "showPivots", "Show Pivots");
	ar(bShowJointNames, "showJointNames", "Show Joint Names");
	ar(viewportMode, "viewportMode", "Viewport mode");
	ar(shadingMode, "shadingMode", "Shading mode");

	if (ar.isEdit())
	{
		ar(Serialization::Range(lod, 0, MAX_STATOBJ_LODS_NUM - 1), "lod", "Show LOD");
	}
}

CDisplayOptionsWidget::CDisplayOptionsWidget(QWidget* pParent)
	: QWidget(pParent)
	, m_pPropertyTree(nullptr)
{
	using namespace Private_DisplayOptions;

	setContentsMargins(0, 0, 0, 0);

	m_pOptions.reset(new SDisplayOptions());

	// Load personalization.
	{
		const QVariant variant(CFbxToolPlugin::GetInstance()->GetPersonalizationProperty(s_settingsPropertyName));
		const QByteArray json(QJsonDocument::fromVariant(variant).toJson());
		yasli::JSONIArchive ar;
		ar.open(json.data(), json.size());
		ar(*m_pOptions, s_settingsPropertyName);
	}

	m_pPropertyTree = new QPropertyTree(this);
	PropertyTreeStyle treeStyle(QPropertyTree::defaultTreeStyle());
	treeStyle.propertySplitter = false;
	treeStyle.groupRectangle = false;
	m_pPropertyTree->setTreeStyle(treeStyle);
	m_pPropertyTree->setCompact(false);
	m_pPropertyTree->setExpandLevels(1);
	m_pPropertyTree->setSliderUpdateDelay(5);
	m_pPropertyTree->setValueColumnWidth(0.6f);
	m_pPropertyTree->attach(Serialization::SStruct(*m_pOptions));

	connect(m_pPropertyTree, &QPropertyTree::signalChanged, this, &CDisplayOptionsWidget::OnPropertyTreeChanged);
	connect(m_pPropertyTree, &QPropertyTree::signalContinuousChange, this, &CDisplayOptionsWidget::OnPropertyTreeChanged);

	QVBoxLayout* pLayout = new QVBoxLayout();
	pLayout->setContentsMargins(0, 0, 0, 0);
	pLayout->addWidget(m_pPropertyTree);
	setLayout(pLayout);

	setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
}

const SDisplayOptions& CDisplayOptionsWidget::GetSettings() const
{
	assert(m_pOptions);
	return *m_pOptions;
}

void CDisplayOptionsWidget::SetUserOptions(const Serialization::SStruct& options, const char* szName, const char* szLabel)
{
	m_pOptions->SetUserOptions(options, szName, szLabel);
	m_pPropertyTree->revert();
}

void CDisplayOptionsWidget::OnPropertyTreeChanged()
{
	using namespace Private_DisplayOptions;

	// Save personalization.
	yasli::JSONOArchive ar;
	ar(*m_pOptions, s_settingsPropertyName);
	CFbxToolPlugin::GetInstance()->SetPersonalizationProperty(s_settingsPropertyName, QJsonDocument::fromJson(ar.c_str()).toVariant());

	SigChanged();
}

