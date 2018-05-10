// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   VTOLVehicleManager.h
//  Version:     v1.00
//  Created:     02/09/2011
//  Compilers:   Visual Studio.NET
//  Description: Turns an entity with an Area proxy into a perimeter ordered list of waypoints
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#ifndef __WAYPOINTPATH_H
#define __WAYPOINTPATH_H

#define MAX_PATH_NODES 63

struct IEntityClass;

class CWaypointPath
{
public:
	
	typedef int8 TNodeId;

	enum E_NodeDataType
	{
		ENDT_None = 0,
		ENDT_Speed,
		ENDT_Wait
	};

private:

	struct SNode
	{
		SNode(Vec3& position, float distAlongPath)
		{
			pos = position;
			distanceAlongPath = distAlongPath;
		}

		Vec3 pos;
		float distanceAlongPath;
	};

	struct SNodeData
	{
		SNodeData() : type(ENDT_None), fValue(0.f) {}

		SNodeData(E_NodeDataType dataType, float dataValue)
		{
			type = dataType;
			fValue = dataValue;
		}

		E_NodeDataType type;
		float fValue;
	};

	typedef CryFixedArray<SNode, MAX_PATH_NODES> NodeArray;
	typedef std::map<TNodeId, SNodeData> NodeDataMap;

	NodeArray m_Nodes;
	NodeDataMap m_nodeDataMap; //Maps node index to speed value
	TNodeId m_MaxNodeIndex;

	void InitNode(IEntity* pNodeEntity);

public:

	CWaypointPath();
	~CWaypointPath();

	bool CreatePath(IEntity* pPathEntity);

	Matrix34 GetMatrixAtNode(TNodeId currentNode, bool loop) const;

	//Returns false if there is no next node
	bool GetNextNodePos(TNodeId currentNode, Vec3& nodePos, bool loop) const;

	//Returns false if the interpolated pos is the end of the spline
	bool GetNextNodePosAfterDistance(TNodeId currentNode, const Vec3& currentPos, float lookAheadDistance, Vec3& interpolatedPos, TNodeId& newNode, bool loop) const;

	//Returns false if the interpolated pos is the end of the spline
	bool GetPosAfterDistance(TNodeId currentNode, const Vec3& currentPos, float lookAheadDistance, Vec3& interpolatedPos, TNodeId& interpolatedNode, TNodeId& newNode, float& newPathLoc, bool loop) const;

	bool HasDataAtNode(TNodeId node, E_NodeDataType& dataType, float& outSpeed) const;

	float GetDistance( float from, float to, bool looping ) const;

	TNodeId GetNearest( const Vec3& pos ) const;

	void GetPathLoc( float pathLoc, QuatT& location, TNodeId& node ) const;


#ifndef _RELEASE
	void DebugDraw(bool renderLooped) const;
#endif

};

#endif //__WAYPOINTPATH_H
