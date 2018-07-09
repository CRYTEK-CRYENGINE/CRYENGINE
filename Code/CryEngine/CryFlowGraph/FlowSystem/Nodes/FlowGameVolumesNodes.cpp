// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// Generic nodes for GameVolumes

#include "StdAfx.h"

#include <CryFlowGraph/IFlowBaseNode.h>
#include <CryMath/Cry_GeoOverlap.h>
#include <CryGame/IGameVolumes.h>

class CEntityGameVolumePositionTestNode : public CFlowBaseNode<eNCT_Singleton>
{
	enum INPUTS
	{
		eIP_TestWorldPosition = 0,
		eIP_WorldPosition,
	};

	enum OUTPUTS
	{
		eOP_Inside = 0,
		eOP_Outside,
		eOP_Failed,
	};

public:
	CEntityGameVolumePositionTestNode(SActivationInfo* pActInfo)
	{
	}

	virtual void GetMemoryUsage(ICrySizer * s) const override
	{
		s->Add(*this);
	}

	virtual void GetConfiguration(SFlowNodeConfig &config) override
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void ("TestWorldPosition", _HELP("Test whether the specified WorldPosition is inside the selected GameVolume or not.")),
			InputPortConfig<Vec3>("WorldPosition", Vec3(ZERO), _HELP("Position to be tested, in World space.")),
			{0}
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_AnyType("Inside",  _HELP("Triggered when the specified WorldPosition is contained inside the GameVolume.")),
			OutputPortConfig_AnyType("Outside", _HELP("Triggered when the specified WorldPosition is outside of the GameVolume.")),
			OutputPortConfig_AnyType("Failed",  _HELP("Triggered when the specified entity has no Volume information.")),
			{0}
		};
		config.nFlags = EFLN_TARGET_ENTITY;
		config.sDescription = _HELP("This node lets you do basic tests with GameVolumes. Should be used for protoyping only.");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}


	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo) override
	{
		LOADING_TIME_PROFILE_SECTION;
		switch (event)
		{
		case eFE_Activate:
			{
				if (pActInfo->pEntity)
				{
					if (IsPortActive(pActInfo, eIP_TestWorldPosition))
					{
						const Vec3 worldPos = GetPortVec3(pActInfo, eIP_WorldPosition);

						IGameVolumes* pGameVolumesManager = gEnv->pGameFramework->GetIGameVolumesManager();
						IGameVolumes::VolumeInfo volumeInfo;
						if (pGameVolumesManager->GetVolumeInfoForEntity(pActInfo->pEntity->GetId(), &volumeInfo))
						{
							const Matrix34& invWorldTM = pActInfo->pEntity->GetWorldTM().GetInverted();
							const Vec3 localPos = invWorldTM * worldPos;

							AABB volumeLocalAABB( AABB::RESET );
							for (size_t i = 0; i < volumeInfo.verticesCount; ++i)
							{
								volumeLocalAABB.Add(volumeInfo.pVertices[i]);
							}
							if(volumeInfo.volumeHeight > 0.0f)
							{
								volumeLocalAABB.max.z += volumeInfo.volumeHeight;
							}
							else
							{
								volumeLocalAABB.min.z += volumeInfo.volumeHeight;
							}

							bool bIsContained = false;
							if (volumeLocalAABB.IsContainPoint(localPos))
							{
								bIsContained = Overlap::Point_Polygon2D(localPos, volumeInfo.pVertices, volumeInfo.verticesCount, &volumeLocalAABB);
							}

							ActivateOutput(pActInfo, bIsContained ? eOP_Inside : eOP_Outside, true);
						}
						else
						{
							GameWarning("[CEntityGameVolumePositionTestNode] Selected entity \"%s\" doesn't have any volume information.", pActInfo->pEntity->GetName());
							ActivateOutput(pActInfo, eOP_Failed, true);
						}
					}
				}
			}
			break;
		}
	}

};

class CEntityGameVolumeDistanceTestNode : public CFlowBaseNode<eNCT_Singleton>
{
	enum INPUTS
	{
		eIP_GetDistance = 0,
		eIP_EntityReference,
		eIP_WorldPosition,
	};

	enum OUTPUTS
	{
		eOP_Inside = 0,
		eOP_Outside,
		eOP_DistanceToVolume,
		eOP_DistanceToSegment,
		eOP_ClosestPointOnSegment,
		eOP_SegmentDirection,
		eOP_Failed,
	};

public:
	CEntityGameVolumeDistanceTestNode(SActivationInfo* pActInfo)
	{
	}

	virtual void GetMemoryUsage(ICrySizer * s) const override
	{
		s->Add(*this);
	}

