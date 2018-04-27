// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "WaypointPath.h"

#include <CryGame/IGameVolumes.h>
#include "EntityUtility/EntityScriptCalls.h"
#include "GameXmlParamReader.h"

#ifndef _RELEASE
#include <CryRenderer/IRenderAuxGeom.h>
#include "GameCVars.h"
#endif

CWaypointPath::CWaypointPath()
{
	m_MaxNodeIndex = -1;
}

CWaypointPath::~CWaypointPath()
{
}

bool CWaypointPath::CreatePath(IEntity* pPathEntity)
{
	m_Nodes.clear();
	m_MaxNodeIndex = -1;

	IGameVolumes* pGameVolumes = gEnv->pGameFramework->GetIGameVolumesManager();
	if (pGameVolumes != NULL)
	{
		IGameVolumes::VolumeInfo volumeInfo;
		if(pGameVolumes->GetVolumeInfoForEntity(pPathEntity->GetId(), &volumeInfo))
		{
			const Matrix34& worldMat = pPathEntity->GetWorldTM();
			Vec3 entityPos = worldMat.GetColumn3();
			Quat entityRot = Quat(worldMat);

			int count = volumeInfo.verticesCount;
			if(count > MAX_PATH_NODES)
			{
				CRY_ASSERT_MESSAGE( false, string().Format("CWaypointPath::CreatePath - path %s has too many nodes - %i/%i. Truncating end nodes.", pPathEntity->GetName(), count, MAX_PATH_NODES) );
				count = MAX_PATH_NODES;
			}

			if(count)
			{
				float currentDistanceAlongPath = 0.f;

				for(int i = 0; i < count; ++i)
				{
					Vec3 nodeWorldPos = entityPos+(entityRot*volumeInfo.pVertices[i]);

					if(i > 0)
					{
						currentDistanceAlongPath += m_Nodes.back().pos.GetDistance(nodeWorldPos);
					}

					++m_MaxNodeIndex;
					m_Nodes.push_back( SNode(nodeWorldPos, currentDistanceAlongPath) );
				}

				//Read optional XML node information
				IScriptTable* pScriptTable = pPathEntity->GetScriptTable();
				const char* nodeDataFile = 0;
				if(EntityScripts::GetEntityProperty(pPathEntity, "fileNodeDataFile", nodeDataFile) && strcmpi("", nodeDataFile) )
				{
					XmlNodeRef rootNode = gEnv->pSystem->LoadXmlFromFile(nodeDataFile);
					if (rootNode)
					{
						CGameXmlParamReader nodeDataReader(rootNode);

						if(XmlNodeRef pathNode = nodeDataReader.FindFilteredChild("Path"))
						{
							int nodeCount = pathNode->getChildCount();
							int index = 0;
							float fValue = 0.f;
							for(int i = 0; i < nodeCount; ++i)
							{
								XmlNodeRef nodeNode = pathNode->getChild(i);
								if(nodeNode->getAttr("index", index))
								{
#ifndef _RELEASE
									if(index > m_MaxNodeIndex)
									{
										string str;
										CryLog("CWaypointPath::CreatePath - Adding node data to map but node %i does not exist. Remove from XML file %s.", index, nodeDataFile);
									}
#endif
									if(nodeNode->getAttr("speed", fValue))
									{
										m_nodeDataMap[index] = SNodeData(ENDT_Speed, fValue);
									}
									else if(nodeNode->getAttr("wait", fValue))
									{
										m_nodeDataMap[index] = SNodeData(ENDT_Wait, fValue);
									}
								}
							}
						}
					}
				}
				return true;
			}
		}
	}

	return false;
}

Matrix34 CWaypointPath::GetMatrixAtNode(TNodeId currentNode, bool loop) const
{
	if(currentNode >= 0 && currentNode <= m_MaxNodeIndex)
	{
		Vec3 atPosition = m_Nodes[currentNode].pos;

		Vec3 faceDirection;
		if(currentNode != m_MaxNodeIndex)
		{
			faceDirection = m_Nodes[currentNode+1].pos - atPosition;
		}
		else
		{
			if(loop)
			{
				faceDirection = m_Nodes[0].pos - atPosition;
			}
			else
			{
				faceDirection = atPosition - m_Nodes[currentNode-1].pos;
			}
		}

		faceDirection.Normalize();
		Quat rotation = Quat::CreateRotationVDir( faceDirection, 0.f );

		return Matrix34::Create(Vec3(1.f,1.f,1.f), rotation, atPosition);
	}

	CRY_ASSERT_MESSAGE(false, "CWaypointPath::GetMatrix - currentNode input is out of range");
	return Matrix34();
}

