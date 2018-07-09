// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __SubObjSelection_h__
#define __SubObjSelection_h__
#pragma once

//////////////////////////////////////////////////////////////////////////
// Sub Object element type.
//////////////////////////////////////////////////////////////////////////
enum ESubObjElementType
{
	SO_ELEM_NONE = 0,
	SO_ELEM_VERTEX,
	SO_ELEM_EDGE,
	SO_ELEM_FACE,
	SO_ELEM_POLYGON,
	SO_ELEM_UV,
};

//////////////////////////////////////////////////////////////////////////
enum ESubObjDisplayType
{
	SO_DISPLAY_WIREFRAME,
	SO_DISPLAY_FLAT,
	SO_DISPLAY_GEOMETRY,
};

//////////////////////////////////////////////////////////////////////////
// Options for sub-object selection.
//////////////////////////////////////////////////////////////////////////
struct SSubObjSelOptions
{
	bool  bSelectByVertex;
	bool  bIgnoreBackfacing;
	int   nMatID;

	bool  bSoftSelection;
	float fSoftSelFalloff;

	// Display options.
	bool               bDisplayBackfacing;
	bool               bDisplayNormals;
	float              fNormalsLength;
	ESubObjDisplayType displayType;

	SSubObjSelOptions()
	{
		bSelectByVertex = false;
		bIgnoreBackfacing = false;
		bSoftSelection = false;
		nMatID = 0;
		fSoftSelFalloff = 1;

		bDisplayBackfacing = true;
		bDisplayNormals = false;
		displayType = SO_DISPLAY_FLAT;
		fNormalsLength = 0.4f;
	}
};

extern SSubObjSelOptions g_SubObjSelOptions;

//////////////////////////////////////////////////////////////////////////
enum ESubObjSelectionModifyType
{
	SO_MODIFY_UNSELECT,
	SO_MODIFY_MOVE,
	SO_MODIFY_ROTATE,
	SO_MODIFY_SCALE,
};

//////////////////////////////////////////////////////////////////////////
// This structure is passed when user is dragging sub object selection.
//////////////////////////////////////////////////////////////////////////
struct SSubObjSelectionModifyContext
{
	CViewport*                 view;
	ESubObjSelectionModifyType type;
	Vec3                       vValue;
	Matrix34                   worldRefFrame;
};

#endif //__SubObjSelection_h__

