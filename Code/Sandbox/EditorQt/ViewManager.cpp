// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ViewManager.h"
#include "Viewport.h"

#include "2DViewport.h"
#include "TopRendererWnd.h"
#include "RenderViewport.h"
#include "ModelViewport.h"
#include "CryEditDoc.h"

#include <IViewSystem.h>
#include <CryGame/IGameFramework.h>
#include <Preferences/ViewportPreferences.h>
#include "QtViewPane.h"

#include <QApplication>
#include <QVBoxLayout>
#include "Qt/QtMainFrame.h"
#include "QViewportPane.h"
#include "Qt/Widgets/QViewportHeader.h"
#include "LevelEditor/LevelEditorViewport.h"

class QViewportPaneContainer : public CDockableWidget
{
public:
	QViewportPaneContainer(QWidget* w, CViewport* pViewport)
		: CDockableWidget(nullptr)
		, m_pViewport(pViewport)
	{
		QVBoxLayout* layout = new QVBoxLayout;
		layout->setContentsMargins(0, 0, 0, 0);
		layout->setSpacing(0);
		setLayout(layout);
		layout->addWidget(w);
	}

	const char* GetPaneTitle() const
	{
		return m_pViewport->GetName().c_str();
	}

	virtual QVariantMap GetState() const
	{
		if (m_pViewport->IsRenderViewport())
		{
			CRenderViewport* viewport = static_cast<CRenderViewport*>(m_pViewport);

			QVariantMap state;
			state.insert("resolutionMode", static_cast<int>(viewport->GetResolutionMode()));
			int width, height;
			viewport->GetResolution(width, height);
			state.insert("xViewportRes", width);
			state.insert("yViewportRes", height);
			state.insert("cameraSpeed", viewport->GetCameraMoveSpeed());
			return state;
		}
		else if (m_pViewport->GetType() == ET_ViewportXY ||
		         m_pViewport->GetType() == ET_ViewportYZ ||
		         m_pViewport->GetType() == ET_ViewportXZ)
		{
			C2DViewport* viewport = static_cast<C2DViewport*>(m_pViewport);
			QVariantMap state;
			state.insert("showGrid", viewport->GetShowGrid());
			return state;
		}

		return QVariantMap();
	}

	// Restore transient state
	virtual void SetState(const QVariantMap& state)
	{
		if (m_pViewport->IsRenderViewport())
		{
			CRenderViewport* viewport = static_cast<CRenderViewport*>(m_pViewport);
			CViewport::EResolutionMode fitTowindow = CViewport::EResolutionMode::Window;
			QVariant fitVar = state.value("resolutionMode");

			if (fitVar.isValid())
			{
				fitTowindow = static_cast<CViewport::EResolutionMode>(fitVar.toInt());
			}
			// Check if current fitting scheme is same before changing because changing fires notifications
			if (fitTowindow != viewport->GetResolutionMode())
			{
				viewport->SetResolutionMode(fitTowindow);

				if (fitTowindow != CViewport::EResolutionMode::Window)
				{
					QVariant widthVar = state.value("xViewportRes");
					QVariant heightVar = state.value("yViewportRes");
					if (widthVar.isValid() && heightVar.isValid())
					{
						viewport->SetResolution(widthVar.toInt(), heightVar.toInt());
					}
				}
			}
			QVariant speedVar = state.value("cameraSpeed");
			if (speedVar.isValid())
			{
				double speed = speedVar.toFloat() - 0.01;
				int ivalue = 1;
				if (speed > 0.01)
				{
					// multiply by a hundred to get range. We do this to avoid negative logs for cameraMoveSpeed < 1.0
					// add 0.5 to fix floating point rounding errors
					ivalue = log((speed - 0.01) * 100) / log(1.1) + 0.5;
				}

				viewport->SetCameraMoveSpeedIncrements(ivalue);
			}
		}
		else if (m_pViewport->GetType() == ET_ViewportXY ||
		         m_pViewport->GetType() == ET_ViewportYZ ||
		         m_pViewport->GetType() == ET_ViewportXZ)
		{
			C2DViewport* viewport = static_cast<C2DViewport*>(m_pViewport);
			QVariant showVar = state.value("showGrid");
			if (showVar.isValid())
			{
				viewport->SetShowGrid(showVar.toBool());
			}
		}
	}

private:
	CViewport* m_pViewport;
};

//////////////////////////////////////////////////////////////////////////
IPane* CViewportClassDesc::CreatePane() const
{
	CViewport* pViewport = CreateViewport();
	pViewport->SetType(type);
	pViewport->SetName(name);
	QViewportPane* pViewportPane = new QViewportPane(pViewport, nullptr);
	QViewportPaneContainer* pViewportPaneContainer = new QViewportPaneContainer(pViewportPane, pViewport);
	return pViewportPaneContainer;
}

struct CViewportClassDesc_Top : public CViewportClassDesc
{
	CViewportClassDesc_Top() : CViewportClassDesc(ET_ViewportXY, "Top") {}
	virtual CViewport* CreateViewport() const { return new C2DViewport; };
};