bool CWaypointPath::GetNextNodePos(TNodeId currentNode, Vec3& nodePos, bool loop) const
{
	if(currentNode != m_MaxNodeIndex)
	{
		nodePos = m_Nodes[currentNode+1].pos;

		return true;
	}

	if(loop)
	{
		nodePos = m_Nodes[0].pos;
		return true;
	}
	
	return false;
}

bool CWaypointPath::GetNextNodePosAfterDistance(TNodeId currentNode, const Vec3& currentPos, float lookAheadDistance, Vec3& interpolatedPos, TNodeId& newNode, bool loop) const
{
	if(currentNode != m_MaxNodeIndex)
	{
		float distanceToNextNode;
		Vec3 direction;
		Vec3 nextNodePos;
		Vec3 currentNodePos;
		if(currentNode >= 0)
		{
			currentNodePos = m_Nodes[currentNode].pos;
			nextNodePos = m_Nodes[currentNode+1].pos;
			distanceToNextNode = currentPos.GetSquaredDistance(nextNodePos);
		}
		//Haven't reached first node yet
		else
		{
			nextNodePos = m_Nodes[currentNode+1].pos;
			currentNodePos = currentPos;
			distanceToNextNode = currentPos.GetSquaredDistance(nextNodePos);
		}
		
		float lookAheadDistanceSquared = lookAheadDistance * lookAheadDistance;
		//If our look ahead distance does not cross one or more nodes
		if(lookAheadDistanceSquared <= distanceToNextNode)
		{
			newNode = currentNode;
			interpolatedPos = m_Nodes[currentNode+1].pos;
			return true;
		}
		else
		{
			//Find which two nodes the look ahead point will be between
			float currentLookAheadDistance = 0.f;
			TNodeId startNode = currentNode;
			TNodeId endNode = currentNode;
			float remainingLookAheadDistance = lookAheadDistance;

			//Take into account distance to first node
			if(currentNode < 0)
			{
				endNode = 0;
				remainingLookAheadDistance -= sqrt(distanceToNextNode);
			}

			while(endNode != m_MaxNodeIndex && remainingLookAheadDistance > 0.f)
			{
				++endNode;
				remainingLookAheadDistance -= m_Nodes[endNode].distanceAlongPath;
			}

			newNode = endNode;
			interpolatedPos = m_Nodes[endNode].pos;
			return true;		
		}
	}
	//Already at or moving towards the final node
	else
	{
		newNode = m_MaxNodeIndex;
		interpolatedPos = m_Nodes[m_MaxNodeIndex].pos;
	}

	if(loop)
	{
		newNode = -1;
		interpolatedPos = m_Nodes[0].pos;
		return true;
	}

	return false;
}

