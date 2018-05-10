// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "PreviewWidget.h"

#include <QBoxLayout>
#include <Util/QParentWndWidget.h>
#include <QPushButton>
#include <QSplitter>
#include <QViewport.h>
#include <Serialization/QPropertyTree/QPropertyTree.h>
#include <CryEntitySystem/IEntitySystem.h>
#include <CrySerialization/IArchiveHost.h>
#include <CrySchematyc2/IDomainContext.h>
#include <CrySchematyc2/IObject.h>
#include <CrySchematyc2/Env/IEnvRegistry.h>

#include "PluginUtils.h"

namespace Schematyc2
{
	const Vec3  CPreviewWidget::DefaultOrbitTarget = Vec3(0.0f, 0.0f, 1.0f);
	const float CPreviewWidget::DefaultOrbitRadius = 2.0f;

	//////////////////////////////////////////////////////////////////////////
	CPreviewWidget::CPreviewWidget(QWidget* pParent)
		: QWidget(pParent)
		, m_pSplitter(nullptr)
		, m_pSettings(nullptr)
		, m_pViewport(nullptr)
		, m_pPreviewObject(nullptr)
	{
		m_pMainLayout             = new QBoxLayout(QBoxLayout::TopToBottom);
		m_pSplitter               = new QSplitter(Qt::Horizontal, this);
		m_pSettings               = new QPropertyTree(this);
		m_pViewport               = new QViewport(gEnv, this);
		m_pControlLayout          = new QBoxLayout(QBoxLayout::LeftToRight);
		m_pShowHideSettingsButton = new QPushButton(">> Show Settings", this);

		m_pSettings->setVisible(false);

		m_viewportSettings.rendering.fps                  = false;
		m_viewportSettings.grid.showGrid                  = true;
		m_viewportSettings.grid.spacing                   = 1.0f;
		m_viewportSettings.camera.showViewportOrientation = false;
		m_viewportSettings.camera.moveSpeed               = 0.2f;

		connect(m_pViewport, SIGNAL(SignalRender(const SRenderContext&)), SLOT(OnRender(const SRenderContext&)));
		connect(m_pShowHideSettingsButton, SIGNAL(clicked()), this, SLOT(OnShowHideSettingsButtonClicked()));
	}

	//////////////////////////////////////////////////////////////////////////
	CPreviewWidget::~CPreviewWidget()
	{
		SetClass(nullptr); // #SchematycTODO : Does this trigger ENTITY_EVENT_DONE?
		SAFE_DELETE(m_pSettings);
	}