	virtual void GetConfiguration(SFlowNodeConfig &config) override
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void     ("GetDistance", _HELP("Get the distance from the specified world position to the volume itself and to the closest segment inside the volume.")),
			InputPortConfig<EntityId>("EntityReference", INVALID_ENTITYID, _HELP("Fill this with a proper entity to test the distance from that entity to the volume. Leave it to its default value to test with the WorldPosition.")),
			InputPortConfig<Vec3>    ("WorldPosition", Vec3(ZERO), _HELP("Position to be tested, in World space, if no entity is provided.")),
			{0}
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_AnyType("Inside",                _HELP("Triggered when the specified WorldPosition is contained inside the GameVolume.")),
			OutputPortConfig_AnyType("Outside",               _HELP("Triggered when the specified WorldPosition is outside of the GameVolume.")),
			OutputPortConfig<float> ("DistanceToVolume",      _HELP("Distance from the world pos to the volume (0 if the volume is closed and the point is inside it, else, same as distanceToSegment).")),
			OutputPortConfig<float> ("DistanceToSegment",     _HELP("Distance from the world pos to the volume's segment (0 if point is actually on a segment).")),
			OutputPortConfig<Vec3>  ("ClosestPointOnSegment", _HELP("Point on the segment that is the closest to the specified world pos.")),
			OutputPortConfig<Vec3>  ("SegmentDirection",      _HELP("Direction of the segment the closest to the target.")),
			OutputPortConfig_AnyType("Failed",                _HELP("Triggered upon failure.")),
			{0}
		};
		config.nFlags = EFLN_TARGET_ENTITY;
		config.sDescription = _HELP("This node lets you do basic tests with GameVolumes. Should be used for protoyping only.");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	struct SProximityTestResult
	{
		SProximityTestResult()
			: location(ZERO)
			, direction(0.0f, 0.0f, 1.0f)
			, squareDistance(FLT_MAX)
		{
		}
		bool HasValidResult() const { return squareDistance != FLT_MAX; }
		Vec3 location;
		Vec3 direction;
		float squareDistance;
	};

	void UpdateClosestPoint(SProximityTestResult& inOutResult, const Vec3& reference, const Vec3& p1, const Vec3& p2)
	{
		Lineseg segment(p1, p2);
		float t = 0.0f;
		float newSquareDistance = Distance::Point_LinesegSq(reference, segment, t);
		if (newSquareDistance < inOutResult.squareDistance)
		{
			inOutResult.direction = (p2 - p1);
			inOutResult.location = p1 + (inOutResult.direction * t);
			inOutResult.squareDistance = newSquareDistance;
		}
	}


	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo) override
	{
		LOADING_TIME_PROFILE_SECTION;
		switch (event)
		{
		case eFE_Activate:
			{
				if (pActInfo->pEntity)
				{
					if (IsPortActive(pActInfo, eIP_GetDistance))
					{
						IGameVolumes* pGameVolumesManager = gEnv->pGameFramework->GetIGameVolumesManager();
						IGameVolumes::VolumeInfo volumeInfo;
						if (pGameVolumesManager->GetVolumeInfoForEntity(pActInfo->pEntity->GetId(), &volumeInfo))
						{
							if (volumeInfo.verticesCount > 1)
							{
								const EntityId entityReference = GetPortEntityId(pActInfo, eIP_EntityReference);
								const Vec3 worldPos = (entityReference != INVALID_ENTITYID)
									? gEnv->pEntitySystem->GetEntity( entityReference )->GetWorldPos()
									: GetPortVec3(pActInfo, eIP_WorldPosition);

								const Matrix34& volumeTM = pActInfo->pEntity->GetWorldTM();
								const Matrix34& invVolumeTM = volumeTM.GetInverted();
								const Vec3 localPos = invVolumeTM * worldPos;

								bool bIsContained = false;
								if (volumeInfo.closed)
								{
									AABB volumeLocalAABB( AABB::RESET );
									for (size_t i = 0; i < volumeInfo.verticesCount; ++i)
									{
										volumeLocalAABB.Add(volumeInfo.pVertices[i]);
									}
									if (volumeInfo.volumeHeight > 0.0f)
									{
										volumeLocalAABB.max.z += volumeInfo.volumeHeight;
									}
									else
									{
										volumeLocalAABB.min.z += volumeInfo.volumeHeight;
									}

									if (volumeLocalAABB.IsContainPoint(localPos))
									{
										bIsContained = Overlap::Point_Polygon2D(localPos, volumeInfo.pVertices, volumeInfo.verticesCount, &volumeLocalAABB);
									}
								}

								ActivateOutput(pActInfo, bIsContained ? eOP_Inside : eOP_Outside, true);

								SProximityTestResult proximityResult;
								for (uint32 i = 0; i < volumeInfo.verticesCount - 1; ++i)
								{
									UpdateClosestPoint(proximityResult, localPos, volumeInfo.pVertices[i], volumeInfo.pVertices[i + 1]);
								}

								if (volumeInfo.closed)
								{
									UpdateClosestPoint(proximityResult, localPos, volumeInfo.pVertices[volumeInfo.verticesCount - 1], volumeInfo.pVertices[0]);
								}

								if (proximityResult.HasValidResult())
								{
									const float distance              = sqrtf(proximityResult.squareDistance);
									const Vec3& worldPointOnSegment   = volumeTM.TransformPoint(proximityResult.location);
									const Vec3& worldSegmentDirection = volumeTM.TransformVector(proximityResult.direction);

									// Todo : DistanceToVolume can clearly be improved. Right now, it will give a distance to the segments when outside of the volume
									// which is always >= to the actual distance to the volume.
									// For now, this should be fine as the intend is to use it for non-closed shape, thus volume makes no sense.

									ActivateOutput(pActInfo, eOP_DistanceToVolume, bIsContained ? 0.0f : distance);
									ActivateOutput(pActInfo, eOP_DistanceToSegment, distance);
									ActivateOutput(pActInfo, eOP_ClosestPointOnSegment, worldPointOnSegment);
									ActivateOutput(pActInfo, eOP_SegmentDirection, worldSegmentDirection.GetNormalized());
								}
								else
								{
									GameWarning("[CEntityGameVolumeDistanceTestNode] Selected entity \"%s\" didn't lead to valid distance checks. Might have corrupt volume info.", pActInfo->pEntity->GetName());
									ActivateOutput(pActInfo, eOP_Failed, true);
								}
							}
							else
							{
								GameWarning("[CEntityGameVolumeDistanceTestNode] Selected entity \"%s\" doesn't have enough points to be valid for a distance check.", pActInfo->pEntity->GetName());
								ActivateOutput(pActInfo, eOP_Failed, true);
							}
						}
						else
						{
							GameWarning("[CEntityGameVolumeDistanceTestNode] Selected entity \"%s\" doesn't have any volume information.", pActInfo->pEntity->GetName());
							ActivateOutput(pActInfo, eOP_Failed, true);
						}
					}
				}
			}
			break;
		}
	}

};