struct CViewportClassDesc_Front : public CViewportClassDesc
{
	CViewportClassDesc_Front() : CViewportClassDesc(ET_ViewportXZ, "Front") {}
	virtual CViewport* CreateViewport() const { return new C2DViewport; };
};

struct CViewportClassDesc_Left : public CViewportClassDesc
{
	CViewportClassDesc_Left() : CViewportClassDesc(ET_ViewportYZ, "Left") {}
	virtual CViewport* CreateViewport() const { return new C2DViewport; };
};

struct CViewportClassDesc_Perspective : public CViewportClassDesc
{
	CViewportClassDesc_Perspective() : CViewportClassDesc(ET_ViewportCamera, "Perspective") {}
	virtual CViewport* CreateViewport() const { return new CLevelEditorViewport; };
	virtual IPane*         CreatePane() const override
	{
		CLevelEditorViewport* pViewport = (CLevelEditorViewport*)CreateViewport();
		QViewportHeader* pHeader = new QViewportHeader(pViewport);
		pViewport->SetType(type);
		pViewport->SetName(name);
		pViewport->SetHeaderWidget(pHeader);
		QViewportPane* pViewportPane = new QViewportPane(pViewport, pHeader);
		QViewportPaneContainer* pViewportPaneContainer = new QViewportPaneContainer(pViewportPane, pViewport);
		return pViewportPaneContainer;
	}
};

struct CViewportClassDesc_Map : public CViewportClassDesc
{
	CViewportClassDesc_Map() : CViewportClassDesc(ET_ViewportMap, "Map") {}
	virtual CViewport* CreateViewport() const { return new CTopRendererWnd; };
};

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
CViewManager::CViewManager()
{
	m_origin2D(0, 0, 0);
	m_zoom2D = 1.0f;

	m_cameraObjectId = CryGUID::Null();

	m_updateRegion.min = Vec3(-100000, -100000, -100000);
	m_updateRegion.max = Vec3(100000, 100000, 100000);

	m_pSelectedView = NULL;

	m_nGameViewports = 0;
	m_bGameViewportsUpdated = false;

	// Initialize viewport descriptions.
	RegisterViewportDesc(new CViewportClassDesc_Front);
	RegisterViewportDesc(new CViewportClassDesc_Left);
	RegisterViewportDesc(new CViewportClassDesc_Map);
	RegisterViewportDesc(new CViewportClassDesc_Perspective);
	RegisterViewportDesc(new CViewportClassDesc_Top);

	GetIEditorImpl()->RegisterNotifyListener(this);
}

//////////////////////////////////////////////////////////////////////////
CViewManager::~CViewManager()
{
	GetIEditorImpl()->UnregisterNotifyListener(this);

	m_viewports.clear();
	m_viewportDesc.clear();
}

//////////////////////////////////////////////////////////////////////////
void CViewManager::RegisterViewportDesc(CViewportClassDesc* desc)
{
	m_viewportDesc.push_back(desc);

	// Register ViewPane class.
	GetIEditorImpl()->GetClassFactory()->RegisterClass(desc);
}

//////////////////////////////////////////////////////////////////////////
void CViewManager::RegisterViewport(CViewport* pViewport)
{
	m_viewports.push_back(pViewport);

	// the type of added viewport can be changed later
	m_bGameViewportsUpdated = false;
}

//////////////////////////////////////////////////////////////////////////
void CViewManager::UnregisterViewport(CViewport* pViewport)
{
	if (m_pSelectedView == pViewport)
	{
		m_pSelectedView = NULL;
	}
	stl::find_and_erase(m_viewports, pViewport);
	m_bGameViewportsUpdated = false;
}