bool CWaypointPath::GetPosAfterDistance(TNodeId currentNode, const Vec3& currentPos, float lookAheadDistance, Vec3& interpolatedPos, TNodeId& interpolatedNode, TNodeId& newNode, float& newPathLoc, bool loop) const
{
	bool crossingStartEndNode = currentNode == m_MaxNodeIndex;
	if(!crossingStartEndNode || loop)
	{
		float distanceToNextNode;
		Vec3 nextNodePos;
		Vec3 currentNodePos;
		TNodeId nextNodeIndex =  !crossingStartEndNode ? currentNode+1 : 0;

		if(currentNode >= 0)
		{
			currentNodePos = m_Nodes[currentNode].pos;
			nextNodePos = m_Nodes[nextNodeIndex].pos;
			// Update the newPathLoc.
			{
				float basePathLoc = 0.f;
				const int totalNodes = m_MaxNodeIndex+1;

				float localPathLoc = 100.f;
				for(int i=0;i<3&&localPathLoc>=1.0f;i++)
				{
					const Vec3 AB(m_Nodes[(currentNode+i+1)%totalNodes].pos-m_Nodes[(currentNode+i)%totalNodes].pos);
					const Vec3 AP(currentPos-m_Nodes[(currentNode+i)%totalNodes].pos);
					basePathLoc = static_cast<float>(currentNode+i);
					const float ABAB = AB.dot(AB);
					localPathLoc = ABAB>0.0f ? AB.dot(AP)/ABAB : 0.f;
				}
				localPathLoc = clamp_tpl(localPathLoc,0.f,0.9999f);
				newPathLoc = clamp_tpl(basePathLoc+localPathLoc, 0.f, static_cast<float>(totalNodes));
			}
		}
		//Haven't reached first node yet
		else
		{
			nextNodePos = m_Nodes[nextNodeIndex].pos;
			currentNodePos = currentPos;
			newPathLoc = 0.f;
		}

		distanceToNextNode = currentPos.GetSquaredDistance(nextNodePos);

		//Check the current and next line segments to see which we are now closest to
		Vec3 currentNodeToNextNode = nextNodePos - currentNodePos;
		Vec3 currentNodePosToCurrentPos = currentPos - currentNodePos;
		float currentSegmentLength = currentNodeToNextNode.GetLength();
		float toSectionLength = currentNodePosToCurrentPos.GetLength();
		Vec3 currentSegment = currentNodeToNextNode / currentSegmentLength;
		Vec3 toSection = !currentNodePosToCurrentPos.IsZeroFast() ? currentNodePosToCurrentPos / toSectionLength : currentNodePosToCurrentPos;
		float fResult = min(1.f, (max(0.f, currentSegment.dot(toSection)) * toSectionLength / currentSegmentLength));
		Vec3 result = fResult * currentNodeToNextNode;
		Vec3 resultPos = currentNodePos + result;
		float distSquared = (resultPos - currentPos).GetLengthSquared();

		TNodeId nextSegmentEndNodeIndex = nextNodeIndex + 1;
		if(nextSegmentEndNodeIndex > m_MaxNodeIndex)
		{
			nextSegmentEndNodeIndex = 0;
		}

		Vec3 nextSegmentEndNodePos = m_Nodes[nextSegmentEndNodeIndex].pos;
		Vec3 nextNodePosToNextSegmentEndNodePos = nextSegmentEndNodePos - nextNodePos;
		Vec3 nextNodePosToCurrentPos = currentPos - nextNodePos;
		float nextSegmentLength = nextNodePosToNextSegmentEndNodePos.GetLength();
		float toNextSegmentLength = nextNodePosToCurrentPos.GetLength();
		Vec3 nextSegment = nextNodePosToNextSegmentEndNodePos / nextSegmentLength;
		Vec3 toNextSegment = !nextNodePosToCurrentPos.IsZeroFast() ? nextNodePosToCurrentPos / toNextSegmentLength : nextNodePosToCurrentPos;
		float fNextSegmentResult = min(1.f, (max(0.f, nextSegment.dot(toNextSegment)) * toNextSegmentLength / nextSegmentLength));
		Vec3 nextSegmentResult = fNextSegmentResult * nextNodePosToNextSegmentEndNodePos;
		Vec3 nextSegmentResultPos = nextNodePos + nextSegmentResult;
		float nextSegmentDistSquared = (nextSegmentResultPos - currentPos).GetLengthSquared();

		if(nextSegmentDistSquared <= distSquared)
		{
			if(++newNode > m_MaxNodeIndex)
			{
				newNode = 0;
			}
		}

		float lookAheadDistanceSquared = lookAheadDistance * lookAheadDistance;
		//If our look ahead distance does not cross one or more nodes
		if(lookAheadDistanceSquared <= distanceToNextNode)
		{
			Vec3 dirToNextNode = (m_Nodes[nextNodeIndex].pos-resultPos).GetNormalized();
			
			interpolatedNode = currentNode;
			interpolatedPos = resultPos + dirToNextNode*lookAheadDistance;
			return true;
		}
		else
		{
			//Find which two nodes the look ahead point will be between
			float currentLookAheadDistance = 0.f;
			TNodeId startNode = currentNode;
			TNodeId endNode = currentNode;
			float remainingLookAheadDistance = lookAheadDistance + (resultPos - currentNodePos).GetLength();

			if(currentNode < 0) //Lookahead has crossed the boundary between being off and on the path (After approaching from off the path)
			{
				newNode = 0;
				endNode = 0;
				remainingLookAheadDistance -= sqrt(distanceToNextNode);
			}

			float lastDistanceChecked = 0.f;
			while(remainingLookAheadDistance > 0.f)
			{
				++endNode;
				if(endNode <= m_MaxNodeIndex)
				{
					lastDistanceChecked = m_Nodes[endNode].distanceAlongPath - m_Nodes[endNode-1].distanceAlongPath;
				}
				else
				{
					endNode = 0;
					lastDistanceChecked = (m_Nodes[endNode].pos - m_Nodes[m_MaxNodeIndex].pos).GetLength();
				}
				remainingLookAheadDistance -= lastDistanceChecked;
			}

			//We are now looking one node too far so step back one
			remainingLookAheadDistance += lastDistanceChecked;
			TNodeId node1Index = endNode-1;
			TNodeId node2Index = endNode;
			if(node1Index < 0)
			{
				if(!loop && remainingLookAheadDistance < lookAheadDistance)
				{
					interpolatedNode = m_MaxNodeIndex;
					interpolatedPos = m_Nodes[m_MaxNodeIndex].pos;
					newNode = m_MaxNodeIndex;
					return false; //Not looping so finished
				}

				node1Index = m_MaxNodeIndex;
			}

			//Finally, find out the actual position between the two nodes we now know we will be between
			const Vec3& node1Pos = m_Nodes[node1Index].pos;
			const Vec3& node2Pos = m_Nodes[node2Index].pos;

			Vec3 dirToNextNode = (node2Pos-node1Pos).GetNormalized();
			interpolatedNode = node1Index;
			interpolatedPos = node1Pos + dirToNextNode*remainingLookAheadDistance;

#ifndef _RELEASE
			/*IRenderAuxGeom* pRenderAuxGeom ( gEnv->pRenderer->GetIRenderAuxGeom() );
			SAuxGeomRenderFlags oldFlags = pRenderAuxGeom->GetRenderFlags();

			pRenderAuxGeom->SetRenderFlags(e_Def3DPublicRenderflags|e_AlphaBlended|e_DrawInFrontOn|e_DepthWriteOff|e_DepthTestOff);

			ColorF nodeColour(1.0f, 1.0f, 0.0f);
			ColorF currentNodeColour(0.0f, 1.0f, 1.0f);
			ColorF currentPosColour(1.0f, 1.0f, 1.0f);
			pRenderAuxGeom->DrawSphere(node1Pos, 0.2f, nodeColour, false);
			pRenderAuxGeom->DrawSphere(node2Pos, 0.2f, nodeColour, false);
			pRenderAuxGeom->DrawSphere(currentNodePos, 0.2f, currentNodeColour, false);
			pRenderAuxGeom->DrawSphere(currentPos, 0.2f, currentPosColour, false);

			pRenderAuxGeom->SetRenderFlags(oldFlags);*/
#endif

			return true;		
		}
	}
	//Already at or moving towards the final node
	else
	{
		newNode = m_MaxNodeIndex;
		interpolatedNode = m_MaxNodeIndex;
		interpolatedPos = m_Nodes[m_MaxNodeIndex].pos;
		newPathLoc = static_cast<float>(m_MaxNodeIndex);
	}

	return false;
}

