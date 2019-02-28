// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/****************************************************************************************
----------------------------------------------------------------------------------------
History:
- Created: Stan Fichele

****************************************************************************************/

#ifndef __GAME_VEHICLE_UTILS__H__
#define __GAME_VEHICLE_UTILS__H__


// GCC requires a full decl of TSerialize, not just a fwd decl. for the CVehicleNetActionSync
#include <CryNetwork/ISerialize.h>


// Lerp an axis of the vector to a target (axis must be normalised)
ILINE void LerpAxisTimeIndependent(Vec3& x, const Vec3& axisNormalised, const float target, const float dt, const float strength)
{
	float currentAxisValue = axisNormalised.dot(x);
	x = x + axisNormalised * ((target - currentAxisValue) * approxOneExp(dt*strength));
}

// Damp an axis of the vector to zero, axis does not need to be normalised
ILINE void DampAxisTimeIndependentN(Vec3& x, const Vec3& axis, const float dt, const float strength)
{
	float sq = axis.GetLengthSquared();
	if (sq>0.0001f)
	{
		x = x - axis * ((axis.dot(x)/sq) * approxOneExp(dt*strength));
	}
}

ILINE void LinearChange(float& value, const float target, float amount)
{
	value += clamp_tpl(target - value, -amount, +amount);
}

// Approximately predict the new quaternion given a starting quaternion, q0, and an angular velocity, w.
ILINE void Predict(Quat& qNew, const Quat& q0, const Vec3 w, float dt)
{
	qNew.w = q0.w - (w*q0.v)*dt*0.5f;
	qNew.v = q0.v + ((w^q0.v)+w*qNew.w)*(dt*0.5f);
	qNew.Normalize();
}

// Calculate the updated (pos,q) of a rigidbody given a (vel,angVel) for a timestep, dt

ILINE void PredictPhysics(Vec3& pos, Quat& q, const Vec3& vel, const Vec3& angVel, float dt)
{
	pos += vel * dt;

	const float wlensq = angVel.GetLengthSquared();
	if (wlensq*sqr(dt) >= sqr(0.003f))
	{
		float wlen = sqrtf(wlensq);
		q = Quat::CreateRotationAA(wlen*dt,angVel/wlen)*q;
	}
	else if(wlensq > 0.0f)
	{
		q.w -= (angVel*q.v)*dt*0.5f;
		q.v += ((angVel^q.v)+angVel*q.w)*(dt*0.5f);
		q.Normalize();
	}	
}

/*
=====================================================================================================
	CVehicleNetActionSync
=====================================================================================================
 	USAGE:
		Call Read() at the start of the Update loop, and Write() at the end
		the Update loop. The helper class deals with read/write mode and new data.
		The serialisation class needs to typedef its parent object and its control aspect
*/

template <class ActionRep>
class CVehicleNetActionSync
{
public:
	typedef typename ActionRep::ParentObject ParentObject;
	static const NetworkAspectType CONTROLLED_ASPECT = ActionRep::CONTROLLED_ASPECT;

	CVehicleNetActionSync()
	{
		ChangeAuthority();
	}

	void ChangeAuthority()
	{
		m_mode = 0;
		m_readFlag = 0;
	}

	void Read(ParentObject* pParentObject)
	{
		if (iszero(m_mode-k_modeRecv)&m_readFlag)
			m_actionRep.Read(pParentObject);
		m_readFlag = 0;
	}

	bool Write(ParentObject* pParentObject)
	{
		if (m_mode==k_modeSend || m_mode==k_modeNone)
		{
			m_actionRep.Write(pParentObject);
			return true;
		}
		return false;
	}
	
	void Serialize( TSerialize ser, EEntityAspects aspects )
	{
		if ((aspects&CONTROLLED_ASPECT)==0)
			return;

		// Only change mode if its not already set
		bool isReading = ser.IsReading();
		if (m_mode==k_modeNone)
			m_mode = isReading ? k_modeRecv : k_modeSend;
		if (isReading)
			m_readFlag = 1;
		m_actionRep.Serialize(ser, aspects);
	}

public:
	enum {
		k_modeNone=0,
		k_modeRecv=1,
		k_modeSend=2,
	};
	ActionRep m_actionRep;
	uint16 m_mode;
	uint16 m_readFlag;
};


/*
=====================================================================================================
	CTimeSmoothedValue
=====================================================================================================
	DESC:
		A value that is smoothed over time
 	USAGE:
		Call Update() each frame with the new value to update the instant value and the filtered one
*/
class CTimeSmoothedValue
{
public:
	CTimeSmoothedValue()
	{
		Reset(0.f);
		m_invTimeConstant = 10.f;
	}
	void Reset(float newValue)
	{
		m_filteredValue = newValue;
		m_value = newValue;
	}
	void Update(float newValue, float dt)
	{
		m_filteredValue += (newValue - m_filteredValue) * approxOneExp(dt*m_invTimeConstant);
		m_value = newValue;
	}
public:
	float m_value;                // Instantly changing value
	float m_filteredValue;        // The time filtered value
	float m_invTimeConstant;      // Inverse exponential time constant for filtering. A small value < 10.0 will change very slowly. A large value >> 10.0 will change very quickly
};

/*
=====================================================================================================
	CTimeSmoothedBar
=====================================================================================================
	DESC:
		Draw a 2D bar. The smoothed value is drawn as a line
		Only supports vertical mode (at the moment)
*/

class CTimeSmoothedBar
{
public:
	CTimeSmoothedBar(float minValue, float maxValue)
	{
		m_minValue = minValue;
		m_maxValue = maxValue;
		m_colourBar.set(255,0,0,255);
		m_colourLine.set(255,255,0,255);
		m_colourBorder.set(255,255,255,255);
	}
	// Expects the the display mode to be 2D already
	void Draw(CTimeSmoothedValue& value, float x, float y, float width, float height)
	{
		if (m_maxValue <= m_minValue)
			return;

		// Draw the main bar
		const float valueFraction = clamp_tpl((value.m_value-m_minValue)/(m_maxValue-m_minValue), 0.f, 1.f);
		DrawQuad(x, y, width, height*valueFraction, m_colourBar);
		
		// Draw upper+lower markers
		const float lineWidth = height * 0.02f;
		const float halfLineWidth = lineWidth * 0.5f;
		y+=halfLineWidth;
		DrawQuad(x, y, width, lineWidth, m_colourBorder);
		DrawQuad(x, y-height, width, lineWidth, m_colourBorder);
		DrawQuad(x + 0.5f*width, y, lineWidth, height, m_colourBorder);
	
		// Draw the time-filtered line
		const float yValueFiltered = y - height * clamp_tpl((value.m_filteredValue - m_minValue)/(m_maxValue-m_minValue), 0.f, 1.f);
		DrawQuad(x, yValueFiltered, width, lineWidth, m_colourLine);
	}

protected:
	void DrawQuad(float x, float y, float w, float h, ColorB& col)
	{
		IRenderAuxGeom* pAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();
		Vec3 v0 (x,y,0.f);
		Vec3 v1 (x,y-h,0.f);
		Vec3 v2 (x+w,y,0.f);
		Vec3 v3 (x+w,y-h,0.f);
		pAuxGeom->DrawTriangle(v0, col, v2, col, v1, col );
		pAuxGeom->DrawTriangle(v2, col, v3, col, v1, col );
	}

public:
	float m_minValue;
	float m_maxValue;
	ColorB m_colourBorder;
	ColorB m_colourLine;
	ColorB m_colourBar;
};

#endif // __GAME_VEHICLE_UTILS__H__

