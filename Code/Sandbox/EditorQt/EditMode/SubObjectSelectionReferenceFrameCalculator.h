// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Objects\SubObjSelection.h"

class SubObjectSelectionReferenceFrameCalculator
{
public:
	SubObjectSelectionReferenceFrameCalculator(ESubObjElementType selectionType);

	virtual void SetExplicitFrame(bool bAnySelected, const Matrix34& refFrame);
	bool         GetFrame(Matrix34& refFrame);

private:
	bool               bAnySelected;
	Vec3               pos;
	Vec3               normal;
	int                nNormals;
	ESubObjElementType selectionType;
	std::vector<Vec3>  positions;
	Matrix34           refFrame;
	bool               bUseExplicitFrame;
	bool               bExplicitAnySelected;
};

