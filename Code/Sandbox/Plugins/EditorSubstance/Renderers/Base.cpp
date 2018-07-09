// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Base.h"
#include "EditorSubstanceManager.h"

namespace EditorSubstance
{
	namespace Renderers
	{



		CInstanceRenderer::CInstanceRenderer()
		{

		}

		void CInstanceRenderer::OnOutputAvailable(SubstanceAir::UInt runUid, const SubstanceAir::GraphInstance *graphInstance, SubstanceAir::OutputInstanceBase * outputInstance)
{
	CryAutoLock<CryMutexFast> lock(m_mutex);
	m_queue.emplace(outputInstance);
}

OutputsSet CInstanceRenderer::GetComputedOutputs()
{
	CryAutoLock<CryMutexFast> lock(m_mutex);
	OutputsSet outputs;

	if (m_queue.size())
	{
		outputs = m_queue;
		m_queue.clear();
	}

	return outputs;
}

SubstanceAir::InputImage::SPtr CInstanceRenderer::GetInputImage(const ISubstancePreset* preset, const string& path)
{
	return EditorSubstance::CManager::Instance()->GetInputImage(preset, path);
}

bool CInstanceRenderer::OutputsInQueue()
{
	return m_queue.size() > 0;
}

void CInstanceRenderer::RemoveItemFromQueue(SubstanceAir::OutputInstanceBase* output)
{
	CryAutoLock<CryMutexFast> lock(m_mutex);
	m_queue.erase(output);
}



	} // END namespace Renderers
} // END namespace EditorSubstance

