// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SubObjectSelectioNReferenceFrameCalculator.h"

SubObjectSelectionReferenceFrameCalculator::SubObjectSelectionReferenceFrameCalculator(ESubObjElementType selectionType)
	: bAnySelected(false),
	pos(0.0f, 0.0f, 0.0f),
	normal(0.0f, 0.0f, 0.0f),
	nNormals(0),
	selectionType(selectionType),
	bUseExplicitFrame(false),
	bExplicitAnySelected(false)
{
}

void SubObjectSelectionReferenceFrameCalculator::SetExplicitFrame(bool bAnySelected, const Matrix34& refFrame)
{
	this->refFrame = refFrame;
	this->bUseExplicitFrame = true;
	this->bExplicitAnySelected = bAnySelected;
}

bool SubObjectSelectionReferenceFrameCalculator::GetFrame(Matrix34& refFrame)
{
	if (this->bUseExplicitFrame)
	{
		refFrame = this->refFrame;
		return this->bExplicitAnySelected;
	}
	else
	{
		refFrame.SetIdentity();

		if (this->nNormals > 0)
		{
			this->normal = this->normal / this->nNormals;
			if (!this->normal.IsZero())
				this->normal.Normalize();

			// Average position.
			this->pos = this->pos / this->nNormals;
			refFrame.SetTranslation(this->pos);
		}

		if (this->bAnySelected)
		{
			if (!this->normal.IsZero())
			{
				Vec3 xAxis(1, 0, 0), yAxis(0, 1, 0), zAxis(0, 0, 1);
				if (this->normal.IsEquivalent(zAxis) || normal.IsEquivalent(-zAxis))
					zAxis = xAxis;
				xAxis = this->normal.Cross(zAxis).GetNormalized();
				yAxis = xAxis.Cross(this->normal).GetNormalized();
				refFrame.SetFromVectors(xAxis, yAxis, normal, pos);
			}
		}

		return bAnySelected;
	}
}

