// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
////////////////////////////////////////////////////////////////////////////
//  CryEngine Source File.
//  Copyright (C), Crytek, 1999-2013
////////////////////////////////////////////////////////////////////////////

struct IEntity;
struct IParticleEffect;
struct IEditorMaterial;
struct IRenderNode;
struct IStatObj;
struct SRenderingPassInfo;

#include <CryMath/Cry_Camera.h>
#include <CryRenderer/IRenderer.h>
#include "Objects/DisplayContext.h"

class PLUGIN_API CPreviewModelCtrl : public CWnd, public IEditorNotifyListener
{
public:
	CPreviewModelCtrl();
	virtual BOOL PreTranslateMessage(MSG* pMsg);

public:
	BOOL       Create(CWnd* pWndParent, const CRect& rc, DWORD dwStyle = WS_CHILD | WS_VISIBLE, UINT nID = 0);
	void       LoadFile(const char* modelFile, bool changeCamera = true);
	void       LoadParticleEffect(IParticleEffect* pEffect);
	Vec3       GetSize() const       { return m_size; };
	const char* GetLoadedFile() const { return m_loadedFile.GetString(); }

	void       SetEntity(IRenderNode* entity);
	void       SetObject(IStatObj* pObject);
	IStatObj*  GetObject() { return m_pObj; }
	void       SetCameraLookAt(float fRadiusScale, const Vec3& dir = Vec3(0, 1, 0));
	void       SetCameraRadius(float fRadius);
	CCamera&   GetCamera();
	void       SetGrid(bool bEnable) { m_bGrid = bEnable; }
	void       SetAxis(bool bEnable) { m_bAxis = bEnable; }
	void       SetRotation(bool bEnable);
	void       SetClearColor(const ColorF& color);
	void       SetBackgroundTexture(const CString& textureFilename);
	void       UseBackLight(bool bEnable);
	bool       UseBackLight() const          { return m_bUseBacklight; }
	void       SetShowNormals(bool bShow)    { m_bShowNormals = bShow; }
	void       SetShowPhysics(bool bShow)    { m_bShowPhysics = bShow; }
	void       SetShowRenderInfo(bool bShow) { m_bShowRenderInfo = bShow; }

	void       EnableUpdate(bool bEnable);
	bool       IsUpdateEnabled() const { return m_bUpdate; }
	void       Update(bool bForceUpdate = false);
	bool       CheckVirtualKey(int virtualKey);
	void       ProcessKeys();

	void       SetMaterial(IEditorMaterial* pMaterial);
	IEditorMaterial* GetMaterial();

	void       GetImage(CImageEx& image);
	void       GetImageOffscreen(CImageEx& image, const CSize& customSize = CSize(0, 0));

	void       GetCameraTM(Matrix34& cameraTM);
	void       SetCameraTM(const Matrix34& cameraTM);

	// Place camera so that whole object fits on screen.
	void FitToScreen();

	// Get information about the preview model.
	int  GetFaceCount();
	int  GetVertexCount();
	int  GetMaxLod();
	int  GetMtlCount();

	void UpdateAnimation();

	void SetShowObject(bool bShowObject)      { m_bShowObject = bShowObject; }
	bool GetShowObject()                      { return m_bShowObject; }

	void SetAmbient(ColorF amb)               { m_ambientColor = amb; }
	void SetAmbientMultiplier(f32 multiplier) { m_ambientMultiplier = multiplier; }

	// Get the character instance.
	ICharacterInstance* GetCharacter() { return m_pCharacter; }

	typedef void (* CameraChangeCallback)(void* m_userData, CPreviewModelCtrl* m_currentCamera);
	void SetCameraChangeCallback(CameraChangeCallback callback, void* userData) { m_cameraChangeCallback = callback, m_pCameraChangeUserData = userData; }

	void EnableMaterialPrecaching(bool bPrecacheMaterial)                       { m_bPrecacheMaterial = bPrecacheMaterial; }
	void EnableWireframeRendering(bool bDrawWireframe)                          { m_bDrawWireFrame = bDrawWireframe; }

public:
	virtual ~CPreviewModelCtrl();

	bool CreateRenderContext();
	void DestroyRenderContext();
	void InitDisplayContext(HWND hWnd);

	void ReleaseObject();

protected:
	//{{AFX_MSG(CPreviewModelCtrl)
	afx_msg virtual int  OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void         OnPaint();
	afx_msg BOOL         OnEraseBkgnd(CDC* pDC);
	afx_msg void         OnTimer(UINT_PTR nIDEvent);
	afx_msg void         OnDestroy();
	afx_msg void         OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void         OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg BOOL         OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
	afx_msg virtual void OnSize(UINT nType, int cx, int cy);
	afx_msg virtual void OnMButtonDown(UINT nFlags, CPoint point);
	afx_msg virtual void OnMButtonUp(UINT nFlags, CPoint point);
	afx_msg virtual void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg virtual void OnRButtonUp(UINT nFlags, CPoint point);
	afx_msg virtual void OnRButtonDown(UINT nFlags, CPoint point);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

	virtual void OnEditorNotifyEvent(EEditorNotifyEvent event);

protected:
	virtual bool Render();
	virtual void SetCamera(CCamera& cam);

	virtual void RenderObject(IMaterial* pMaterial, const SRenderingPassInfo& passInfo);
	virtual void RenderEffect(IMaterial* pMaterial, const SRenderingPassInfo& passInfo);

	float          m_fov;
	CCamera        m_camera;
	DisplayContext m_displayContext;

	bool RenderInternal();
	void SetOrbitAngles(const Ang3& ang);
	void DrawGrid();
	void DrawBackground();

	_smart_ptr<IStatObj>     m_pObj;
	ICharacterInstance*      m_pCharacter;

	IRenderer*               m_pRenderer;
	ICharacterManager*       m_pAnimationSystem;
	bool                     m_renderContextCreated;

	Vec3                     m_size;
	Vec3                     m_pos;
	int                      m_nTimer;

	CString                  m_loadedFile;
	std::vector<SRenderLight> m_lights;

	AABB                     m_aabb;
	Vec3                     m_cameraTarget;
	float                    m_cameraRadius;
	Vec3                     m_cameraAngles;
	bool                     m_bInRotateMode;
	bool                     m_bInMoveMode;
	CPoint                   m_mousePosition;
	IRenderNode*             m_pEntity;
	struct IParticleEmitter* m_pEmitter;
	bool                     m_bHaveAnythingToRender;
	bool                     m_bGrid;
	bool                     m_bAxis;
	bool                     m_bUpdate;
	bool                     m_bRotate;
	float                    m_rotateAngle;
	ColorF                   m_clearColor;
	ColorF                   m_ambientColor;
	f32                      m_ambientMultiplier;
	bool                     m_bUseBacklight;
	bool                     m_bShowObject;
	bool                     m_bPrecacheMaterial;
	bool                     m_bDrawWireFrame;
	bool                     m_bShowNormals;
	bool                     m_bShowPhysics;
	bool                     m_bShowRenderInfo;
	int                      m_backgroundTextureId;
	float                    m_tileX;
	float                    m_tileY;
	float                    m_tileSizeX;
	float                    m_tileSizeY;
	_smart_ptr<IEditorMaterial>    m_pCurrentMaterial;
	CameraChangeCallback     m_cameraChangeCallback;
	void*                    m_pCameraChangeUserData;
	int                      m_physHelpers0;

private:
	SDisplayContextKey m_displayContextKey;

protected:
	virtual void PreSubclassWindow();
};

