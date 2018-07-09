// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __HUDUTILS_H__
#define __HUDUTILS_H__

#include "IViewSystem.h"
#include "Actor.h"

struct IHUDAsset;
struct IFlashVariableObject;

#define HUDUTILS_MAX_CENTER_SORT_HELPERS 1024

namespace CHUDUtils
{
	const char * LocalizeString( const char *text, const char *arg1 = 0, const char *arg2 = 0, const char *arg3 = 0, const char *arg4 = 0);
	void LocalizeString( string &out, const char *text, const char *arg1, const char *arg2, const char *arg3, const char *arg4 );
	void LocalizeStringn( char* dest, size_t bufferSizeInBytes, const char *text, const char *arg1 = 0, const char *arg2 = 0, const char *arg3 = 0, const char *arg4 = 0 );

	const char* LocalizeNumber(const int number);
	void LocalizeNumber(string &out, const int number);
	void LocalizeNumbern(const char* dest, size_t bufferSizeInBytes, const int number);

	const char* LocalizeNumber(const float number, int decimals);
	void LocalizeNumber(string &out, const float number, int decimals);
	void LocalizeNumbern(const char* dest, size_t bufferSizeInBytes, const float number, int decimals);

	void ConvertSecondsToTimerString( const int s, string* in_out_string, const bool stripZeroElements=false, bool keepMinutes=false, const char* const hex_colour=NULL );

	IFlashPlayer* GetFlashPlayerFromMaterial( IMaterial* pMaterial, bool bGetTemporary = false );
	IFlashPlayer* GetFlashPlayerFromCgfMaterial( IEntity* pCgfEntity, bool bGetTemporary = false/*, const char* materialName*/ );
	IFlashPlayer* GetFlashPlayerFromMaterialIncludingSubMaterials( IMaterial* pMaterial, bool bGetTemporary = false );

	static bool ClampToScreen(Vec3& inoutPos, const bool isInFront)
	{
		if(!inoutPos.IsValid())
		{
			return false;
		}

		Vec3 center(50.0f,50.0f, 0.0f);
		Vec3 dir = center - inoutPos;

		if(!isInFront)
		{
			dir.NormalizeSafe();
			dir *= -72.0f;
		}

		Vec3 dirModify(dir);
		dirModify = dirModify.abs();

		float modifier = 0.0f;
		if(dirModify.x > dirModify.y)
		{
			modifier = (50.0f * __fres(dirModify.x));
		}
		else if(dirModify.y > 0.0f)
		{
			modifier = (50.0f * __fres(dirModify.y));
		}

		modifier = min(modifier, 1.0f);
		dir *= modifier;

		inoutPos = center - dir;
		return (modifier<1.0f);
	}


	static bool ClampToCircle(Vec2& inoutPos, const Vec2 radiusDim, const bool isInFront)
	{
		Vec2 dir = inoutPos;

		if(!isInFront)
		{
			dir.NormalizeSafe();
			dir *= -radiusDim.GetLength();
		}
		
		float rot = atan2_tpl(dir.x, dir.y);

		const Vec2 circle(radiusDim.x * sinf(rot), radiusDim.y * cosf(rot));

		bool ret = false;

		if(dir.GetLength() > circle.GetLength())
		{
			dir.x = circle.x;
			dir.y = circle.y;
			ret = true;
		}

		const Vec3 prevPos(inoutPos);

		inoutPos = dir;
		return ret;
	}


	static const Vec3& GetClientPos()
	{
		static Vec3 dummyVal(0,0,0);

		IView *pView = gEnv->pGameFramework->GetIViewSystem()->GetActiveView();
		if(!pView)
			return dummyVal;

		return pView->GetCurrentParams()->position;
	}



	static Vec3 GetClientDir()
	{
		IView *pView = gEnv->pGameFramework->GetIViewSystem()->GetActiveView();
		if(!pView)
			return FORWARD_DIRECTION;

		return pView->GetCurrentParams()->rotation.GetColumn1();
	}



	static void ClampToFront(Vec3& inOutWorldPos, const Vec3& viewTargetWorldPos)
	{
		const Line line(inOutWorldPos, viewTargetWorldPos);
		const Plane plane = Plane::CreatePlane(GetClientDir(), GetClientPos());
		Vec3 intersect(0.0f, 0.0f, 0.0f);
		Intersect::Line_Plane(line, plane, intersect, false);

		inOutWorldPos = (intersect - viewTargetWorldPos) * 0.8f + viewTargetWorldPos;
	}



	static bool IsInFront(const Vec3& worldPos)
	{
		Vec3 dir = GetClientDir().GetNormalized();
		Vec3 toTarget = (worldPos - GetClientPos()).GetNormalized();

		return (dir.Dot(toTarget) > 0.0f);
	}



