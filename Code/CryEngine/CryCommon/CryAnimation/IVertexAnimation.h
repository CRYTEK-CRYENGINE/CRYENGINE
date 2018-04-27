// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
////////////////////////////////////////////////////////////////////////////

#ifndef IVertexAnimation_h
#define IVertexAnimation_h

struct IVertexFrames
{
	virtual ~IVertexFrames() {}

	virtual uint        GetCount() const = 0;

	virtual const char* GetNameByIndex(const uint index) const = 0;
	virtual uint        GetIndexByName(const char* name) const = 0;
};

struct IVertexAnimation
{
	virtual ~IVertexAnimation() {}

	virtual void OverrideFrameWeightByName(const ISkin* pISkin, const char* name, float weight) = 0;
	virtual bool OverridenFrameWeights() = 0;
	virtual void OverrideFrameWeights(bool val) = 0;
	virtual void ClearAllFramesWeight() = 0;
};

#endif // IVertexAnimation_h
