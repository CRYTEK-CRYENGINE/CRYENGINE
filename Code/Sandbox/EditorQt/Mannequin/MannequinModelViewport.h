// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __MANNEQUIN_MODEL_VIEWPORT_H__
#define __MANNEQUIN_MODEL_VIEWPORT_H__

#if _MSC_VER > 1000
	#pragma once
#endif

#include "ModelViewport.h"
#include "MannequinBase.h"
#include "ICryMannequinEditor.h"
#include "SequencerDopeSheetBase.h"
#include "Util/ArcBall.h"
#include "Gizmos/AxisHelper.h"

class IActionController;
class CMannequinDialog;

//////////////////////////////////////////////////////////////////////////
class CMannequinModelViewport : public CModelViewport, public IParticleEffectListener, public IMannequinGameListener, public CActionInputHandler
{
public:

	enum ELocatorMode
	{
		LM_Translate,
		LM_Rotate,
	};

	enum EMannequinViewMode
	{
		eVM_Unknown     = 0,
		eVM_FirstPerson = 1,
		eVM_ThirdPerson = 2
	};

	CMannequinModelViewport(EMannequinEditorMode editorMode = eMEM_FragmentEditor);
	virtual ~CMannequinModelViewport();

	virtual void Update();
	void         UpdateAnimation(float timePassed);

	virtual void OnRender();

	void         SetActionController(IActionController* pActionController)
	{
		m_pActionController = pActionController;
	}

	void ToggleCamera()
	{
		mv_AttachCamera = !mv_AttachCamera;

		CCamera oldCamera(GetCamera());
		SetCamera(m_alternateCamera);
		m_alternateCamera = oldCamera;
	}

	bool          IsCameraAttached() const { return mv_AttachCamera; }

	void          ClearLocators();
	void          AddLocator(uint32 refID, const char* name, const QuatT& transform, IEntity* pEntity = NULL, int16 refJointId = -1, IAttachment* pAttachment = NULL, uint32 paramCRC = 0, string helperName = "");

	const QuatTS& GetPhysicalLocation() const
	{
		return m_PhysicalLocation;
	}

	virtual void DrawCharacter(ICharacterInstance* pInstance, const SRendParams& rRP, const SRenderingPassInfo& passInfo) override;

	//--- Override base level as we don't want the animation control there
	void SetPlaybackMultiplier(float multiplier)
	{
		m_bPaused = (multiplier == 0.0f);
		m_playbackMultiplier = multiplier;
	}

	void OnScrubTime(float timePassed);
	void OnSequenceRestart(float timePassed);
	void UpdateDebugParams();

	void OnLoadPreviewFile()
	{
		if (m_attachCameraToEntity)
		{
			m_attachCameraToEntity = NULL;
			AttachToEntity();
		}

		m_viewmode = eVM_Unknown;
		m_pHoverBaseObject = NULL;
	}

	void ClearCharacters()
	{
		m_entityList.resize(0);
	}

	void AddCharacter(IEntity* entity, const QuatT& startPosition)
	{
		m_entityList.push_back(SCharacterEntity(entity, startPosition));
	}

	static int   OnPostStepLogged(const EventPhys* pEvent);

	virtual void OnCreateEmitter(IParticleEmitter* pEmitter) override;
	virtual void OnDeleteEmitter(IParticleEmitter* pEmitter) override;
	virtual void OnSpawnParticleEmitter(IParticleEmitter* pEmitter, IActionController& actionController) override;
	void         SetTimelineUnits(ESequencerTickMode mode);

	bool         ToggleLookingAtCamera()                { (m_lookAtCamera = !m_lookAtCamera); return m_lookAtCamera; }
	void         SetLocatorRotateMode()                 { m_locMode = LM_Rotate; }
	void         SetLocatorTranslateMode()              { m_locMode = LM_Translate; }
	void         SetShowSceneRoots(bool showSceneRoots) { m_showSceneRoots = showSceneRoots; }
	void         AttachToEntity()
	{
		if (m_entityList.size() > 0)
		{
			m_attachCameraToEntity = m_entityList[0].GetEntity();
			if (m_attachCameraToEntity != NULL)
			{
				m_lastEntityPos = m_attachCameraToEntity->GetPos();
			}
		}
	}
	void DetachFromEntity() { m_attachCameraToEntity = NULL; }

	void Focus(AABB& pBoundingBox);

	bool IsLookingAtCamera() const      { return m_lookAtCamera; }
	bool IsTranslateLocatorMode() const { return (LM_Translate == m_locMode); }
	bool IsAttachedToEntity() const     { return NULL != m_attachCameraToEntity; }
	bool IsShowingSceneRoots() const    { return m_showSceneRoots; }

	void SetPlayerPos();

protected:

	bool         UseAnimationDrivenMotionForEntity(const IEntity* piEntity);
	void         SetFirstperson(IAttachmentManager* pAttachmentManager, EMannequinViewMode viewmode);
	virtual BOOL PreTranslateMessage(MSG* pMsg);

	bool         HitTest(HitContext& hc, const bool bIsClick);

	//////////////////////////////////////////////////////////////////////////
	// Message Handlers
	//////////////////////////////////////////////////////////////////////////

	void OnLButtonDown(UINT nFlags, CPoint point);
	void OnLButtonUp(UINT nFlags, CPoint point);
	void OnMouseMove(UINT nFlags, CPoint point);

	void DrawGrid(const Quat& tmRotation, const Vec3& MotionTranslation, const Vec3& FootSlide, const Matrix33& rGridRot) {};

private:

	void UpdateCharacter(IEntity* pEntity, ICharacterInstance* pInstance, float deltaTime);
	void DrawCharacter(IEntity* pEntity, ICharacterInstance* pInstance, IStatObj* pStatObj, const SRendParams& rRP, const SRenderingPassInfo& passInfo);
	void DrawEntityAndChildren(class CEntityObject* pEntityObject, const SRendParams& rp, const SRenderingPassInfo& passInfo);
	void UpdatePropEntities(SMannequinContexts* pContexts, SMannequinContexts::SProp& prop);

	struct SLocator
	{
		uint32                  m_refID;
		CString                 m_name;
		CArcBall3D              m_ArcBall;
		CAxisHelper             m_AxisHelper;
		IEntity*                m_pEntity;
		_smart_ptr<IAttachment> m_pAttachment;
		string                  m_helperName;
		int16                   m_jointId;
		uint32                  m_paramCRC;
	};

	inline Matrix34 GetLocatorReferenceMatrix(const SLocator& locator);
	inline Matrix34 GetLocatorWorldMatrix(const SLocator& locator);

	ELocatorMode m_locMode;

	struct SCharacterEntity
	{
		SCharacterEntity(IEntity* _entity = NULL, const QuatT& _startLocation = QuatT(IDENTITY))
			:
			entityId(_entity ? _entity->GetId() : 0),
			startLocation(_startLocation)
		{}
		EntityId entityId;
		QuatT    startLocation;

		IEntity* GetEntity() const { return gEnv->pEntitySystem->GetEntity(entityId); }
	};
	std::vector<SCharacterEntity>  m_entityList;

	std::vector<IParticleEmitter*> m_particleEmitters;

	std::vector<SLocator>          m_locators;
	uint32                         m_selectedLocator;
	EMannequinViewMode             m_viewmode;
	bool                           m_draggingLocator;
	CPoint                         m_dragStartPoint;
	bool                           m_LeftButtonDown;
	bool                           m_lookAtCamera;
	bool                           m_showSceneRoots;
	bool                           m_cameraKeyDown;
	float                          m_playbackMultiplier;
	Vec3                           m_tweenToFocusStart;
	Vec3                           m_tweenToFocusDelta;
	float                          m_tweenToFocusTime;
	static const float             s_maxTweenTime;

	EMannequinEditorMode           m_editorMode;

	IActionController*             m_pActionController;

	IPhysicalEntity*               m_piGroundPlanePhysicalEntity;

	ESequencerTickMode             m_TickerMode;

	IEntity*                       m_attachCameraToEntity;
	Vec3                           m_lastEntityPos;

	CCamera                        m_alternateCamera;

	CBaseObject*                   m_pHoverBaseObject;

	HitContext                     m_HitContext;
	int                            m_highlightedBoneID;

	//-------------------------------------------
	//---   player-control in CharEdit        ---
	//-------------------------------------------
	f32                  m_MoveSpeedMSec;

	uint32               m_key_W, m_keyrcr_W;
	uint32               m_key_S, m_keyrcr_S;
	uint32               m_key_A, m_keyrcr_A;
	uint32               m_key_D, m_keyrcr_D;

	uint32               m_key_SPACE, m_keyrcr_SPACE;
	uint32               m_ControllMode;

	int32                m_Stance;
	int32                m_State;
	f32                  m_AverageFrameTime;

	uint32               m_PlayerControl;

	f32                  m_absCameraHigh;
	Vec3                 m_absCameraPos;
	Vec3                 m_absCameraPosVP;

	f32                  m_absCurrentSlope; //in radiants

	Vec2                 m_absLookDirectionXY;

	Vec3                 m_LookAt;
	Vec3                 m_LookAtRate;
	Vec3                 m_vCamPos;
	Vec3                 m_vCamPosRate;
	float                m_camFOV;

	f32                  m_relCameraRotX;
	f32                  m_relCameraRotZ;

	QuatTS               m_PhysicalLocation;

	Matrix34             m_AnimatedCharacterMat;

	Matrix34             m_LocalEntityMat; //this is used for data-driven animations where the character is running on the spot
	Matrix34             m_PrevLocalEntityMat;

	std::vector<Vec3>    m_arrVerticesHF;
	std::vector<vtx_idx> m_arrIndicesHF;

	std::vector<Vec3>    m_arrAnimatedCharacterPath;
	std::vector<Vec3>    m_arrSmoothEntityPath;
	std::vector<f32>     m_arrRunStrafeSmoothing;

	Vec2                 m_vWorldDesiredBodyDirection;
	Vec2                 m_vWorldDesiredBodyDirectionSmooth;
	Vec2                 m_vWorldDesiredBodyDirectionSmoothRate;

	Vec2                 m_vWorldDesiredBodyDirection2;

	Vec2                 m_vWorldDesiredMoveDirection;
	Vec2                 m_vWorldDesiredMoveDirectionSmooth;
	Vec2                 m_vWorldDesiredMoveDirectionSmoothRate;
	Vec2                 m_vLocalDesiredMoveDirection;
	Vec2                 m_vLocalDesiredMoveDirectionSmooth;
	Vec2                 m_vLocalDesiredMoveDirectionSmoothRate;
	Vec2                 m_vWorldAimBodyDirection;

	f32                  m_udGround;
	f32                  m_lrGround;
	OBB                  m_GroundOBB;
	Vec3                 m_GroundOBBPos;

};

#endif // __MANNEQUIN_MODEL_VIEWPORT_H__