//////////////////////////////////////////////////////////////////////////
CViewport* CViewManager::GetViewport(EViewportType type) const
{
	////////////////////////////////////////////////////////////////////////
	// Returns the first view which has a render window of a specific
	// type attached
	////////////////////////////////////////////////////////////////////////
	for (int i = 0; i < m_viewports.size(); i++)
	{
		if (m_viewports[i]->GetType() == type)
			return m_viewports[i];
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
CViewport* CViewManager::GetViewport(const string& name) const
{
	for (int i = 0; i < m_viewports.size(); i++)
	{
		if (stricmp(m_viewports[i]->GetName().c_str(), name) == 0)
			return m_viewports[i];
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CViewManager::UpdateViews(int flags)
{
	// Update each attached view,
	for (int i = 0; i < m_viewports.size(); i++)
	{
		m_viewports[i]->UpdateContent(flags);
	}
}

//////////////////////////////////////////////////////////////////////////
void CViewManager::ResetViews()
{
	LOADING_TIME_PROFILE_SECTION;
	// Reset each attached view,
	for (int i = 0; i < m_viewports.size(); i++)
	{
		m_viewports[i]->ResetContent();
	}
}

//////////////////////////////////////////////////////////////////////////
void CViewManager::IdleUpdate()
{
	LOADING_TIME_PROFILE_SECTION;

	// Depending on user preferences, update active viewport solely
	if (gViewportPreferences.toolsRenderUpdateMutualExclusive)
	{
		if (GetActiveViewport()) GetActiveViewport()->Update();
		return;
	}

	// Update each attached view,
	for (int i = 0; i < m_viewports.size(); i++)
	{
		//if (m_viewports[i]->GetType() != ET_ViewportCamera || GetIEditorImpl()->GetDocument()->IsDocumentReady())
		m_viewports[i]->Update();
	}
}

//////////////////////////////////////////////////////////////////////////
void CViewManager::SetAxisConstrain(int axis)
{
	for (CViewport*& viewport : m_viewports)
	{
		viewport->SetAxisConstrain(axis);
	}

	signalAxisConstrainChanged(axis);
}

//////////////////////////////////////////////////////////////////////////
void CViewManager::GetViewportDescriptions(std::vector<CViewportClassDesc*>& descriptions)
{
	descriptions.clear();
	for (int i = 0; i < m_viewportDesc.size(); i++)
	{
		descriptions.push_back(m_viewportDesc[i]);
	}
}

void CViewManager::SetZoom2D(float zoom)
{
	m_zoom2D = zoom;
	if (m_zoom2D > 460.0f)
		m_zoom2D = 460.0f;
};

//////////////////////////////////////////////////////////////////////////
void CViewManager::Cycle2DViewport()
{
	//GetLayout()->Cycle2DViewport();
}

//////////////////////////////////////////////////////////////////////////
CViewport* CViewManager::GetViewportAtPoint(CPoint point) const
{
	QWidget* wnd = qApp->widgetAt(point.x, point.y);

	for (int i = 0; i < m_viewports.size(); i++)
	{
		if (m_viewports[i]->GetViewWidget() == wnd)
		{
			return m_viewports[i];
		}
	}
	return 0;
}

bool CViewManager::IsViewport(QWidget* w)
{
	if (!w)
	{
		return false;
	}

	if (strcmp(w->metaObject()->className(), "QViewport") == 0)
	{
		return true;
	}

	for (int i = 0; i < m_viewports.size(); i++)
	{
		if (m_viewports[i]->GetViewWidget() == w)
		{
			return true;
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CViewManager::SelectViewport(CViewport* pViewport)
{
	//TODO : this could be a property of CViewport, does not need to be here
	REINST("Handle viewport change for listeners");

	if (m_pSelectedView != pViewport)
	{
		if (m_pSelectedView != NULL)
		{
			if (gEnv->pGameFramework && m_pSelectedView->GetType() == ET_ViewportCamera)
			{
				// This is the main view, here we inform the ViewSystem as well.
				IViewSystem* const pIViewSystem = gEnv->pGameFramework->GetIViewSystem();

				if (pIViewSystem != NULL)
				{
					pIViewSystem->SetControlAudioListeners(false);
				}
			}
		}

		m_pSelectedView = pViewport;

		if (m_pSelectedView != NULL)
		{
			if (m_pSelectedView->GetType() == ET_ViewportCamera)
			{
				// This is the main view, here we inform the ViewSystem as well.
				if (gEnv->pGameFramework)
				{
					if (IViewSystem* const pIViewSystem = gEnv->pGameFramework->GetIViewSystem())
					{
						pIViewSystem->SetControlAudioListeners(true);
					}
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
CViewport* CViewManager::GetActiveViewport() const
{
	return m_pSelectedView;
}

//////////////////////////////////////////////////////////////////////////
CViewport* CViewManager::GetGameViewport() const
{
	return GetViewport(ET_ViewportCamera);
}

//////////////////////////////////////////////////////////////////////////
int CViewManager::GetNumberOfGameViewports()
{
	if (m_bGameViewportsUpdated)
		return m_nGameViewports;

	m_nGameViewports = 0;
	for (int i = 0; i < m_viewports.size(); ++i)
	{
		if (m_viewports[i]->GetType() == ET_ViewportCamera)
			++m_nGameViewports;
	}
	m_bGameViewportsUpdated = true;

	return m_nGameViewports;
}

//////////////////////////////////////////////////////////////////////////
void CViewManager::OnEditorNotifyEvent(EEditorNotifyEvent event)
{
	switch (event)
	{
	case eNotify_OnEndLoad:
	case eNotify_OnIdleUpdate:
		IdleUpdate();
		break;
	case eNotify_OnUpdateViewports:
		UpdateViews();
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
string CViewManager::ViewportTypeToClassName(EViewportType viewType)
{
	string viewClassName;
	for (int i = 0; i < m_viewportDesc.size(); i++)
	{
		if (m_viewportDesc[i]->type == viewType)
			viewClassName = m_viewportDesc[i]->ClassName();
	}
	return viewClassName;
}

//////////////////////////////////////////////////////////////////////////
bool CViewManager::TryResize(CViewport* viewport, int width, int height, bool bMaximize)
{
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CViewManager::RegisterCameraDelegate(ICameraDelegate* pCameraDelegate)
{
	stl::push_back_unique(m_cameraDelegates, pCameraDelegate);
}

//////////////////////////////////////////////////////////////////////////
void CViewManager::UnregisterCameraDelegate(ICameraDelegate* pCameraDelegate)
{
	stl::find_and_erase(m_cameraDelegates, pCameraDelegate);
}

