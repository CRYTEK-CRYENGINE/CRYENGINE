// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "OCMExporter.h"

class CFileEndianWriter
{
	std::vector<uint8> m_Data;
	size_t             m_Offset;
public:
	CFileEndianWriter() : m_Offset(0){}
	void Seek(size_t Offset) { m_Offset = Offset; }
	void Write(const void* pData, size_t Size)
	{
		if (m_Offset + Size > m_Data.size())
			m_Data.resize(m_Offset + Size);
		memcpy(&m_Data[m_Offset], pData, Size);
		m_Offset += Size;
	}
	size_t                    Pos() const  { return m_Offset; }
	template<size_t Padding>
	void                      Align()      { m_Offset = (m_Offset + (Padding - 1)) & ~(Padding - 1); }
	template<class T>
	void                      Write(T D)   { Write(&D, sizeof(T)); }

	const std::vector<uint8>& Data() const { return m_Data; }
};

const char* COCMExporter::GetExtension() const
{
	return "ocm";
}

const char* COCMExporter::GetShortDescription() const
{
	return "occlusion culler mesh";
}

Matrix44 MatRotate(f32 a, f32 b, f32 g)
{
	const f32 CosR = cosf(a);
	const f32 SinR = sinf(a);
	const f32 CosP = cosf(b);
	const f32 SinP = sinf(b);
	const f32 CosY = cosf(g);
	const f32 SinY = sinf(g);
	const f32 SRSP = SinR * SinP;
	const f32 CRSP = CosR * SinP;

	Matrix44 Mat;
	Mat.m00 = CosP * CosY;
	Mat.m01 = CosP * SinY;
	Mat.m02 = -SinP;
	Mat.m03 = 0.f;
	Mat.m10 = SRSP * CosY - CosR * SinY;
	Mat.m11 = SRSP * SinY + CosR * CosY;
	Mat.m12 = SinR * CosP;
	Mat.m13 = 0.f;
	Mat.m20 = CRSP * CosY + SinR * SinY;
	Mat.m21 = CRSP * SinY - SinR * CosY;
	Mat.m22 = CosR * CosP;
	Mat.m23 = 0.f;
	Mat.m30 = 0.f;
	Mat.m31 = 0.f;
	Mat.m32 = 0.f;
	Mat.m33 = 1.f;
	return Mat;
};

void COCMExporter::Extends(const Matrix44& rTransform, const SExportObject* pMesh, f32& rMinX, f32& rMaxX, f32& rMinY, f32& rMaxY, f32& rMinZ, f32& rMaxZ) const
{
	rMinX = rMinY = rMinZ = FLT_MAX;
	rMaxX = rMaxY = rMaxZ = -FLT_MAX;
	const uint32 TriCount = pMesh->GetVertexCount();
	const Export::Vector3D* pVerts = pMesh->GetVertexBuffer();
	for (size_t a = 0; a < TriCount; a++)
	{
		const Export::Vector3D& rV = pVerts[a];
		const Vec3 VO(rV.x, rV.y, rV.z);
		const Vec3 V = rTransform.TransformPoint(VO);
		rMinX = std::min(rMinX, V.x);
		rMinY = std::min(rMinY, V.y);
		rMinZ = std::min(rMinZ, V.z);
		rMaxX = std::max(rMaxX, V.x);
		rMaxY = std::max(rMaxY, V.y);
		rMaxZ = std::max(rMaxZ, V.z);
	}
}

Matrix44 COCMExporter::CalcOBB(const SExportObject* pMesh)
{
	Matrix44 OBBMat(IDENTITY);
	f32 MinX, MaxX, MinY, MaxY, MinZ, MaxZ;
	Extends(OBBMat, pMesh, MinX, MaxX, MinY, MaxY, MinZ, MaxZ);

	OBBMat.m03 = -(MaxX + MinX) * 0.5f;
	OBBMat.m13 = -(MaxY + MinY) * 0.5f;
	OBBMat.m23 = -(MaxZ + MinZ) * 0.5f;

	return OBBMat;

}

