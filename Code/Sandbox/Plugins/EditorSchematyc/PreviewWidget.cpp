// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "PreviewWidget.h"

#include <QBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QSplitter>
#include <QViewport.h>
#include <QViewportConsumer.h>
#include <CryEntitySystem/IEntitySystem.h>
#include <CrySerialization/IArchiveHost.h>
#include <QAdvancedPropertyTree.h>

#include "Objects/DisplayContext.h"
#include "QViewportEvents.h"
#include "IEditor.h"

#include <Cry3DEngine/I3DEngine.h>

namespace Schematyc {

CPreviewSettingsWidget::CPreviewSettingsWidget(CPreviewWidget& previewWidget)
{
	QVBoxLayout* pLayout = new QVBoxLayout(this);

	m_pPropertyTree = new QAdvancedPropertyTree("Preview Settings");
	m_pPropertyTree->setExpandLevels(4);
	m_pPropertyTree->setValueColumnWidth(0.6f);
	m_pPropertyTree->setAutoRevert(false);
	m_pPropertyTree->setAggregateMouseEvents(false);
	m_pPropertyTree->setFullRowContainers(true);

	m_pPropertyTree->attach(Serialization::SStruct(previewWidget));

	pLayout->addWidget(m_pPropertyTree);
}

void CPreviewSettingsWidget::showEvent(QShowEvent* pEvent)
{
	QWidget::showEvent(pEvent);

	if (m_pPropertyTree)
	{
		m_pPropertyTree->setSizeToContent(true);
	}
}

CGizmoTranslateOp::CGizmoTranslateOp(ITransformManipulator& gizmo, IScriptComponentInstance& componentInstance)
	: m_gizmo(gizmo)
	, m_componentInstance(componentInstance)
{}

void CGizmoTranslateOp::OnInit()
{
	m_initTransform.SetIdentity();
	if (m_componentInstance.GetTransform())
		m_initTransform = m_initTransform = m_componentInstance.GetTransform()->ToMatrix34();
}

void CGizmoTranslateOp::OnMove(const Vec3& offset)
{
	CTransformPtr transform = m_componentInstance.GetTransform();
	transform->SetTranslation(m_initTransform.GetTranslation() + offset);
	m_componentInstance.SetTransform(transform);

	gEnv->pSchematyc->GetScriptRegistry().ElementModified(m_componentInstance);
}

void CGizmoTranslateOp::OnRelease()
{
	if (m_componentInstance.GetTransform())
		m_gizmo.SetCustomTransform(true, m_componentInstance.GetTransform()->ToMatrix34());
}

CPreviewWidget::CPreviewWidget(QWidget* pParent)
	: QWidget(pParent)
{
	m_pMainLayout = new QBoxLayout(QBoxLayout::TopToBottom);
	m_pViewport = new QViewport(gEnv, this);

	m_viewportSettings.rendering.fps = false;
	m_viewportSettings.grid.showGrid = true;
	m_viewportSettings.grid.spacing = 1.0f;
	m_viewportSettings.camera.showViewportOrientation = false;
	m_viewportSettings.camera.moveSpeed = 0.2f;

	connect(m_pViewport, SIGNAL(SignalRender(const SRenderContext &)), SLOT(OnRender(const SRenderContext &)));

	gEnv->pSchematyc->GetScriptRegistry().GetChangeSignalSlots().Connect(SCHEMATYC_MEMBER_DELEGATE(&CPreviewWidget::OnScriptRegistryChange, *this), m_connectionScope);
}

CPreviewWidget::~CPreviewWidget()
{
	SetClass(nullptr);
}

void CPreviewWidget::InitLayout()
{
	QWidget::setLayout(m_pMainLayout);
	m_pMainLayout->setSpacing(1);
	m_pMainLayout->setMargin(0);
	m_pMainLayout->addWidget(m_pViewport);

	m_pViewport->SetSettings(m_viewportSettings);
	m_pViewport->SetSceneDimensions(Vec3(100.0f, 100.0f, 100.0f));

	ResetCamera(ms_defaultOrbitTarget, ms_defaultOrbitRadius);
	m_pViewport->SetOrbitMode(true);

	Update();
}

void CPreviewWidget::Update()
{
	if (m_pViewport)
	{
		m_pViewport->Update();
	}
}

void CPreviewWidget::SetClass(const IScriptClass* pScriptClass)
{
	auto releaseClass = [this]()
	{
		if (m_pObjectPreviewer)
		{
			m_pObjectPreviewer->DestroyObject(m_objectId);

			m_classGUID = CryGUID();
			m_pObjectPreviewer = nullptr;
			m_objectId = ObjectId::Invalid;
		}
	};

	if (pScriptClass)
	{
		CRY_ASSERT_MESSAGE(pScriptClass->GetType() == Schematyc::EScriptElementType::Class, "Unsupported element type.");
		if (pScriptClass->GetGUID() != m_classGUID)
		{
			releaseClass();

			IScriptViewPtr pScriptView = gEnv->pSchematyc->CreateScriptView(pScriptClass->GetGUID());
			const IEnvClass* pEnvClass = pScriptView->GetEnvClass();
			CRY_ASSERT(pEnvClass);
			if (pEnvClass)
			{
				m_pObjectPreviewer = pEnvClass->GetPreviewer();
				if (m_pObjectPreviewer)
				{
					m_classGUID = pScriptClass->GetGUID();

					Reset();

					Vec3 orbitTarget = ms_defaultOrbitTarget;
					float orbitRadius = ms_defaultOrbitRadius;

					const Sphere objetcBounds = m_pObjectPreviewer->GetObjectBounds(m_objectId);
					if (objetcBounds.radius > 0.001f)
					{
						orbitTarget = objetcBounds.center;
						orbitRadius = std::max(objetcBounds.radius * 1.25f, orbitRadius);
					}

					ResetCamera(orbitTarget, orbitRadius);
				}
			}
		}
	}
	else
	{
		releaseClass();
	}
}

void CPreviewWidget::SetComponentInstance(const IScriptComponentInstance* pComponentInstance)
{
	auto releaseComponentInstance = [this]()
	{
		m_componentInstanceGUID = CryGUID();

		if (m_pGizmo)
		{
			m_pViewport->GetGizmoManager()->RemoveManipulator(m_pGizmo);
			m_pGizmo = nullptr;
		}
	};

	if (pComponentInstance && pComponentInstance->HasTransform())
	{
		if (pComponentInstance->GetGUID() != m_componentInstanceGUID)
		{
			releaseComponentInstance();

			m_componentInstanceGUID = pComponentInstance->GetGUID();

			m_pGizmo = m_pViewport->GetGizmoManager()->AddManipulator(this);

			auto onBeginDrag = [this](IDisplayViewport*, ITransformManipulator*, const Vec2i&, int)
			{
				if (GetIEditor()->GetEditMode() == eEditModeMove)
				{
					IScriptComponentInstance* pComponentInstance = DynamicCast<IScriptComponentInstance>(gEnv->pSchematyc->GetScriptRegistry().GetElement(m_componentInstanceGUID));
					if (pComponentInstance)
					{
						m_pGizmoTransformOp.reset(new CGizmoTranslateOp(*m_pGizmo, *pComponentInstance));
						m_pGizmoTransformOp->OnInit();
					}
				}
				else
				{
					m_pGizmoTransformOp->OnRelease();
					m_pGizmoTransformOp.release();
				}
			};
			m_pGizmo->signalBeginDrag.Connect(onBeginDrag);

			auto onDrag = [this](IDisplayViewport*, ITransformManipulator*, const Vec2i&, const Vec3& offset, int)
			{
				if (m_pGizmoTransformOp)
				{
					m_pGizmoTransformOp->OnMove(offset);
				}
			};
			m_pGizmo->signalDragging.Connect(onDrag);

			auto onEndDrag = [this](IDisplayViewport*, ITransformManipulator*)
			{
				if (m_pGizmoTransformOp)
				{
					m_pGizmoTransformOp->OnRelease();
					m_pGizmoTransformOp.release();
					signalChanged();
				}
			};
			m_pGizmo->signalEndDrag.Connect(onEndDrag);
		}
	}
	else
	{
		releaseComponentInstance();
	}
}

void CPreviewWidget::Reset()
{
	if (m_pObjectPreviewer)
	{
		if (m_objectId != ObjectId::Invalid)
		{
			m_objectId = m_pObjectPreviewer->ResetObject(m_objectId);
		}
		else
		{
			m_objectId = m_pObjectPreviewer->CreateObject(m_classGUID);
		}
	}
}

void CPreviewWidget::LoadSettings(const XmlNodeRef& xml)
{
	Serialization::LoadXmlNode(m_viewportSettings, xml);
}

XmlNodeRef CPreviewWidget::SaveSettings(const char* szName)
{
	return Serialization::SaveXmlNode(m_viewportSettings, szName);
}

void CPreviewWidget::Serialize(Serialization::IArchive& archive)
{
	IObject* pObject = gEnv->pSchematyc->GetObject(m_objectId);
	if (pObject)
	{
		m_pObjectPreviewer->SerializeProperties(archive);

		std::set<IEntityComponentPreviewer*> componentPreviewers;

		auto visitComponent = [&componentPreviewers](const IEntityComponent& component) -> EVisitStatus
		{
			IEntityComponentPreviewer* pComponentPreviewer = nullptr; //component.GetPreviewer();
			if (pComponentPreviewer)
			{
				componentPreviewers.insert(pComponentPreviewer);
			}
			return EVisitStatus::Continue;
		};
		pObject->VisitComponents(visitComponent);

		for (IEntityComponentPreviewer* pComponentPreviewer : componentPreviewers)
		{
			pComponentPreviewer->SerializeProperties(archive); // #SchematycTODO : How do we avoid name collisions? Do we need to fully qualify component names?
		}
	}

	archive(m_viewportSettings, "viewportSettings", "Viewport");
	if (archive.isInput())
	{
		m_pViewport->SetSettings(m_viewportSettings);
	}
}

bool CPreviewWidget::GetManipulatorMatrix(RefCoordSys coordSys, Matrix34& tm)
{
	return false;
}

void CPreviewWidget::GetManipulatorPosition(Vec3& position)
{
	if (!GUID::IsEmpty(m_componentInstanceGUID))
	{
		const IScriptComponentInstance* pComponentInstance = DynamicCast<const IScriptComponentInstance>(gEnv->pSchematyc->GetScriptRegistry().GetElement(m_componentInstanceGUID));
		if (pComponentInstance && pComponentInstance->GetTransform())
		{
			position = pComponentInstance->GetTransform()->GetTranslation();
		}
	}
	else
	{
		position = Vec3(ZERO);
	}
}

void CPreviewWidget::OnRender(const SRenderContext& context)
{
	IObject* pObject = gEnv->pSchematyc->GetObject(m_objectId);
	if (pObject)
	{
		m_pObjectPreviewer->RenderObject(*pObject, *context.renderParams, *context.passInfo);

		auto visitComponent = [pObject, &context](const IEntityComponent& component) -> EVisitStatus
		{
			const IEntityComponentPreviewer* pComponentPreviewer = nullptr;// component.GetPreviewer();
			if (pComponentPreviewer)
			{
				SGeometryDebugDrawInfo dd;
				SEntityPreviewContext preview(dd);
				preview.pRenderParams = context.renderParams;
				preview.pPassInfo = context.passInfo;
				pComponentPreviewer->Render(*pObject->GetEntity(), component, preview);
			}

			return EVisitStatus::Continue;
		};
		pObject->VisitComponents(visitComponent);
	}
}

void CPreviewWidget::ResetCamera(const Vec3& orbitTarget, float orbitRadius) const
{
	SViewportState viewportState = m_pViewport->GetState();
	viewportState.cameraTarget.t = Vec3(0.0f, -orbitRadius, orbitRadius * 0.5f);
	viewportState.orbitTarget = orbitTarget;
	viewportState.orbitRadius = orbitRadius;

	m_pViewport->SetState(viewportState);
	m_pViewport->LookAt(orbitTarget, orbitRadius * 2.0f, true);
}

void CPreviewWidget::OnScriptRegistryChange(const SScriptRegistryChange& change)
{
	Reset();
}

const Vec3 CPreviewWidget::ms_defaultOrbitTarget = Vec3(0.0f, 0.0f, 1.0f);
const float CPreviewWidget::ms_defaultOrbitRadius = 2.0f;

} // Schematyc

