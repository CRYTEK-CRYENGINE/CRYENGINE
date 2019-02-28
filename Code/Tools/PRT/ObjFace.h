#pragma once
#include "SHAllocator.h"

struct CObjFace
{
	INSTALL_CLASS_NEW(CObjFace)

	int v[3];     //!< 3 position indices
	int t[3];     //!< 3 texture indices
	int n[3];     //!< 3 normal indices
	int shaderID; //!< material identifier index into m_ObjMaterials
	CObjFace() : shaderID(-1) {}
};