size_t COCMExporter::SaveMesh(CFileEndianWriter& rWriter, const SExportObject* pMesh, Matrix44& rOBBMat)
{
	const size_t PosStart = rWriter.Pos();
	rOBBMat = CalcOBB(pMesh);
	const Export::Vector3D* pVerts = pMesh->GetVertexBuffer();
	std::vector<float> Pos;
	const uint32 SubMeshCount = pMesh->GetMeshCount();
	for (uint32 b = 0; b < SubMeshCount; b++)
	{
		const SExportMesh* pSubMesh = pMesh->GetMesh(b);
		const uint32 FaceCount = pSubMesh->GetFaceCount();
		const Export::Face* pFaces = pSubMesh->GetFaceBuffer();
		Pos.reserve(Pos.size() + FaceCount * 9);
		for (size_t a = 0; a < FaceCount; a++)
		{
			const Export::Vector3D& rV0 = pVerts[pFaces[a].idx[0]];
			const Export::Vector3D& rV1 = pVerts[pFaces[a].idx[1]];
			const Export::Vector3D& rV2 = pVerts[pFaces[a].idx[2]];
			const Vec3 VO0(rV0.x, rV0.y, rV0.z);
			const Vec3 VO1(rV1.x, rV1.y, rV1.z);
			const Vec3 VO2(rV2.x, rV2.y, rV2.z);
			const Vec3 V0 = rOBBMat.TransformPoint(VO0);
			const Vec3 V1 = rOBBMat.TransformPoint(VO1);
			const Vec3 V2 = rOBBMat.TransformPoint(VO2);
			const Vec3 N = (V2 - V0).cross(V1 - V0);
			if (fabsf(N.dot(N)) <= FLT_EPSILON)//degenerated?
				continue;
			Pos.push_back(V0.x);
			Pos.push_back(V0.y);
			Pos.push_back(V0.z);
			Pos.push_back(1.f);
			Pos.push_back(V1.x);
			Pos.push_back(V1.y);
			Pos.push_back(V1.z);
			Pos.push_back(1.f);
			Pos.push_back(V2.x);
			Pos.push_back(V2.y);
			Pos.push_back(V2.z);
			Pos.push_back(1.f);
		}
	}
	const uint32 TriCount = static_cast<uint16>(std::min<size_t>(65535, Pos.size() / 4));//vertex count at 4float/vertex
	rWriter.Write(TriCount);
	rWriter.Align<16>();
	rWriter.Write(&Pos[0], 4 * TriCount * sizeof(float));
	rWriter.Align<4>();
	const size_t PosEnd = rWriter.Pos();
	return PosEnd - PosStart;//sizeof(TriCount)+sizeof(Pos[0])*Pos.size();
}

void COCMExporter::SaveInstance(CFileEndianWriter& rWriter, const SExportObject* pInstance, const SOCMeshInfo& rMeshInfo)
{
	Vec3 pos(pInstance->pos.x, pInstance->pos.y, pInstance->pos.z);
	Quat rot(pInstance->rot.w, pInstance->rot.v.x, pInstance->rot.v.y, pInstance->rot.v.z);
	Vec3 scale(pInstance->scale.x, pInstance->scale.y, pInstance->scale.z);

	Matrix44 tm = Matrix33::CreateScale(scale) * Matrix34(rot);
	tm.SetTranslation(pos);

	tm = tm * rMeshInfo.m_OBBMat.GetInverted();

	//const uint32 MeshID	=	pInstance->MeshToUse()->ID();
	rWriter.Write(rMeshInfo.m_Offset);
	for (size_t a = 0; a < 12; a++)//just savin the 3x4 matrix
		rWriter.Write(*(reinterpret_cast<float*>(&tm) + a));
}

bool COCMExporter::ExportToFile(const char* filename, const SExportData* pExportData)
{
	CryLog("Exporting OCM file to '%s'", filename);

	FILE* pFile = fopen(filename, "wb");
	if (!pFile)
	{
		CryLog("Error while opening file '%s'!", filename);
		ASSERT(pFile);
		return false;
	}

	CFileEndianWriter Writer;
	// Write header
	const uint32 Version = ~(4u << 24);
	const uint32 MeshCount = pExportData->GetObjectCount();
	const uint32 InstCount = pExportData->GetObjectCount();
	uint32 OffsetInstances = 0;
	Writer.Write(Version);
	Writer.Write(MeshCount);
	Writer.Write(InstCount);
	Writer.Write(OffsetInstances);//keep header end aligned to 16byte

	tdMeshOffset MeshOffsets;
	MeshOffsets.reserve(MeshCount);
	size_t Offset = 16;
	//std::map<void*,size_t> MeshO
	for (size_t a = 0; a < MeshCount; a++)
	{
		SOCMeshInfo MeshInfo;
		MeshInfo.m_MeshHash = pExportData->GetExportObject(a)->MeshHash();
		const tdMeshOffset::iterator it = std::find(MeshOffsets.begin(), MeshOffsets.end(), MeshInfo);
		if (it != MeshOffsets.end())
			MeshInfo = *it;
		else
		{
			MeshInfo.m_Offset = Offset;
			Offset += SaveMesh(Writer, pExportData->GetExportObject(a), MeshInfo.m_OBBMat);
		}
		MeshOffsets.push_back(MeshInfo);
	}
	OffsetInstances = Offset;
	for (size_t a = 0; a < InstCount; a++)
		SaveInstance(Writer, pExportData->GetExportObject(a), MeshOffsets[a]);
	Writer.Seek(4);
	Writer.Write(static_cast<uint32>(MeshOffsets.size()));
	Writer.Seek(12);
	Writer.Write(OffsetInstances);

	fwrite(&Writer.Data()[0], 1, Writer.Data().size(), pFile);

	fclose(pFile);
	return true;
}

const char* COCMExporter::TrimFloat(float fValue) const
{
	// Convert a float into a string representation and remove all
	// uneccessary zeroes and the decimal dot if it is not needed
	const int nMaxAccessInTime = 4;

	static char ppBufs[nMaxAccessInTime][16];
	static int nCurBuf = 0;

	if (nCurBuf >= nMaxAccessInTime)
		nCurBuf = 0;

	char* pBuf = ppBufs[nCurBuf++];
	sprintf(pBuf, "%f", fValue);

	for (int i = strlen(pBuf) - 1; i > 0; --i)
	{
		if (pBuf[i] == '0')
			pBuf[i] = 0;
		else if (pBuf[i] == '.')
		{
			pBuf[i] = 0;
			break;
		}
		else
			break;
	}

	return pBuf;
}

void COCMExporter::Release()
{
	delete this;
}

