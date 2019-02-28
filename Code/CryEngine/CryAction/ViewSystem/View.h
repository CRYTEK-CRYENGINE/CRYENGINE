// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "IViewSystem.h"

class CGameObject;
struct IGameObjectView;

namespace Cry
{
namespace Audio
{
namespace DefaultComponents
{
class CListenerComponent;
} // namespace DefaultComponents
} // namespace Audio
} // namespace Cry

class CView final : public IView, public IEntityEventListener
{
public:

	explicit CView(ISystem* const pSystem);
	virtual ~CView() override;

	//shaking
	struct SShake
	{
		bool      updating;
		bool      flip;
		bool      doFlip;
		bool      groundOnly;
		bool      permanent;
		bool      interrupted; // when forcefully stopped
		bool      isSmooth;

		int const ID;

		float     nextShake;
		float     timeDone;
		float     sustainDuration;
		float     fadeInDuration;
		float     fadeOutDuration;

		float     frequency;
		float     ratio;

		float     randomness;

		Quat      startShake;
		Quat      startShakeSpeed;
		Vec3      startShakeVector;
		Vec3      startShakeVectorSpeed;

		Quat      goalShake;
		Quat      goalShakeSpeed;
		Vec3      goalShakeVector;
		Vec3      goalShakeVectorSpeed;

		Ang3      amount;
		Vec3      amountVector;

		Quat      shakeQuat;
		Vec3      shakeVector;

		explicit SShake(int const shakeID)
			: updating(false)
			, flip(false)
			, doFlip(false)
			, groundOnly(false)
			, permanent(false)
			, interrupted(false)
			, isSmooth(false)
			, ID(shakeID)
			, nextShake(0.0f)
			, timeDone(0.0f)
			, sustainDuration(0.0f)
			, fadeInDuration(0.0f)
			, fadeOutDuration(0.0f)
			, frequency(0.0f)
			, ratio(0.0f)
			, randomness(0.5f)
			, startShake(IDENTITY)
			, startShakeSpeed(IDENTITY)
			, startShakeVector(ZERO)
			, startShakeVectorSpeed(ZERO)
			, goalShake(IDENTITY)
			, goalShakeSpeed(IDENTITY)
			, goalShakeVector(ZERO)
			, goalShakeVectorSpeed(ZERO)
			, amount(ZERO)
			, amountVector(ZERO)
			, shakeQuat(IDENTITY)
			, shakeVector(ZERO)
		{}

		void GetMemoryUsage(ICrySizer* pSizer) const { /*nothing*/ }
	};

	// IView
	virtual void               Release() override;
	virtual void               Update(float frameTime, bool isActive) override;
	virtual void               ResetShaking() override;
	virtual void               ResetBlending() override { m_viewParams.ResetBlending(); }
	//FIXME: keep CGameObject *  or use IGameObject *?
	virtual void               LinkTo(IGameObject* follow) override;
	virtual void               LinkTo(IEntity* follow, IGameObjectView* callback) override;
	virtual EntityId           GetLinkedId() override                         { return m_linkedTo; };
	virtual void               SetCurrentParams(SViewParams& params) override { m_viewParams = params; };
	virtual const SViewParams* GetCurrentParams() override                    { return &m_viewParams; }
	virtual void               SetViewShake(Ang3 shakeAngle, Vec3 shakeShift, float duration, float frequency, float randomness, int shakeID, bool bFlipVec = true, bool bUpdateOnly = false, bool bGroundOnly = false) override;
	virtual void               SetViewShakeEx(const SShakeParams& params) override;
	virtual void               StopShake(int shakeID) override;
	virtual void               SetFrameAdditiveCameraAngles(const Ang3& addFrameAngles) override;
	virtual void               SetScale(const float scale) override;
	virtual void               SetZoomedScale(const float scale) override;
	virtual void               SetActive(bool const bActive) override;
	// ~IView

	// IEntityEventListener
	virtual void OnEntityEvent(IEntity* pEntity, const SEntityEvent& event) override;
	// ~IEntityEventListener

	virtual void ProcessShaking(float frameTime);
	virtual void ProcessShake(SShake* pShake, float frameTime);

	void         Serialize(TSerialize ser);
	void         PostSerialize();
	CCamera& GetCamera() { return m_camera; }
	void     UpdateAudioListener(Matrix34 const& worldTM);

	void     GetMemoryUsage(ICrySizer* s) const;

protected:

	CGameObject* GetLinkedGameObject();
	IEntity*     GetLinkedEntity();
	void         ProcessShakeNormal(SShake* pShake, float frameTime);
	void         ProcessShakeNormal_FinalDamping(SShake* pShake, float frameTime);
	void         ProcessShakeNormal_CalcRatio(SShake* pShake, float frameTime, float endSustain);
	void         ProcessShakeNormal_DoShaking(SShake* pShake, float frameTime);

	void         ProcessShakeSmooth(SShake* pShake, float frameTime);
	void         ProcessShakeSmooth_DoShaking(SShake* pShake, float frameTime);

	void         ApplyFrameAdditiveAngles(Quat& cameraOrientation);

	const float  GetScale();

private:

	void GetRandomQuat(Quat& quat, SShake* pShake);
	void GetRandomVector(Vec3& vec3, SShake* pShake);
	void CubeInterpolateQuat(float t, SShake* pShake);
	void CubeInterpolateVector(float t, SShake* pShake);
	void CreateAudioListener();

protected:

	EntityId            m_linkedTo;
	IGameObjectView*    m_linkedEntityCallback;

	SViewParams         m_viewParams;
	CCamera             m_camera;

	ISystem* const      m_pSystem;

	std::vector<SShake> m_shakes;

	Cry::Audio::DefaultComponents::CListenerComponent* m_pAudioListenerComponent;
	IEntity* m_pAudioListenerEntity;
	Ang3     m_frameAdditiveAngles; // Used mainly for cinematics, where the game can slightly override camera orientation

	float    m_scale;
	float    m_zoomedScale;
};