	static float GetSizeFromSquaredDistance(const Vec3& worldPos, const float minDist, const float maxDist, const float bigSize, const float smallSize)
	{
		const float maxDistSq = maxDist*maxDist;
		const float minDistSq = minDist*minDist;

		const float distRange = maxDistSq - minDistSq;
		const float sizeRange = bigSize - smallSize;

		const float distSq = clamp_tpl(GetClientPos().GetSquaredDistance(worldPos), minDistSq, maxDistSq);

		//This math tries to normalize the squared distance values, which tend to scale too extreme with high values
		//It is used to be able to use the squared distance instead of the distance.
		//If this is too complicated or the result not satisfying or proves not worth the optimization,
		//use GetSizeFromDistance() instead
		const float distRatio = 1.0f + (distSq - minDistSq) * __fres(distRange);
		const float invertedDistRatio = (__fres(distRatio) - 0.5f) * 2.0f;
		return (smallSize + (invertedDistRatio * sizeRange));
	}



	static float GetSizeFromDistance(const Vec3& worldPos, const float minDist, const float maxDist, const float bigSize, const float smallSize)
	{
		const float distRange = maxDist - minDist;
		const float sizeRange = bigSize - smallSize;

		const float dist = clamp_tpl(GetClientPos().GetDistance(worldPos), minDist, maxDist);

		const float distRatio = (dist - minDist) * __fres(distRange);
		return (bigSize - (distRatio * sizeRange));
	}

	static bool IsLineIntersectsFromOutside(const Vec2 &faceLineA, const Vec2 &faceLineB, const Vec2 &pos, const Vec2 &center, Vec2 &intersectPoint)
	{
		Vec2 line = faceLineB-faceLineA; //k
		Vec2 lineNormal = Vec2(-line.y, line.x);
		Vec2 faceLine = faceLineA-pos;

		bool intersects = false;

		float dot = lineNormal.Dot(faceLine);
		if(dot > 0.0f) //it's possibly crossing, try to compute the intersection
		{
			Vec2 crossingLine = (pos-center); //t

			if(line.x == 0.0f)
			{
				float i2 = crossingLine.y / crossingLine.x;
				float r2 = center.y - ((center.x * crossingLine.y) / crossingLine.x); //p
				intersectPoint.x = faceLineA.x;
				intersectPoint.y = i2 * intersectPoint.x + r2;
			}
			else if(crossingLine.x == 0.0f)
			{
				float i1 = line.y / line.x;
				float r1 = faceLineA.y - ((faceLineA.x * line.y) / line.x); //b
				intersectPoint.x = center.x;
				intersectPoint.y = i1 * intersectPoint.x + r1;
			}
			else
			{
				float i1 = line.y / line.x;   //gradients m
				float i2 = crossingLine.y / crossingLine.x; //n
				float r1 = faceLineA.y - ((faceLineA.x * line.y) / line.x); //b
				float r2 = center.y - ((center.x * crossingLine.y) / crossingLine.x); //p
				intersectPoint.x = (r1-r2) / (i2-i1);
				intersectPoint.y = i1 * intersectPoint.x + r1;
			}

			float onLine = -1.0f;
			if(line.y != 0.0f)
				onLine = (intersectPoint.y - faceLineA.y) / line.y;
			else if(line.x != 0.0f)
				onLine = (intersectPoint.x - faceLineA.x) / line.x;

			if(onLine > 0.0f && onLine < 1.0f)
				intersects = true;
		}
		return intersects;
	}

	enum EFriendState
	{
		eFS_Unknown = 0,
		eFS_Friendly = 1,
		eFS_Enemy = 2,
		eFS_LocalPlayer = 3,
		eFS_Squaddie = 4,
		eFS_Server = 5,
		eFS_Max = 6		// Keep last
	};

	const char* GetFriendlyStateColour( EFriendState friendState );
	const EFriendState GetFriendlyState( const EntityId entityId, CActor* pLocalActor );
	ILINE const EFriendState GetFriendlyState( const EntityId entityId )
	{
		CActor* pLocalActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetClientActor());
		return GetFriendlyState( entityId, pLocalActor );
	}

	const float GetIconDepth(const float distance);

	const ColorF& GetHUDColor();

	int GetBetweenRoundsTimer(int previousTimer);

	bool IsSensorPackageActive();

	struct SCenterSortPoint
	{
		SCenterSortPoint(Vec2 screenPos, void* pData = NULL)
			: m_screenPos(screenPos)
			, m_pData(pData)
		{}
		Vec2	m_screenPos;
		void* m_pData;
	};

	typedef CryFixedArray<SCenterSortPoint, HUDUTILS_MAX_CENTER_SORT_HELPERS> TCenterSortArray;

	const size_t GetNumSubtitleCharacters();
	const char* GetSubtitleCharacter(const size_t index);
	const char* GetSubtitleColor(const size_t index);

	TCenterSortArray& GetCenterSortHelper();
	void* GetNearestToCenter();
	void* GetNearestToCenter(const float maxValidDistance);
	void* GetNearestTo(const Vec2& center, const float maxValidDistance);
	void* GetNearestTo(const TCenterSortArray& array, const Vec2& center, const float maxValidDistance);

	// Converts the silhouette parameters and activity state based on r,g,b,a values and enabled boolean respectively
	uint32 ConverToSilhouetteParamValue(ColorF color, bool bEnable = true);
	uint32 ConverToSilhouetteParamValue(float r, float g, float b, float a, bool bEnable = true);
};


#endif // __HUDDEFINES_H__
