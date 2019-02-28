// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryMovie/IMovieSystem.h>
#include "HyperGraph/FlowGraphNode.h"

#define TRACKEVENT_CLASS     ("TrackEvent")
#define TRACKEVENTNODE_TITLE ("TrackEvent")
#define TRACKEVENTNODE_CLASS ("TrackEvent")
#define TRACKEVENTNODE_DESC  ("Outputs for Trackview Events")

class CTrackEventNode : public CFlowNode, public ITrackEventListener
{
public:
	static const char* GetClassType()
	{
		return TRACKEVENT_CLASS;
	}
	CTrackEventNode();
	virtual ~CTrackEventNode();

	// CHyperNode
	virtual void           Init() override;
	virtual void           Done() override;
	virtual CHyperNode*    Clone() override;
	virtual void           Serialize(XmlNodeRef& node, bool bLoading, CObjectArchive* ar) override;

	virtual CString        GetTitle() const override;
	virtual const char*    GetClassName() const override;
	virtual const char*    GetDescription() const override;
	virtual Gdiplus::Color GetCategoryColor() const override { return Gdiplus::Color(220, 40, 40); }
	virtual void           OnInputsChanged();
	// ~CHyperNode

	// Description:
	//		Re-populates output ports based on input sequence
	// Arguments:
	//		bLoading - TRUE if called from serialization on loading
	virtual void PopulateOutput(bool bLoading);

	// ~ITrackEventListener
	virtual void OnTrackEvent(IAnimSequence* pSequence, int reason, const char* event, void* pUserData);

protected:
	//! Add an output port for an event
	virtual void AddOutputEvent(const char* event);

	//! Remove an output port for an event (including any edges)
	virtual void RemoveOutputEvent(const char* event);

	//! Rename an output port for an event
	virtual void RenameOutputEvent(const char* event, const char* newName);

	//! Move up a specified output port once in the list of output ports
	virtual void MoveUpOutputEvent(const char* event);

	//! Move down a specified output port once in the list of output ports
	virtual void MoveDownOutputEvent(const char* event);

private:
	IAnimSequence* m_pSequence; // Current sequence
};

