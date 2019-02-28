// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "IViewportManager.h"
#include "Viewport.h"
#include "QtViewPane.h"

// forward declaration.
class CViewport;
class QWidget;

// Description of viewport.
class CViewportClassDesc : public IViewPaneClass
{
public:
	EViewportType type; //!< Type of viewport.
	string        name; //!< Name of viewport.
public:
	CViewportClassDesc(EViewportType inType, const string& inName)
	{
		name = inName;
		type = inType;
		m_className = string("ViewportClass_") + inName;
	}
	//////////////////////////////////////////////////////////////////////////
	// IClassDesc
	//////////////////////////////////////////////////////////////////////////
	virtual ESystemClassID SystemClassID()   { return ESYSTEM_CLASS_VIEWPANE; };
	virtual const char*    ClassName()       { return m_className.c_str(); };
	virtual const char*    Category()        { return "Viewport"; };
	virtual const char*    GetMenuPath()     { return "Viewport"; };
	//////////////////////////////////////////////////////////////////////////
	virtual CRuntimeClass* GetRuntimeClass() { return 0; };
	virtual const char*    GetPaneTitle()    { return name.c_str(); };
	virtual bool           SinglePane()      { return false; };
	virtual IPane*         CreatePane() const override;
	//////////////////////////////////////////////////////////////////////////

	virtual CViewport* CreateViewport() const = 0;
private:
	string m_className;
};

/** Manages set of viewports.
 */
class SANDBOX_API CViewManager : public IViewportManager, public IEditorNotifyListener
{
public:
	CViewport* GetViewport(EViewportType type) const;
	CViewport* GetViewport(const string& name) const;

	// Return currently active viewport.
	CViewport* GetActiveViewport() const;

	//! Find viewport at Screen point.
	CViewport* GetViewportAtPoint(CPoint point) const;
	bool       IsViewport(QWidget* w);
	void       SelectViewport(CViewport* pViewport);
	CViewport* GetSelectedViewport() const { return m_pSelectedView; }

	void       SetAxisConstrain(int axis);

	//! Reset all views.
	void        ResetViews();
	//! Update all views.
	void        UpdateViews(int flags = 0xFFFFFFFF);

	void        SetUpdateRegion(const AABB& updateRegion) { m_updateRegion = updateRegion; }
	const AABB& GetUpdateRegion()                         { return m_updateRegion; }

	/** Get 2D viewports origin.
	 */
	Vec3 GetOrigin2D() const { return m_origin2D; }

	/** Assign 2D viewports origin.
	 */
	void SetOrigin2D(const Vec3& org) { m_origin2D = org; }

	/** Assign zoom factor for 2d viewports.
	 */
	void SetZoom2D(float zoom) override;

	/** Get zoom factor of 2d viewports.
	 */
	float GetZoom2D() const override { return m_zoom2D; }

	//////////////////////////////////////////////////////////////////////////
	//! Get currently active camera object id.
	CryGUID GetCameraObjectId() const override                { return m_cameraObjectId; }
	//! Sets currently active camera object id.
	void    SetCameraObjectId(CryGUID cameraObjectId) override { m_cameraObjectId = cameraObjectId; }

	//////////////////////////////////////////////////////////////////////////
	//! Add new viewport description to view manager.
	virtual void RegisterViewportDesc(CViewportClassDesc* desc);
	//! Get viewport descriptions.
	virtual void GetViewportDescriptions(std::vector<CViewportClassDesc*>& descriptions);

	string       ViewportTypeToClassName(EViewportType viewType);

	//////////////////////////////////////////////////////////////////////////
	//! Get number of currently existing viewports.
	virtual int GetViewCount() { return m_viewports.size(); }
	//! Get viewport by index.
	//! @param index 0 <= index < GetViewportCount()
	virtual CViewport* GetView(int index) { return m_viewports[index]; }

	//! Cycle between different 2D viewports type on same view pane.
	virtual void Cycle2DViewport();

	//////////////////////////////////////////////////////////////////////////
	// Retrieve main game viewport, where the full game is rendered in 3D.
	CViewport* GetGameViewport() const override;

	//! Get number of Game Viewports
	int                                  GetNumberOfGameViewports() override;

	virtual void                         OnEditorNotifyEvent(EEditorNotifyEvent event);

	void                                 RegisterCameraDelegate(ICameraDelegate* pCameraDelegate);
	void                                 UnregisterCameraDelegate(ICameraDelegate* pCameraDelegate);

	const std::vector<ICameraDelegate*>& GetCameraDelegates() const { return m_cameraDelegates; }
	// Try to resize viewport for the desired width/height.
	bool                                 TryResize(CViewport* viewport, int width, int height, bool bMaximize = false);

private:
	friend class CEditorImpl;
	friend class CViewport;

	CViewManager();
	~CViewManager();

	void IdleUpdate();
	void RegisterViewport(CViewport* vp);
	void UnregisterViewport(CViewport* vp);

public:
	CCrySignal<void(int)> signalAxisConstrainChanged;

private:
	//////////////////////////////////////////////////////////////////////////
	//FIELDS.
	AABB                         m_updateRegion;

	//! Origin of 2d viewports.
	Vec3  m_origin2D;
	//! Zoom of 2d viewports.
	float m_zoom2D;

	//! Id of camera object.
	CryGUID m_cameraObjectId;

	int  m_nGameViewports;
	bool m_bGameViewportsUpdated;

	//! Array of viewport descriptions.
	std::vector<CViewportClassDesc*> m_viewportDesc;
	//! Array of currently existing viewports.
	std::vector<CViewport*>                    m_viewports;

	CViewport*                    m_pSelectedView;

	std::vector<ICameraDelegate*> m_cameraDelegates;
};