	//////////////////////////////////////////////////////////////////////////
	void CPreviewWidget::OnEntityEvent(IEntity* pEntity, const SEntityEvent& event)
	{
		if(event.event == ENTITY_EVENT_DONE)
		{
			if(pEntity)
			{
				gEnv->pEntitySystem->RemoveEntityEventListener(pEntity->GetId(), ENTITY_EVENT_DONE, this);
			}
			if(m_pPreviewObject)
			{
				m_pFoundationPreviewExtension->EndPreview();
			}
			m_pPreviewObject     = nullptr;
			m_pPreviewProperties = IObjectPreviewPropertiesPtr();
			// Force re-serialization.
			m_pSettings->revert();
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CPreviewWidget::InitLayout()
	{
		QWidget::setLayout(m_pMainLayout);
		m_pMainLayout->setSpacing(1);
		m_pMainLayout->setMargin(0);
		m_pMainLayout->addWidget(m_pSplitter, 1);
		m_pMainLayout->addLayout(m_pControlLayout, 0);
		
		m_pSplitter->setStretchFactor(0, 1);
		m_pSplitter->setStretchFactor(1, 0);
		m_pSplitter->addWidget(m_pSettings);
		m_pSplitter->addWidget(m_pViewport);
		
		QList<int>	splitterSizes;
		splitterSizes.push_back(60);
		splitterSizes.push_back(200);
		m_pSplitter->setSizes(splitterSizes);

		// TODO pavloi 2016.05.02: propertySplitter breaks inlining of property rows. Until it's fixed - apply custom style.
		PropertyTreeStyle treeStyle(QPropertyTree::defaultTreeStyle());
		treeStyle.propertySplitter = false;
		m_pSettings->setTreeStyle(treeStyle);

		m_pSettings->setSizeHint(QSize(250, 400));
		m_pSettings->setExpandLevels(1);
		m_pSettings->setSliderUpdateDelay(5);
		m_pSettings->setValueColumnWidth(0.6f);
		m_pSettings->attach(Serialization::SStruct(*this));

		m_pViewport->SetSettings(m_viewportSettings);
		m_pViewport->SetSceneDimensions(Vec3(100.0f, 100.0f, 100.0f));

		ResetCamera(DefaultOrbitTarget, DefaultOrbitRadius);
		m_pViewport->SetOrbitMode(true);

		m_pControlLayout->setSpacing(2);
		m_pControlLayout->setMargin(4);
		m_pControlLayout->addWidget(m_pShowHideSettingsButton, 1);
	}

	//////////////////////////////////////////////////////////////////////////
	void CPreviewWidget::Update()
	{
		if(m_pViewport)
		{
			m_pViewport->Update();
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CPreviewWidget::SetClass(const IScriptClass* pClass)
	{
		if(pClass != m_pClass)
		{
			if(m_pFoundationPreviewExtension)
			{
				m_pPreviewObject     = nullptr;
				m_pPreviewProperties = IObjectPreviewPropertiesPtr();
				m_pFoundationPreviewExtension->EndPreview();
				m_pClass                      = nullptr;
				m_pFoundationPreviewExtension = nullptr;
				// Force re-serialization.
				m_pSettings->revert();
			}
			if(pClass)
			{
				#ifdef CRY_USE_SCHEMATYC2_BRIDGE
				TDomainContextPtr pDomainContext = GetSchematyc()->CreateDomainContext(pClass);
				#else
				TDomainContextPtr pDomainContext = GetSchematyc()->CreateDomainContext(TDomainContextScope(&pClass->GetFile(), pClass->GetGUID()));
				#endif //CRY_USE_SCHEMATYC2_BRIDGE

				IFoundationConstPtr pFoundation = pDomainContext->GetEnvFoundation();
				CRY_ASSERT(pFoundation);
				if(pFoundation)
				{
					m_pFoundationPreviewExtension = pFoundation->QueryExtension<IFoundationPreviewExtension>();
					if(m_pFoundationPreviewExtension)
					{
						m_pClass = pClass;
						Reset();
						// Update viewport state.
						Vec3	orbitTarget = DefaultOrbitTarget;
						float	orbitRadius = DefaultOrbitRadius;
						if(m_pPreviewObject)
						{
							if(IEntity* pEntity = gEnv->pEntitySystem->GetEntity(m_pPreviewObject->GetEntityId().GetValue()))
							{
								AABB	entityWorldBounds;
								pEntity->GetWorldBounds(entityWorldBounds);
								const float	entityRadius = entityWorldBounds.GetRadius();
								if(entityRadius > 0.001f)
								{
									orbitTarget	= entityWorldBounds.GetCenter();
									orbitRadius	= std::max(entityRadius * 1.25f, orbitRadius);
								}
							}
						}
						ResetCamera(orbitTarget, orbitRadius);
					}
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CPreviewWidget::Reset()
	{
		if(m_pFoundationPreviewExtension)
		{
			if(m_pPreviewObject)
			{
				m_pFoundationPreviewExtension->EndPreview();
			}
			m_pPreviewObject = m_pFoundationPreviewExtension->BeginPreview(m_pClass->GetGUID(), PluginUtils::GetSelectedEntityId());
			if(m_pPreviewObject)
			{
				const EntityId	entityId = m_pPreviewObject->GetEntityId().GetValue();
				if(entityId)
				{
					gEnv->pEntitySystem->AddEntityEventListener(entityId, ENTITY_EVENT_DONE, this);
				}
				m_pPreviewProperties = m_pPreviewObject->GetPreviewProperties();
			}
		}
		// Force re-serialization.
		m_pSettings->revert();
	}

	//////////////////////////////////////////////////////////////////////////
	void CPreviewWidget::LoadSettings(const XmlNodeRef& xml)
	{
		Serialization::LoadXmlNode(m_viewportSettings, xml);
	}

	//////////////////////////////////////////////////////////////////////////
	XmlNodeRef CPreviewWidget::SaveSettings(const char* szName)
	{
		return Serialization::SaveXmlNode(m_viewportSettings, szName);
	}

	//////////////////////////////////////////////////////////////////////////
	void CPreviewWidget::Serialize(Serialization::IArchive& archive)
	{
		if(m_pPreviewProperties)
		{
			archive(m_viewportSettings, "viewportSettings", "Viewport Settings");
			archive(*m_pPreviewProperties, "previewProperties", "Preview Properties");
			if(archive.isInput())
			{
				m_pViewport->SetSettings(m_viewportSettings);
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CPreviewWidget::OnRender(const SRenderContext& context)
	{
		if(m_pFoundationPreviewExtension)
		{
			m_pFoundationPreviewExtension->RenderPreview(*context.renderParams, *context.passInfo);
		}
		if(m_pPreviewObject && m_pPreviewProperties)
		{
			m_pPreviewObject->Preview(*context.renderParams, *context.passInfo, *m_pPreviewProperties);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CPreviewWidget::OnShowHideSettingsButtonClicked()
	{
		const bool bShowSettings = !m_pSettings->isVisible();
		m_pShowHideSettingsButton->setText(bShowSettings ? "<< Hide Settings" : ">> Show Settings");
		m_pSettings->setVisible(bShowSettings);
	}

	//////////////////////////////////////////////////////////////////////////
	void CPreviewWidget::ResetCamera(const Vec3& orbitTarget, float orbitRadius) const
	{
		SViewportState viewportState = m_pViewport->GetState();
		viewportState.cameraTarget.t = Vec3(0.0f, -orbitRadius, orbitRadius * 0.5f);
		viewportState.orbitTarget    = orbitTarget;
		viewportState.orbitRadius    = orbitRadius;
		m_pViewport->SetState(viewportState);
		m_pViewport->LookAt(orbitTarget, orbitRadius * 2.0f, true);
	}

	//////////////////////////////////////////////////////////////////////////
	BEGIN_MESSAGE_MAP(CPreviewWnd, CWnd)
		ON_WM_SIZE()
	END_MESSAGE_MAP()

	//////////////////////////////////////////////////////////////////////////
	CPreviewWnd::CPreviewWnd()
		: m_pParentWndWidget(nullptr)
		, m_pPreviewWidget(nullptr)
		, m_pLayout(nullptr)
	{}

	//////////////////////////////////////////////////////////////////////////
	CPreviewWnd::~CPreviewWnd()
	{
		SAFE_DELETE(m_pLayout);
		SAFE_DELETE(m_pPreviewWidget);
		SAFE_DELETE(m_pParentWndWidget);
	}

	//////////////////////////////////////////////////////////////////////////
	void CPreviewWnd::Init()
	{
		LOADING_TIME_PROFILE_SECTION;
		m_pParentWndWidget = new QParentWndWidget(GetSafeHwnd());
		m_pPreviewWidget   = new CPreviewWidget(m_pParentWndWidget);
		m_pLayout          = new QBoxLayout(QBoxLayout::TopToBottom);
	}

	//////////////////////////////////////////////////////////////////////////
	void CPreviewWnd::InitLayout()
	{
		LOADING_TIME_PROFILE_SECTION;
		m_pLayout->setContentsMargins(0, 0, 0, 0);
		m_pLayout->addWidget(m_pPreviewWidget, 1);
		m_pParentWndWidget->setLayout(m_pLayout);
		m_pParentWndWidget->show();
		m_pPreviewWidget->InitLayout();
		Update();
	}

	//////////////////////////////////////////////////////////////////////////
	void CPreviewWnd::Update()
	{
		if(m_pParentWndWidget)
		{
			CRect rect; 
			CWnd::GetClientRect(&rect);
			m_pParentWndWidget->setGeometry(0, 0, rect.Width(), rect.Height());
		}
		if(m_pPreviewWidget)
		{
			m_pPreviewWidget->Update();
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CPreviewWnd::SetClass(const IScriptClass* pClass)
	{
		if(m_pPreviewWidget)
		{
			m_pPreviewWidget->SetClass(pClass);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CPreviewWnd::Reset()
	{
		if(m_pPreviewWidget)
		{
			m_pPreviewWidget->Reset();
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CPreviewWnd::LoadSettings(const XmlNodeRef& xml)
	{
		if(m_pPreviewWidget)
		{
			m_pPreviewWidget->LoadSettings(xml);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	XmlNodeRef CPreviewWnd::SaveSettings(const char* szName)
	{
		if(m_pPreviewWidget)
		{
			return m_pPreviewWidget->SaveSettings(szName);
		}
		return XmlNodeRef();
	}

	//////////////////////////////////////////////////////////////////////////
	void CPreviewWnd::OnSize(UINT nType, int cx, int cy)
	{
		Update();
	}
}
