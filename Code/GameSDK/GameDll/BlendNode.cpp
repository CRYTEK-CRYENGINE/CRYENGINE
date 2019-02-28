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

#include "StdAfx.h"
#include "BlendNode.h"
#include "BlendTypes.h"
#include "BlendedEffect.h"

#define MIN_BLEND_GROUP_SIZE	8  

//------------------CBlendNode-----------------------------

CBlendJobNode::CBlendJobNode():
m_speed(0.0f),
m_progress(0.0f),
m_myEffect(NULL),
m_blendType(NULL)
{
}

//--------------------------------------
void CBlendJobNode::Init(IBlendType* pBlend, IBlendedEffect* pFx, float speed)
{
	m_blendType = pBlend;
	m_myEffect	= pFx;
	m_speed			= speed;
}
//-------------------------------------------------
CBlendJobNode::~CBlendJobNode()
{
	Reset();
}

//--------------------------------------------
void CBlendJobNode::Reset()
{
	if(m_myEffect)
		m_myEffect->Release();

	if(m_blendType)
		m_blendType->Release();

	m_myEffect = NULL; m_blendType = NULL;
	m_speed = m_progress = 0.0f;
}

//-----------------------------------------------
void CBlendJobNode::Update(float frameTime)
{
	float progressDifferential = m_speed * frameTime;
	m_progress = min(m_progress + progressDifferential, 1.0f);
	
	float point = 0.1f;
	if(m_blendType)
		point = m_blendType->Blend(m_progress);
	if(m_myEffect)
		m_myEffect->Update(point);
}

//-------------------CBlendGroup----------------------------

CBlendGroup::CBlendGroup():
m_currentJob(-1),
m_nextFreeSlot(0),
m_activeJobs(0),
m_maxActiveJobs(0)
{
	AllocateMinJobs();
}

//---------------------------------
CBlendGroup::~CBlendGroup()
{
	for(size_t i=0;i<m_jobs.size();i++)
	{
		delete m_jobs[i];
	}
}

//---------------------------------
void CBlendGroup::AllocateMinJobs()
{
	m_jobs.reserve(MIN_BLEND_GROUP_SIZE);

	for(int i=0;i<MIN_BLEND_GROUP_SIZE;i++)
	{
		CBlendJobNode *node = new CBlendJobNode();
		m_jobs.push_back(node);
	}
}
//----------------------------------------
void CBlendGroup::Update(float frameTime)
{
	int jobsSize = m_jobs.size();
	if (m_currentJob>=0 && m_currentJob<jobsSize)
	{
		m_jobs[m_currentJob]->Update(frameTime);
		if (m_jobs[m_currentJob]->Done())
		{
			m_jobs[m_currentJob]->Reset();
			m_activeJobs--;
			
			if(m_activeJobs>0)
			{
				m_currentJob++;
				if(m_currentJob==jobsSize)
					m_currentJob = 0;

				if (m_jobs[m_currentJob]->m_myEffect)
					m_jobs[m_currentJob]->m_myEffect->Init();
			}
			else
			{
				//Reset if there are no more jobs
				m_currentJob = -1;
				m_nextFreeSlot = 0;
			}

		}
	}
}

//--------------------------------------
bool CBlendGroup::HasJobs()
{
	return (m_activeJobs>0);
}

//---------------------------------------------
void CBlendGroup::AddJob(IBlendType* pBlend, IBlendedEffect* pFx, float speed)
{
	int jobsSize = m_jobs.size();
	if(m_activeJobs==jobsSize)
	{
		//We need to add another slot
		CBlendJobNode* newJob = new CBlendJobNode();
		newJob->Init(pBlend,pFx,speed);

		if(m_nextFreeSlot==0)
		{
			m_jobs.push_back(newJob);
		}
		else
		{
			TJobVector::iterator it = m_jobs.begin() + m_nextFreeSlot;
			m_jobs.insert(it,newJob);
			m_nextFreeSlot++;
		}
		GameWarning("CBlendGroup::AddJob() needs to add a new slot (You might want to reserve more on init). New size: %d",jobsSize+1);
		m_activeJobs++;

	}
	else
	{
		m_jobs[m_nextFreeSlot]->Init(pBlend,pFx,speed);
		m_activeJobs++;
		m_nextFreeSlot++;
		if(m_nextFreeSlot==jobsSize)
			m_nextFreeSlot=0;
	}

	if(m_currentJob==-1)
	{
		m_currentJob = 0;
		m_jobs[m_currentJob]->m_myEffect->Init();	
	}

	if(m_activeJobs>m_maxActiveJobs)
		m_maxActiveJobs=m_activeJobs;

	jobsSize = m_jobs.size();
	assert(m_activeJobs>=0 && m_activeJobs<=m_jobs.size() && "CBlendGroup::AddJob() --> 0h, oh!");
}

//-----------------------------------
void CBlendGroup::Reset()
{
	for(size_t i=0; i<m_jobs.size(); i++ )
	{
		m_jobs[i]->Reset();
		m_currentJob = -1;
		m_nextFreeSlot = 0;
		m_activeJobs = 0;
	}
}

//------------------------------------
void CBlendGroup::GetMemoryUsage(ICrySizer* s) const
{
	s->Add(this);
	s->AddContainer(m_jobs);

}