// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
//
//	Crytek Character Animation source code
//	
//	History:
//	Created by Sergiy Migdalskiy
//	
//  Notes:
//    IController interface declaration
//    See the IController comment for more info
//
/////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _CRYTEK_CONTROLLER_HEADER_
#define _CRYTEK_CONTROLLER_HEADER_


#include "QuatQuantization.h"

#define STATUS_O   0x1 
#define STATUS_P   0x2
#define STATUS_S   0x4
#define STATUS_PAD 0x8

#define TICKS_PER_SECOND 30                         //the maximum possible frequency is 4800Hz
#define SECONDS_PER_TICK (1.0f / TICKS_PER_SECOND)
#define TICKS_PER_FRAME 1                           //TODO: change this comment to something more readable: if we have 30 keyframes per second, then one tick is 1
#define TICKS_CONVERT 160

struct Status4
{
	union 
	{
		uint32 ops;
		struct 
		{
			uint8 o;
			uint8 p;
			uint8 s;
			uint8 pad;
		};
	};
	ILINE Status4()
	{
	}
	ILINE Status4(uint8 _x, uint8 _y, uint8 _z, uint8 _w=1)
	{
		o = _x;
		p = _y;
		s = _z;
		pad = _w;
	}
};

struct CInfo
{
	uint32 numKeys;
	Quat   quat;
	Vec3   pos;
	int    stime;
	int    etime;
	int    realkeys;

	CInfo()
		: numKeys(-1)
		, stime(-1)
		, etime(-1)
		, realkeys(-1)
	{
	}
}; 


struct GlobalAnimationHeaderCAF;

//////////////////////////////////////////////////////////////////////////////////////////
// interface IController  
// Describes the position and orientation of an object, changing in time.
// Responsible for loading itself from a binary file, calculations
//////////////////////////////////////////////////////////////////////////////////////////
class CController;

class IController: public _reference_target_t
{
public:
	virtual const CController* GetCController() const
	{
		return 0;
	}

	// each controller has an ID, by which it is identifiable
	virtual uint32 GetID() const = 0;

	// returns the orientation of the controller at the given time
	virtual Status4 GetOPS(f32 t, Quat& quat, Vec3& pos, Diag33& scale) = 0;
	// returns the orientation and position of the controller at the given time
	virtual Status4 GetOP(f32 t, Quat& quat, Vec3& pos) = 0;
	// returns the orientation of the controller at the given time
	virtual uint32 GetO(f32 t, Quat& quat) = 0;
	// returns position of the controller at the given time
	virtual uint32 GetP(f32 t, Vec3& pos) = 0;
	// returns scale of the controller at the given time
	virtual uint32 GetS(f32 t, Diag33& pos) = 0;

	virtual int32 GetO_numKey() = 0;
	virtual int32 GetP_numKey() = 0;
	virtual int32 GetS_numKey() = 0;

	virtual CInfo GetControllerInfo() const = 0;
	virtual size_t SizeOfThis() const = 0;

	IController()
	{
	}

	virtual ~IController() 
	{
	}
};

TYPEDEF_AUTOPTR(IController);


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
	bool operator() (const IController_AutoPtr& a, const IController_AutoPtr& b) const
	{
		assert (a!=(IController*)NULL && b != (IController*)NULL);
		return a->GetID() < b->GetID();
	}
	bool operator() (const IController_AutoPtr& a, uint32 nID) const
	{
		assert (a != (IController*)NULL);
		return a->GetID() < nID;
	}
};

class AnimCtrlSortPredInt
{
public:
	bool operator() (const IController_AutoPtr& a, uint32 nID) const
	{
		assert (a != (IController*)NULL);
		return a->GetID() < nID;
	}
};

typedef std::vector<IController_AutoPtr> TControllersVector;

#endif