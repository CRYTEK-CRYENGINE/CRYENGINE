// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#define ANIMATION_30Hz (30.0f) //> Animation framerate is currently hard-coded to 30fps.
#define TICKS_CONVERT  (160)   //> A helper macro for converting 3ds Max 'ticks' (1/4800[s]) present in certain legacy assets to the hard-coded 30fps framerate (160 * 1/4800s == 1/30[s]).

enum EJointState
{
	eJS_Position    = (1 << 0),
	eJS_Orientation = (1 << 1),
	eJS_Scale       = (1 << 2),
};

typedef uint8 JointState;

//////////////////////////////////////////////////////////////////////////////////////////
// interface IController
// Describes the position and orientation of an object, changing in time.
// Responsible for loading itself from a binary file, calculations
//////////////////////////////////////////////////////////////////////////////////////////
class IController : public _i_reference_target_t
{

public:
	// each controller has an ID, by which it is identifiable
	uint32 m_nControllerId;

	uint32 GetID() const { return m_nControllerId; }

	// returns the orientation,position and scaling of the controller at the given time
	virtual JointState GetOPS(f32 key, Quat& quat, Vec3& pos, Diag33& scale) const = 0;
	// returns the orientation and position of the controller at the given time
	virtual JointState GetOP(f32 key, Quat& quat, Vec3& pos) const = 0;

	// returns the orientation of the controller at the given time
	virtual JointState GetO(f32 key, Quat& quat) const = 0;
	// returns position of the controller at the given time
	virtual JointState GetP(f32 key, Vec3& pos) const = 0;
	// returns scale of the controller at the given time
	virtual JointState GetS(f32 key, Diag33& scl) const = 0;

	virtual int32      GetO_numKey() const { return -1; } //only implemented for the PQ controller
	virtual int32      GetP_numKey() const { return -1; } //only implemented for the PQ controller

	virtual size_t     GetRotationKeysNum() const = 0;
	virtual size_t     GetPositionKeysNum() const = 0;
	virtual size_t     GetScaleKeysNum() const = 0;

	virtual size_t     SizeOfController() const = 0;
	virtual size_t     ApproximateSizeOfThis() const = 0;
};

TYPEDEF_AUTOPTR(IController);

// adjusts the rotation of these PQs: if necessary, flips them or one of them (effectively NOT changing the whole rotation,but
// changing the rotation angle to Pi-X and flipping the rotation axis simultaneously)
// this is needed for blending between animations represented by quaternions rotated by ~PI in quaternion space
// (and thus ~2*PI in real space)
extern void AdjustLogRotations(Vec3& vRotLog1, Vec3& vRotLog2);

//////////////////////////////////////////////////////////////////////////
// This class is a non-trivial predicate used for sorting an
// ARRAY OF smart POINTERS  to IController's. That's why I need a separate
// predicate class for that. Also, to avoid multiplying predicate classes,
// there are a couple of functions that are also used to find a IController
// in a sorted by ID array of IController* pointers, passing only ID to the
// lower_bound function instead of creating and passing a dummy IController*
class AnimCtrlSortPred
{
public:
	bool operator()(const IController_AutoPtr& a, const IController_AutoPtr& b) { assert(a != (IController*)NULL && b != (IController*)NULL); return a->GetID() < b->GetID(); }
};

class AnimCtrlSortPredInt
{
public:
	bool operator()(const IController* a, uint32 nID) { assert(a != (IController*)NULL); return a->GetID() < nID; }
	bool operator()(uint32 nID, const IController* b) { assert(b != (IController*)NULL); return nID < b->GetID(); }
};
