// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __IntersectionTestQueue_h__
#define __IntersectionTestQueue_h__

#pragma once

#include <type_traits>
#include <CryCore/StlUtils.h>
#include "DeferredActionQueue.h"

struct IntersectionTestResult
{
	operator bool() const
	{
		return distance > 0.0f;
	}

	Vec3  point;
	Vec3  normal;

	float distance;
	int   partId;
	int   idxMat;
};

struct IntersectionTestRequest
{
	enum
	{
		MaxSkipListCount = 64,
	};

	enum Priority
	{
		LowPriority = 0,
		MediumPriority,
		HighPriority,
		HighestPriority,
	};

	IntersectionTestRequest()
		: skipListCount(0)
	{
	}

	IntersectionTestRequest(int _primitiveType, const primitives::primitive& _primitive, const Vec3& _sweepDir, int _objTypes,
	                        int _flagsAll = 0, int _flagsAny = geom_colltype0 | geom_colltype_player, IPhysicalEntity** _skipList = 0,
	                        uint8 _skipListCount = 0)
		: primitiveType(_primitiveType)
		, sweepDir(_sweepDir)
		, objTypes(_objTypes)
		, flagsAll(_flagsAll)
		, flagsAny(_flagsAny)
		, skipListCount(_skipListCount)
	{
		assert(skipListCount <= MaxSkipListCount);
		skipListCount = std::min<uint32>(skipListCount, MaxSkipListCount);

		uint32 k = 0;
		for (uint32 i = 0; i < skipListCount; ++i)
		{
			assert(_skipList[i]);
			if (_skipList[i])
				skipList[k++] = _skipList[i];
		}

		skipListCount = k;

		switch (primitiveType)
		{
		case primitives::box::type:
			new(&primitiveBuf) primitives::box((primitives::box&)_primitive);
			break;
		case primitives::cylinder::type:
			new(&primitiveBuf) primitives::cylinder((primitives::cylinder&)_primitive);
			break;
		case primitives::capsule::type:
			new(&primitiveBuf) primitives::capsule((primitives::capsule&)_primitive);
			break;
		case primitives::sphere::type:
			new(&primitiveBuf) primitives::sphere((primitives::sphere&)_primitive);
			break;
		default:
			{
				assert(0);
			};
		}
	}

	~IntersectionTestRequest()
	{
		switch (primitiveType)
		{
		case primitives::box::type:
			(*(primitives::box*&)primitiveBuf).~box();
			break;
		case primitives::cylinder::type:
			(*(primitives::cylinder*)&primitiveBuf).~cylinder();
			break;
		case primitives::capsule::type:
			(*(primitives::capsule*)&primitiveBuf).~capsule();
			break;
		case primitives::sphere::type:
			(*(primitives::sphere*)&primitiveBuf).~sphere();
			break;
		default:
			{
				assert(0);
			}
			break;
		}
	}

	template<typename PrimitiveType>
	PrimitiveType& primitive()
	{
		return *(PrimitiveType*)&primitiveBuf;
	}

	template<typename PrimitiveType>
	const PrimitiveType& primitive() const
	{
		return *(PrimitiveType*)&primitiveBuf;
	}

	int              primitiveType;

	Vec3             sweepDir;
	int              objTypes;
	int              flagsAll;
	int              flagsAny;
	IPhysicalEntity* skipList[MaxSkipListCount];
	uint8            skipListCount;

private:
	enum
	{
		_MaxCapsuleSphereSize              = stl::static_max<sizeof(primitives::capsule), sizeof(primitives::sphere)>::value,
		_MaxCapsuleSphereCylinderSize      = stl::static_max<sizeof(primitives::cylinder), _MaxCapsuleSphereSize>::value,
		MaxPrimitiveSize                   = stl::static_max<sizeof(primitives::box), _MaxCapsuleSphereCylinderSize>::value,

