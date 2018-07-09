// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __ISKINNINGDATA_H__
#define __ISKINNINGDATA_H__

class ISkinningData
{
public:
	virtual void SetVertexCount(int vertexCount) = 0;
	virtual void AddWeight(int vertexIndex, int boneIndex, float weight) = 0;
};

#endif //__ISKINNINGDATA_H__
