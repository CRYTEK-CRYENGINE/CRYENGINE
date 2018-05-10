// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __RULERPATHAGENT_H__
#define __RULERPATHAGENT_H__

#include <CryAISystem/IAgent.h>
#include <CryAISystem/IPathfinder.h>

class CRuler;
class CRulerPoint;

//! Ruler path agent, to hook in to AI path finder.
class CRulerPathAgent : public IAIPathAgent
{
public:
	CRulerPathAgent();
	virtual ~CRulerPathAgent();

	void          Init(CRuler* pRuler);
	void          Render(IRenderer* pRenderer, const ColorF& colour);

	void          SetType(uint32 type, const string& name);
	uint32        GetType() const { return m_agentType; }
	const string& GetName() const { return m_agentName; }

	// Request a path
	bool RequestPath(const CRulerPoint& startPoint, const CRulerPoint& endPoint);
	void ClearLastRequest();
	bool HasQueuedPaths() const { return m_bPathQueued; }

	// Get path output
	bool  GetLastPathSuccess() const { return m_bLastPathSuccess; }
	float GetLastPathDist() const    { return m_fLastPathDist; }

	// IAIPathAgent
	virtual IEntity*                    GetPathAgentEntity() const;
	virtual const char*                 GetPathAgentName() const;
	virtual unsigned short              GetPathAgentType() const;

	virtual float                       GetPathAgentPassRadius() const;
	virtual Vec3                        GetPathAgentPos() const;
	virtual Vec3                        GetPathAgentVelocity() const;

	virtual const AgentMovementAbility& GetPathAgentMovementAbility() const;

	virtual void                        SetPathToFollow(const char* pathName)  {}
	virtual void                        SetPathAttributeToFollow(bool bSpline) {}

	virtual bool           GetValidPositionNearby(const Vec3& proposedPosition, Vec3& adjustedPosition) const { return false; }
	virtual bool           GetTeleportPosition(Vec3& teleportPos) const                                       { return false; }
	virtual IPathFollower* GetPathFollower() const                                                            { return 0; }
	virtual bool           IsPointValidForAgent(const Vec3& pos, uint32 flags) const                          { return true; }
	//~IAIPathAgent

private:
	bool                 m_bPathQueued;
	bool                 m_bLastPathSuccess;
	float                m_fLastPathDist;

	Vec3                 m_vStartPoint;
	AgentMovementAbility m_AgentMovementAbility;
	uint32               m_agentType;
	string               m_agentName;

	PATHPOINTVECTOR      m_path; // caches the path locally to allow multiple requests
};

#endif //__RULERPATHAGENT_H__

