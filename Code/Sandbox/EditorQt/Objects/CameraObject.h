// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __cameraobject_h__
#define __cameraobject_h__

#if _MSC_VER > 1000
	#pragma once
#endif

#include "EntityObject.h"

class ICameraObjectListener
{
public:
	virtual void OnFovChange(const float fov)                                                                                                      {}
	virtual void OnNearZChange(const float nearZ)                                                                                                  {}
	virtual void OnFarZChange(const float farZ)                                                                                                    {}
	virtual void OnOmniCameraChange(const bool isOmni)                                                                                             {}
	virtual void OnShakeAmpAChange(const Vec3 amplitude)                                                                                           {}
	virtual void OnShakeAmpBChange(const Vec3 amplitude)                                                                                           {}
	virtual void OnShakeFreqAChange(const Vec3 frequency)                                                                                          {}
	virtual void OnShakeFreqBChange(const Vec3 frequency)                                                                                          {}
	virtual void OnShakeMultChange(const float amplitudeAMult, const float amplitudeBMult, const float frequencyAMult, const float frequencyBMult) {}
	virtual void OnShakeNoiseChange(const float noiseAAmpMult, const float noiseBAmpMult, const float noiseAFreqMult, const float noiseBFreqMult)  {}
	virtual void OnShakeWorkingChange(const float timeOffsetA, const float timeOffsetB)                                                            {}
	virtual void OnCameraShakeSeedChange(const int seed)                                                                                           {}
};

/*!
 *	CCameraObject is an object that represent Source or Target of camera.
 *
 */
class SANDBOX_API CCameraObject : public CEntityObject
{
public:
	DECLARE_DYNCREATE(CCameraObject)

	//////////////////////////////////////////////////////////////////////////
	// Ovverides from CBaseObject.
	//////////////////////////////////////////////////////////////////////////
	bool   Init(CBaseObject* prev, const string& file);
	void   InitVariables();
	void   Done();
	string GetTypeDescription() const { return GetTypeName(); };
	void   Display(CObjectRenderHelper& objRenderHelper) override;

	virtual void CreateInspectorWidgets(CInspectorWidgetCreator& creator) override;

	//////////////////////////////////////////////////////////////////////////
	virtual void SetName(const string& name);
	bool         IsScalable() const override { return false; }

	//! Called when object is being created.
	int  MouseCreateCallback(IDisplayViewport* view, EMouseEvent event, CPoint& point, int flags);
	bool HitTestRect(HitContext& hc);
	bool HitHelperTest(HitContext& hc);
	void Serialize(CObjectArchive& ar);

	void GetBoundBox(AABB& box);
	void GetLocalBounds(AABB& box);
	void OnContextMenu(CPopupMenuItem* menu);
	//////////////////////////////////////////////////////////////////////////

	void SetFOV(const float fov);
	void SetNearZ(const float nearZ);
	void SetAmplitudeAMult(const float amplitudeAMult);
	void SetAmplitudeBMult(const float amplitudeBMult);
	void SetFrequencyAMult(const float frequencyAMult);
	void SetFrequencyBMult(const float frequencyBMult);

	//! Get Camera Field Of View angle.
	const float GetFOV() const             { return mv_fov; }
	const float GetNearZ() const           { return mv_nearZ; }
	const float GetFarZ() const            { return mv_farZ; }
	const Vec3& GetAmplitudeA() const      { return mv_amplitudeA; }
	const float GetAmplitudeAMult() const  { return mv_amplitudeAMult; }
	const Vec3& GetFrequencyA() const      { return mv_frequencyA; }
	const float GetFrequencyAMult() const  { return mv_frequencyAMult; }
	const float GetNoiseAAmpMult() const   { return mv_noiseAAmpMult; }
	const float GetNoiseAFreqMult() const  { return mv_noiseAFreqMult; }
	const float GetTimeOffsetA() const     { return mv_timeOffsetA; }
	const Vec3& GetAmplitudeB() const      { return mv_amplitudeB; }
	const float GetAmplitudeBMult() const  { return mv_amplitudeBMult; }
	const Vec3& GetFrequencyB() const      { return mv_frequencyB; }
	const float GetFrequencyBMult() const  { return mv_frequencyBMult; }
	const float GetNoiseBAmpMult() const   { return mv_noiseBAmpMult; }
	const float GetNoiseBFreqMult() const  { return mv_noiseBFreqMult; }
	const float GetTimeOffsetB() const     { return mv_timeOffsetB; }
	const uint  GetCameraShakeSeed() const { return mv_cameraShakeSeed; }
	const bool  GetIsOmniCamera() const    { return mv_omniCamera; }