bool CWaypointPath::HasDataAtNode( TNodeId node, E_NodeDataType& dataType, float& outSpeed ) const
{
	NodeDataMap::const_iterator result = m_nodeDataMap.find(node);
	if(result != m_nodeDataMap.end())
	{
		dataType = result->second.type;
		outSpeed = result->second.fValue;
		return true;
	}

	dataType = ENDT_None;
	outSpeed = 0.f;
	return false;
}

float CWaypointPath::GetDistance( float from, float to, bool looping ) const
{
	const TNodeId totalNodes = m_MaxNodeIndex+1;

	const float totalDist = m_Nodes[m_MaxNodeIndex].distanceAlongPath + m_Nodes[m_MaxNodeIndex].pos.GetDistance(m_Nodes[0].pos);

	const TNodeId fromA = looping ? (((TNodeId)floor_tpl(from))+totalNodes)%totalNodes : ((TNodeId)floor_tpl(from))<0?0:((TNodeId)floor_tpl(from));;
	const TNodeId fromB = looping ? (((TNodeId)ceil_tpl(from))+totalNodes)%totalNodes : (fromA+1)>m_MaxNodeIndex?m_MaxNodeIndex:fromA+1;
	const float fromLerp = from - fromA;
	const float fromDistAlongPath = m_Nodes[fromA].distanceAlongPath + (((fromB<fromA?totalDist:0.f)+m_Nodes[fromB].distanceAlongPath-m_Nodes[fromA].distanceAlongPath)*fromLerp);

	const TNodeId toA = looping ? (((TNodeId)floor_tpl(to))+totalNodes)%totalNodes : ((TNodeId)floor_tpl(to))<0?0:((TNodeId)floor_tpl(to));;
	const TNodeId toB = looping ? (((TNodeId)ceil_tpl(to))+totalNodes)%totalNodes : (toA+1)>m_MaxNodeIndex?m_MaxNodeIndex:toA+1;
	const float toLerp = to - toA;
	const float toDistAlongPath = m_Nodes[toA].distanceAlongPath + (((toB<toA?totalDist:0.f)+m_Nodes[toB].distanceAlongPath-m_Nodes[toA].distanceAlongPath)*toLerp);

	bool viaLoop = false;
	float diff = toDistAlongPath - fromDistAlongPath;
	if( looping )
	{
		const float absDiff = (float)__fsel( diff, diff, -diff );
		const float halfDist = totalDist*0.5f;
		return (float)__fsel( absDiff-halfDist, (float)__fsel(diff,diff-totalDist,diff+totalDist), diff );
	}
	return diff;
}