class CEntityGameVolumeGetInfoNode : public CFlowBaseNode<eNCT_Singleton>
{
	enum INPUTS
	{
		eIP_GetHeight = 0,
		eIP_IsClosed,
		eIP_GetPointCount,
		eIP_GetPointByIndex,
		eIP_PointIndex,
		eIP_GetLastPoint
	};

	enum OUTPUTS
	{
		eOP_Done = 0,
		eOP_Failed,
		eOP_Height,
		eOP_Closed,
		eOP_PointCount,
		eOP_LocalPoint,
		eOP_WorldPoint,
	};

public:
	CEntityGameVolumeGetInfoNode(SActivationInfo* pActInfo)
	{
	}

	virtual void GetMemoryUsage(ICrySizer * s) const override
	{
		s->Add(*this);
	}

	virtual void GetConfiguration(SFlowNodeConfig &config) override
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void("GetHeight",       _HELP("Get the height of the specified volume.")),
			InputPortConfig_Void("IsClosed",        _HELP("Get whether the specified volume is a closed shape or not.")),
			InputPortConfig_Void("GetPointCount",   _HELP("Get the number of points in the specified volume.")),
			InputPortConfig_Void("GetPointByIndex", _HELP("Get the point at the specified index in the specified volume (both in world and local space).")),
			InputPortConfig<int>("PointIndex", 0,   _HELP("Index to the point that is to be fetched.")),
			InputPortConfig_Void("GetLastPoint",    _HELP("Simple direct access to the last point of a shape (equivalent to asking for a point with index = count-1).")),
			{0}
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_AnyType("Success",    _HELP("Triggered upon success.")),
			OutputPortConfig_AnyType("Failed",     _HELP("Triggered on failure (specified entity has no volume information or does not contain a point at the specified index).")),
			OutputPortConfig<float> ("Height",     _HELP("Height defined for the volume.")),
			OutputPortConfig<bool>  ("IsClosed",   _HELP("Whether the volume represents an actual shape rather than just a line of connected segments.")),
			OutputPortConfig<int>   ("PointCount", _HELP("Number of points in the volume.")),
			OutputPortConfig<Vec3>  ("LocalPoint", _HELP("Point in local space.")),
			OutputPortConfig<Vec3>  ("WorldPoint", _HELP("Point in world space.")),
			{0}
		};
		config.nFlags = EFLN_TARGET_ENTITY;
		config.sDescription = _HELP("This node lets you do basic tests with GameVolumes. Should be used for protoyping only.");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo) override
	{
		LOADING_TIME_PROFILE_SECTION;
		switch (event)
		{
		case eFE_Activate:
			{
				if (pActInfo->pEntity)
				{
					const bool bGetHeight       = IsPortActive(pActInfo, eIP_GetHeight);
					const bool bGetIsClosed     = IsPortActive(pActInfo, eIP_IsClosed);
					const bool bGetPointCount   = IsPortActive(pActInfo, eIP_GetPointCount);
					const bool bGetPointByIndex = IsPortActive(pActInfo, eIP_GetPointByIndex);
					const bool bGetLastPoint    = IsPortActive(pActInfo, eIP_GetLastPoint);

					const bool bAnyGetPort
						=  bGetHeight
						|| bGetIsClosed
						|| bGetPointCount
						|| bGetPointByIndex
						|| bGetLastPoint;

					if (bAnyGetPort)
					{
						IGameVolumes* pGameVolumesManager = gEnv->pGameFramework->GetIGameVolumesManager();
						IGameVolumes::VolumeInfo volumeInfo;
						if (pGameVolumesManager->GetVolumeInfoForEntity(pActInfo->pEntity->GetId(), &volumeInfo))
						{
							bool bSuccess = false;
							bool bFailure = false;

							if (bGetHeight)
							{
								bSuccess = true;
								ActivateOutput(pActInfo, eOP_Height, volumeInfo.volumeHeight);
							}

							if (bGetIsClosed)
							{
								bSuccess = true;
								ActivateOutput(pActInfo, eOP_Closed, volumeInfo.closed);
							}

							if (bGetPointCount)
							{
								bSuccess = true;
								ActivateOutput(pActInfo, eOP_PointCount, static_cast<int>(volumeInfo.verticesCount));
							}

							if (bGetPointByIndex)
							{
								int index = GetPortInt(pActInfo, eIP_PointIndex);
								if ((index >= 0) && (index < static_cast<int>(volumeInfo.verticesCount)))
								{
									bSuccess = true;
									const Vec3 localPoint = volumeInfo.pVertices[index];
									const Vec3 worldPoint = pActInfo->pEntity->GetWorldTM() * volumeInfo.pVertices[index];

									ActivateOutput(pActInfo, eOP_LocalPoint, localPoint);
									ActivateOutput(pActInfo, eOP_WorldPoint, worldPoint);
								}
								else
								{
									GameWarning("[CEntityGameVolumeDistanceTestNode] Selected entity \"%s\" doesn't have enough point for getting index %d.", pActInfo->pEntity->GetName(), index);
									bFailure = true;
								}
							}

							if (bGetLastPoint)
							{
								if (volumeInfo.verticesCount>0)
								{
									bSuccess = true;
									const Matrix34& worldTM = pActInfo->pEntity->GetWorldTM();
									const uint32 index = volumeInfo.verticesCount - 1;
									const Vec3 localPoint = volumeInfo.pVertices[index];
									const Vec3 worldPoint = worldTM.TransformPoint(volumeInfo.pVertices[index]);

									ActivateOutput(pActInfo, eOP_LocalPoint, localPoint);
									ActivateOutput(pActInfo, eOP_WorldPoint, worldPoint);
								}
								else
								{
									GameWarning("[CEntityGameVolumeDistanceTestNode] Selected entity \"%s\" has no point available, pActInfo->pEntity->GetName()");
									bFailure = true;
								}
							}

							if (bSuccess) ActivateOutput(pActInfo, eOP_Done, true);
							if (bFailure) ActivateOutput(pActInfo, eOP_Failed, true);
						}
						else
						{
							GameWarning("[CEntityGameVolumeDistanceTestNode] Selected entity \"%s\" doesn't have any volume information.", pActInfo->pEntity->GetName());
							ActivateOutput(pActInfo, eOP_Failed, true);
						}
					}
				}
			}
			break;
		}
	}

};


REGISTER_FLOW_NODE("Entity:GameVolumes:PositionTest", CEntityGameVolumePositionTestNode);
REGISTER_FLOW_NODE("Entity:GameVolumes:DistanceTest", CEntityGameVolumeDistanceTestNode);
REGISTER_FLOW_NODE("Entity:GameVolumes:GetInfo",      CEntityGameVolumeGetInfoNode);
