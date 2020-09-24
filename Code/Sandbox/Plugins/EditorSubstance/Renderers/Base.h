// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "ISubstanceInstanceRenderer.h"
#include "substance/framework/typedefs.h"

namespace SubstanceAir
{
	class GraphInstance;
	class OutputInstance;
}


namespace EditorSubstance
{
namespace Renderers
{

typedef std::unordered_set<SubstanceAir::OutputInstance*> OutputsSet;

class CInstanceRenderer : public ISubstanceInstanceRenderer
{
public:
	virtual void OnOutputAvailable(SubstanceAir::UInt runUid, const SubstanceAir::GraphInstance *graphInstance,
		SubstanceAir::OutputInstance * outputInstance) override;

	virtual void ProcessComputedOutputs() = 0;
	virtual bool OutputsInQueue();
	virtual void RemoveItemFromQueue(SubstanceAir::OutputInstance* output);
	virtual SubstanceAir::InputImage::SPtr GetInputImage(const ISubstancePreset* preset, const string& path) override;
	virtual void RemovePresetRenderData(ISubstancePreset* preset) = 0;
protected:
	virtual OutputsSet GetComputedOutputs();


	CryMutexFast m_mutex;
	OutputsSet m_queue;
};


} // END namespace Renderers
} // END namespace EditorSubstance
