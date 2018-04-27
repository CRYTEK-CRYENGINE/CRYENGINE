// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "EditorCommonAPI.h"
#include "RenderViewport.h"
#include "IEditorMaterial.h"
#include <CryAnimation/ICryAnimation.h>
#include <CryInput/IInput.h>
#include <CryEntitySystem/IEntitySystem.h>
#include "Util/Variable.h"

struct IPhysicalEntity;
struct CryCharAnimationParams;
struct ISkeletonAnim;
class CAnimationSet;

/////////////////////////////////////////////////////////////////////////////
// CModelViewport window
class EDITOR_COMMON_API CModelViewport : public CRenderViewport, public IInputEventListener
{
public:
	CModelViewport(const char* settingsPath = R"(Settings/CharacterEditorUserOptions)");
	virtual ~CModelViewport() override;

	static bool           IsPreviewableFileType(const char* szPath);

	virtual EViewportType GetType() const override             { return ET_ViewportModel; }
	virtual void          SetType(EViewportType type) override { assert(type == ET_ViewportModel); };

	virtual void          LoadObject(const string& obj, float scale = 1.0f);

	virtual bool          CanDrop(CPoint point, IDataBaseItem* pItem);
	virtual void          Drop(CPoint point, IDataBaseItem* pItem);

	void                  AttachObjectToBone(const string& model, const string& bone);
	void                  AttachObjectToFace(const string& model);

	// Callbacks.
	void                OnShowShaders(IVariable* var);
	void                OnShowNormals(IVariable* var);
	void                OnShowTangents(IVariable* var);

	void                OnShowPortals(IVariable* var);
	void                OnShowShadowVolumes(IVariable* var);
	void                OnShowTextureUsage(IVariable* var);
	void                OnCharPhysics(IVariable* var);
	void                OnShowOcclusion(IVariable* var);

	void                OnLightColor(IVariable* var);
	void                OnLightMultiplier(IVariable* var);
	void                OnDisableVisibility(IVariable* var);

	void                OnSubmeshSetChanged();
	ICharacterInstance* GetCharacterBase()
	{
		return m_pCharacterBase;
	}

	IStatObj*         GetStaticObject() { return m_object; }

	void              GetOnDisableVisibility(IVariable* var);

	const CVarObject* GetVarObject() const { return &m_vars; }
	CVarObject*       GetVarObject()       { return &m_vars; }

	virtual void      Update() override;

	void              UseWeaponIK(bool val) { m_weaponIK = true; }

	// Set current material to render object.
	void               SetCustomMaterial(IEditorMaterial* pMaterial);
	// Get custom material that object is rendered with.
	IEditorMaterial*   GetCustomMaterial()  { return m_pCurrentMaterial; };

	ICharacterManager* GetAnimationSystem() { return m_pAnimationSystem; };

	// Get material the object is actually rendered with.
	IEditorMaterial* GetMaterial();

	void             ReleaseObject();
	void             RePhysicalize();

	Vec3                           m_GridOrigin;
	_smart_ptr<ICharacterInstance> m_pCharacterBase;

	void          SetPaused(bool bPaused);
	bool          GetPaused()              { return m_bPaused; }

	bool          IsCameraAttached() const { return mv_AttachCamera; }

	virtual void  PlayAnimation(const char* szName);

	const string& GetLoadedFileName() const { return m_loadedFile; }

	void          Physicalize();

protected:
	void LoadStaticObject(const string& file);

	// Called to render stuff.
	virtual void OnRender() override;

	virtual void DrawFloorGrid(const Quat& tmRotation, const Vec3& MotionTranslation, const Matrix33& rGridRot, bool bInstantSubmit);
	void         DrawCoordSystem(const QuatT& q, f32 length);

	void         SaveDebugOptions() const;
	void         RestoreDebugOptions();

	virtual void DrawModel(const SRenderingPassInfo& passInfo);
	virtual void DrawLights(const SRenderingPassInfo& passInfo);
	virtual void DrawSkyBox(const SRenderingPassInfo& passInfo);
	virtual bool UseAnimationDrivenMotion() const;

	//This implementation is dangerous. If we change or rename the specialized function, we use this as fallback and don't execute anything
	virtual void DrawCharacter(ICharacterInstance* pInstance, const SRendParams& rp, const SRenderingPassInfo& pass) = 0;