CWaypointPath::TNodeId CWaypointPath::GetNearest( const Vec3& pos ) const
{
	float bestDist=FLT_MAX;
	TNodeId bestNode=0;
	for(TNodeId i=0; i<=m_MaxNodeIndex; ++i)
	{
		float dist = m_Nodes[i].pos.GetSquaredDistance(pos);
		if(dist<bestDist)
		{
			bestDist=dist;
			bestNode=i;
		}
	}
	return bestNode;
}

void CWaypointPath::GetPathLoc( float pathLoc, QuatT& location, TNodeId& node ) const
{
	const TNodeId totalNodes = m_MaxNodeIndex+1;
	const TNodeId a = (((TNodeId)floor_tpl(pathLoc))+totalNodes)%totalNodes;
	const TNodeId b = (((TNodeId)ceil_tpl(pathLoc))+totalNodes)%totalNodes;
	const float lerpVal = pathLoc - a;
	QuatT aQ(GetMatrixAtNode(a,true));
	QuatT bQ(GetMatrixAtNode(b,true));
	location.SetNLerp(aQ,bQ,lerpVal);
	node = a;
}




#ifndef _RELEASE

void CWaypointPath::DebugDraw(bool renderLooped) const
{
	IRenderAuxGeom* pRenderAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();
	if(!pRenderAuxGeom)
		return;

	SAuxGeomRenderFlags oldFlags = pRenderAuxGeom->GetRenderFlags();
	pRenderAuxGeom->SetRenderFlags(e_Def3DPublicRenderflags|e_AlphaBlended);

	ColorF nodeColour(1.0f, 0.0f, 0.0f);
	ColorF startNodeColour(0.0f, 0.0f, 1.0f);
	ColorF lineColour(0.0f, 1.0f, 0.0f);
	float textColour[4] = {1.0f, 1.0f, 1.0f, 1.0f};
	Vec3 vTextPosOffset(0.0f,0.0f,0.5f);
	string textString = "";

	for(TNodeId i = 0; i <= m_MaxNodeIndex; ++i)
	{
		pRenderAuxGeom->DrawSphere(m_Nodes[i].pos, 0.1f, nodeColour, false);
		IRenderAuxText::DrawLabelExF(m_Nodes[i].pos+vTextPosOffset, g_pGameCVars->g_mpPathFollowingNodeTextSize, textColour, true, true, "%i", i);

		if(i > 0)
		{
			pRenderAuxGeom->DrawLine(m_Nodes[i-1].pos, lineColour, m_Nodes[i].pos, lineColour, 0.1f);
		}
		else
		{
			pRenderAuxGeom->DrawSphere(m_Nodes[i].pos, 0.2f, startNodeColour, false);
		}
	}

	if(renderLooped)
	{
		pRenderAuxGeom->DrawLine(m_Nodes[m_MaxNodeIndex].pos, lineColour, m_Nodes[0].pos, lineColour, 0.1f);
	}



	pRenderAuxGeom->SetRenderFlags(oldFlags);
}
#endif