	void RegisterCameraListener(ICameraObjectListener* pListener);
	void UnregisterCameraListener(ICameraObjectListener* pListener);

private:
	//! Dtor must be protected.
	CCameraObject();

	// overrided from IAnimNodeCallback
	//void OnNodeAnimated( IAnimNode *pNode );

	virtual void DrawHighlight(DisplayContext& dc) {};

	// return world position for the entity targeted by look at.
	Vec3 GetLookAtEntityPos() const;

	void OnFovChange(IVariable* var);
	void OnNearZChange(IVariable* var);
	void OnFarZChange(IVariable* var);

	void OnOmniCameraChange(IVariable *var);

	void UpdateCameraEntity();

	void OnShakeAmpAChange(IVariable* var);
	void OnShakeAmpBChange(IVariable* var);
	void OnShakeFreqAChange(IVariable* var);
	void OnShakeFreqBChange(IVariable* var);
	void OnShakeMultChange(IVariable* var);
	void OnShakeNoiseChange(IVariable* var);
	void OnShakeWorkingChange(IVariable* var);
	void OnCameraShakeSeedChange(IVariable* var);

	void OnMenuSetAsViewCamera();

	// Arguments
	//   fAspectRatio - e.g. 4.0/3.0
	void GetConePoints(Vec3 q[4], float dist, const float fAspectRatio);
	void DrawCone(DisplayContext& dc, float dist, float fScale = 1);
	void CreateTarget();
	//////////////////////////////////////////////////////////////////////////
	//! Field of view.
	CVariable<float> mv_fov;

	CVariable<float> mv_nearZ;
	CVariable<float> mv_farZ;

	//////////////////////////////////////////////////////////////////////////
	//! Camera shake.
	CVariableArray   mv_shakeParamsArray;
	CVariable<Vec3>  mv_amplitudeA;
	CVariable<float> mv_amplitudeAMult;
	CVariable<Vec3>  mv_frequencyA;
	CVariable<float> mv_frequencyAMult;
	CVariable<float> mv_noiseAAmpMult;
	CVariable<float> mv_noiseAFreqMult;
	CVariable<float> mv_timeOffsetA;
	CVariable<Vec3>  mv_amplitudeB;
	CVariable<float> mv_amplitudeBMult;
	CVariable<Vec3>  mv_frequencyB;
	CVariable<float> mv_frequencyBMult;
	CVariable<float> mv_noiseBAmpMult;
	CVariable<float> mv_noiseBFreqMult;
	CVariable<float> mv_timeOffsetB;
	CVariable<int>   mv_cameraShakeSeed;
	CVariable<bool> mv_omniCamera;

	//////////////////////////////////////////////////////////////////////////
	// Mouse callback.
	int m_creationStep;

	CListenerSet<ICameraObjectListener*> m_listeners;
};

/*!
 * Class Description of CameraObject.
 */
class CCameraObjectClassDesc : public CObjectClassDesc
{
public:
	ObjectType     GetObjectType()     { return OBJTYPE_ENTITY; };
	const char*    ClassName()         { return "Camera"; };
	const char*    Category()          { return "Misc"; };
	CRuntimeClass* GetRuntimeClass()   { return RUNTIME_CLASS(CCameraObject); };
};

/*!
 *	CCameraObjectTarget is a target object for Camera.
 *
 */
class SANDBOX_API CCameraObjectTarget : public CEntityObject
{
public:
	DECLARE_DYNCREATE(CCameraObjectTarget)

	//////////////////////////////////////////////////////////////////////////
	// Ovverides from CBaseObject.
	//////////////////////////////////////////////////////////////////////////
	bool    Init(CBaseObject* prev, const string& file);
	void    InitVariables();
	string GetTypeDescription() const { return GetTypeName(); };
	void    Display(CObjectRenderHelper& objRenderHelper) override;
	bool    HitTest(HitContext& hc);
	void    GetBoundBox(AABB& box);
	bool    IsScalable() const override { return false; }
	bool    IsRotatable() const override { return false; }
	void    Serialize(CObjectArchive& ar);
	//////////////////////////////////////////////////////////////////////////

protected:
	//! Dtor must be protected.
	CCameraObjectTarget();

	virtual void DrawHighlight(DisplayContext& dc) {};
};

/*!
 * Class Description of CameraObjectTarget.
 */
class CCameraObjectTargetClassDesc : public CObjectClassDesc
{
public:
	ObjectType     GetObjectType()     { return OBJTYPE_ENTITY; };
	const char*    ClassName()         { return "CameraTarget"; };
	const char*    Category()          { return ""; };
	CRuntimeClass* GetRuntimeClass()   { return RUNTIME_CLASS(CCameraObjectTarget); };
};

#endif // __cameraobject_h__

