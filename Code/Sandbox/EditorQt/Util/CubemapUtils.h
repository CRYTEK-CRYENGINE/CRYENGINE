// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#if !defined __cubemaputils_h_
#define __cubemaputils_h_

namespace CubemapUtils
{
bool GenCubemapWithPathAndSize(string& filename, const int size, const bool dds = true);
bool GenCubemapWithObjectPathAndSize(string& filename, CBaseObject* pObject, const int size, const bool dds);
void GenHDRCubemapTiff(const string& fileName, std::size_t size, Vec3& pos);
void RegenerateAllEnvironmentProbeCubemaps();
void GenerateCubemaps();
}

#endif //__cubemaputils_h_