	void         SetConsoleVar(const char* var, int value);

	void         OnEditorNotifyEvent(EEditorNotifyEvent event)
	{
		if (event != eNotify_OnBeginGameMode)
			CRenderViewport::OnEditorNotifyEvent(event);
	}

	//virtual bool OnInputEvent( const SInputEvent &event ) = 0;
	virtual bool OnInputEvent(const SInputEvent& rInputEvent) override;

	f32                            m_RT;
	Vec2                           m_LTHUMB;
	Vec2                           m_RTHUMB;
	QuatTS                         m_PhysicalLocation;
	f32                            m_absCurrentSlope; //in radiants

	Vec2                           m_arrLTHUMB[0x100];

	IStatObj*                      m_object;
	IStatObj*                      m_weaponModel;
	// this is the character to attach, instead of weaponModel
	_smart_ptr<ICharacterInstance> m_attachedCharacter;
	string                         m_attachBone;

	AABB                           m_AABB;

	struct BBox { OBB obb; Vec3 pos; ColorB col;  };
	std::vector<BBox> m_arrBBoxes;
	std::vector<Vec3> m_gridLineVertices;

	// Camera control.
	float m_camRadius;

	// True to show grid.
	bool                        m_bBase;

	string                      m_settingsPath;

	bool                        m_weaponIK;

	ICharacterManager*          m_pAnimationSystem;

	string                      m_loadedFile;
	std::vector<SRenderLight>   m_VPLights;

	f32                         m_LightRotationRadian;

	class CRESky*               m_pRESky;
	struct ICVar*               m_pSkyboxName;
	IShader*                    m_pSkyBoxShader;
	_smart_ptr<IEditorMaterial> m_pCurrentMaterial;

	CryAudio::IListener*        m_pIAudioListener;

	//---------------------------------------------------
	//---    debug options                            ---
	//---------------------------------------------------
	CVariable<bool>  mv_showGrid;
	CVariable<bool>  mv_showBase;
	CVariable<bool>  mv_showLocator;
	CVariable<bool>  mv_InPlaceMovement;
	CVariable<bool>  mv_StrafingControl;

	CVariable<bool>  mv_showWireframe1; //draw wireframe instead of solid-geometry.
	CVariable<bool>  mv_showWireframe2; //this one is software-wireframe rendered on top of the solid geometry
	CVariable<bool>  mv_showTangents;
	CVariable<bool>  mv_showBinormals;
	CVariable<bool>  mv_showNormals;

	CVariable<bool>  mv_showSkeleton;
	CVariable<bool>  mv_showJointNames;
	CVariable<bool>  mv_showJointsValues;
	CVariable<bool>  mv_showStartLocation;
	CVariable<bool>  mv_showMotionParam;
	CVariable<float> mv_UniformScaling;

	CVariable<bool>  mv_printDebugText;
	CVariable<bool>  mv_AttachCamera;

	CVariable<bool>  mv_showShaders;

	CVariable<bool>  mv_lighting;
	CVariable<bool>  mv_animateLights;

	CVariable<Vec3>  mv_backgroundColor;
	CVariable<Vec3>  mv_objectAmbientColor;

	CVariable<Vec3>  mv_lightDiffuseColor0;
	CVariable<Vec3>  mv_lightDiffuseColor1;
	CVariable<Vec3>  mv_lightDiffuseColor2;
	CVariable<float> mv_lightMultiplier;
	CVariable<float> mv_lightSpecMultiplier;
	CVariable<float> mv_lightRadius;
	CVariable<float> mv_lightOrbit;

	CVariable<float> mv_fov;
	CVariable<bool>  mv_showPhysics;
	CVariable<bool>  mv_useCharPhysics;
	CVariable<bool>  mv_showPhysicsTetriders;
	CVariable<int>   mv_forceLODNum;

	CVariableArray   mv_advancedTable;

	CVarObject       m_vars;

public:
	IPhysicalEntity* m_pPhysicalEntity;
protected:

	bool m_bPaused;

	virtual bool OnKeyDown(uint32 nChar, uint32 nRepCnt, uint32 nFlags) override;

	// Generated message map functions
	afx_msg int  OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
	afx_msg void OnDestroy();
};