		_MaxCapsuleSphereAlignment         = stl::static_max<alignof(primitives::capsule), alignof(primitives::sphere)>::value,
		_MaxCapsuleSphereCylinderAlignment = stl::static_max<alignof(primitives::cylinder), _MaxCapsuleSphereAlignment>::value,
		MaxPrimitiveAlignment              = stl::static_max<alignof(primitives::box), _MaxCapsuleSphereCylinderAlignment>::value,
	};

	typename std::aligned_storage<MaxPrimitiveSize, MaxPrimitiveAlignment>::type primitiveBuf;
};

template<int IntersectionTesterID>
struct DefaultIntersectionTester
{
protected:
	typedef DefaultIntersectionTester<IntersectionTesterID> Type;
	typedef Functor2<uint32, const IntersectionTestResult&> Callback;

	DefaultIntersectionTester()
		: callback(0)
	{
	}

	~DefaultIntersectionTester()
	{
	}

	ILINE void Acquire(IntersectionTestRequest& request)
	{
		for (size_t i = 0; i < request.skipListCount; ++i)
			request.skipList[i]->AddRef();
	}

	ILINE void Release(IntersectionTestRequest& request)
	{
		for (size_t i = 0; i < request.skipListCount; ++i)
			request.skipList[i]->Release();
	}

	IPhysicalWorld::SPWIParams GetPWIParams(const IntersectionTestRequest& request)
	{
		IPhysicalWorld::SPWIParams params;

		params.itype = request.primitiveType;
		params.pprim = &request.primitive<primitives::primitive>();
		params.sweepDir = request.sweepDir;
		params.entTypes = request.objTypes;
		params.geomFlagsAll = request.flagsAll;
		params.geomFlagsAny = request.flagsAny;
		params.pSkipEnts = request.skipListCount ? const_cast<IPhysicalEntity**>(&request.skipList[0]) : 0;
		params.nSkipEnts = request.skipListCount;

		return params;
	}

	inline const IntersectionTestResult& Cast(const IntersectionTestRequest& request)
	{
		IPhysicalWorld::SPWIParams params = GetPWIParams(request);

		geom_contact* contact = 0;
		params.ppcontact = &contact;

		m_resultBuf.distance = gEnv->pPhysicalWorld->PrimitiveWorldIntersection(params);

		assert(contact);
		PREFAST_ASSUME(contact);
		m_resultBuf.point = contact->pt;
		m_resultBuf.normal = contact->n;
		m_resultBuf.partId = contact->iPrim[1];
		m_resultBuf.idxMat = contact->id[1];

		return m_resultBuf;
	}

	inline void Queue(uint32 testID, const IntersectionTestRequest& request)
	{
		IPhysicalWorld::SPWIParams params = GetPWIParams(request);
		params.entTypes |= rwi_queue;
		params.pForeignData = this;
		params.iForeignData = testID;
		params.OnEvent = OnPWIResult;

		gEnv->pPhysicalWorld->PrimitiveWorldIntersection(params);
	}

	inline void SetCallback(const Callback& _callback)
	{
		callback = _callback;
	}

	static int OnPWIResult(const EventPhysPWIResult* result)
	{
		Type* _this = static_cast<Type*>(result->pForeignData);
		int testID = result->iForeignData;

		_this->m_resultBuf.point = result->pt;
		_this->m_resultBuf.normal = result->n;
		_this->m_resultBuf.partId = result->partId;
		_this->m_resultBuf.idxMat = result->idxMat;
		_this->m_resultBuf.distance = result->dist;

		_this->callback(testID, _this->m_resultBuf);

		return 1;
	}

private:
	Callback               callback;
	IntersectionTestResult m_resultBuf;
};

typedef uint32 QueuedIntersectionID;

template<int IntersectionTesterID>
class IntersectionTestQueue :
	public DeferredActionQueue<DefaultIntersectionTester<IntersectionTesterID>,
	                           IntersectionTestRequest, IntersectionTestResult>
{
public:
	typedef DeferredActionQueue<DefaultIntersectionTester<IntersectionTesterID>,
	                            IntersectionTestRequest, IntersectionTestResult> BaseType;
};

#endif
