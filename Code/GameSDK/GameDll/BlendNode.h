// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description:  BlendNodes and BlendGroups for ScreenEffects

-------------------------------------------------------------------------
History:
- 23:1:2008   Created by Benito G.R. - Refactor'd from John N. ScreenEffects.h/.cpp

*************************************************************************/

#ifndef _BLEND_NODE_H_
#define _BLEND_NODE_H_

//-BlendJobNode------------------------
// Has all of the information for one blend.
// That is, a blend type, an effect, and a speed.
// Speed is how fast progress goes from 0 to 1.

struct IBlendedEffect;
struct IBlendType;

class CBlendGroup;

class CBlendJobNode
{
	
	friend class CBlendGroup;

	public:
		CBlendJobNode();
		~CBlendJobNode();

		void Init(IBlendType* pBlend, IBlendedEffect* pFx, float speed);
		void Update(float frameTime);
		bool Done() const{ return (m_progress==1.0f);}

		void Reset();

		void GetMemoryUsage(ICrySizer *pSizer ) const {}
	private:
		IBlendedEffect	*m_myEffect;
		IBlendType			*m_blendType;
	
		float m_speed;
		float m_progress;

};


// A blend group is a queue of blend jobs.
class CBlendGroup 
{
	public:
		CBlendGroup();
		~CBlendGroup();

		void Update(float frameTime);
		void AddJob(IBlendType* pBlend, IBlendedEffect* pFx, float speed);
		bool HasJobs();
		void Reset();

		void GetMemoryUsage(ICrySizer* s) const;

	private:

		void AllocateMinJobs();
		typedef std::vector<CBlendJobNode*> TJobVector;
		TJobVector			m_jobs; 

		int						m_currentJob;
		uint32					m_nextFreeSlot;
		uint32					m_activeJobs;

		uint32					m_maxActiveJobs;
			
};

#endif