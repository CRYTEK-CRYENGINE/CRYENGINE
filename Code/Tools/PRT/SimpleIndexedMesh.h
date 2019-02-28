/*
	simple indexed mesh declarations
	mesh interface for callers
*/

#if defined(OFFLINE_COMPUTATION)

#pragma once

#include "PRTTypes.h"
#include "SHFrameworkBasis.h"
#include "TransferParameters.h"
#include "ISHMaterial.h"
#include "ObjFace.h"
#include "SHCompressionInfo.h"
#include "SHRotate.h"

namespace NSH
{
	namespace NTransfer
	{
		struct STransferParameters;
	}
}

namespace NSH
{
	namespace NMaterial
	{
		interface ISHMaterial;
	}
}

/************************************************************************************************************************************************/

struct SCoefficientExportPolicy
{
	NSH::TCompressionInfo	materialCompressionInfo; //!< compression infos for direct coefficient stream
	uint8 bytesPerComponent;							//!< bytes per input component(1: each coefficient as colour, 2: 16 bit format, 4: float
	uint8	coefficientsPerSet;							//!< coefficients per set(usually 8 or 9)
	uint8	compressionBytesPerComponent;		//!< bytes per compressed component (1: each coefficient as colour, 2: 16 bit format, 4: float
	bool  swizzled;												//!< if true, then at least some of the coefficients are preswizzled(for shadowing: the 2x4 component vectors are pre-prepared)
	bool  preMultiplied;									//!< if true then the scalar multiplications for the per pixel bump matrix multiplication are already incorporated
	bool  compressed;											//!< if true then the data are compressed from floats to bytes
	SCoefficientExportPolicy();
};

/************************************************************************************************************************************************/

struct CObjCoor
{
	INSTALL_CLASS_NEW(CObjCoor)

	float s,t;
	
	bool operator == (CObjCoor& rOther);	//!< compare operator
	CObjCoor(const float cS, const float cT);
	CObjCoor();
};

/************************************************************************************************************************************************/

#pragma warning (disable : 4291)
//!< covers additional data for the sh materials
struct SAddMaterialProperty
{
	INSTALL_CLASS_NEW(SAddMaterialProperty)

	NSH::CSmartPtr<NSH::NMaterial::ISHMaterial, CSHAllocator<> > pSHMaterial;	//!< sh material
	bool considerForRayCasting;	//!< true if to consider for ray casting, decals should be false here
	bool computeSHCoefficients;	//!< true if sh coefficients are to be computed for this material
	bool is2Sided;							//!< true if this material is supposed to be 2 sided (important for ray caster)
	SAddMaterialProperty
	(
		NSH::CSmartPtr<NSH::NMaterial::ISHMaterial, CSHAllocator<> >& rSHMaterial,
		const bool cConsiderForRayCasting = true,
		const bool cComputeSHCoefficients = true,
		const bool cIs2Sided							= false
	);

	SAddMaterialProperty();
};
#pragma warning (default : 4291)

//!< single indexed mesh with world space values
//!< dont forget to set the mesh names(without .obj)
//!< public members since it acts fully as interface mesh class between this library and all callers
class CSimpleIndexedMesh
{
public:
	INSTALL_CLASS_NEW(CSimpleIndexedMesh)

	typedef NSH::CRotateMatrix_tpl<float, false> TSHRotationMatrix;

/************************************************************************************************************************************************/

	void RetrieveFacedata(const uint32 cFaceIndex, Vec3& rV0, Vec3& rV1, Vec3& rV2) const;
	void RetrieveVertexDataBary(const CObjFace& rFace, const Vec3& rBary, Vec3& rV) const;
	void Log() const;
	void LogName() const;
	const bool HasTransparentMaterials();		//!< retrieves whether this object has transparent materials for the ray caster
	const bool ConsiderForRayCastingByFaceID(const uint32 cFaceID) const;	//!< retrieves for a certain material whether to consider for ray casting or not
	const bool Has2SidedMatByFaceID(const uint32 cFaceID) const;	//!< retrieves for a certain material whether is it 2 sided or not
	const bool ComputeSHCoeffs(const uint32 cFaceID) const;	//!< retrieves for a certain material whether to calc sh coeffs for 
	const NSH::NMaterial::ISHMaterial& GetMaterialByFaceID(const uint32 cFaceID) const;	//!< retrieves for a certain material from the face id
	const uint32 GetMaterialIndexByFaceID(const uint32 cFaceID) const;		//!< retrieves the material index from a face id
	CSimpleIndexedMesh();														//!< constructor
	void FreeData();																//!< frees the data
	~CSimpleIndexedMesh();													//!< destructor
	const Matrix33& GetWSOSRotation()const;					//!< retrieves the world space -> object space rotation matrix
	const bool HasIdentityWSOSRotation()const;			//!< returns true if the rotation matrix is the identity matrix (therefore is matrix private)
	void SetWSOSRotation(const Matrix33& crMatrix);	//!< sets the world space -> object space rotation matrix
	const TSHRotationMatrix& GetSHWSOSRotation()const;//!< retrieves the world space -> object space sh coefficient rotation matrix(stored for performance reason)

	//week accessor functions
	const char* GetMeshName()const;
	void SetMeshName(const char* cpName);
	
	SCoefficientExportPolicy& GetExportPolicy();
	const SCoefficientExportPolicy& GetExportPolicy()const;

	void AddMaterial(const SAddMaterialProperty& crMatToAdd);
	const uint32 MaterialCount()const;
	const bool ComputeSHForMatFromIndex(const uint32 cIndex);

	const CObjFace& GetObjFace(const uint32 cIndex) const;
	CObjFace& GetObjFace(const uint32 cIndex);
	void AllocateFaces(const uint32 cCount);
	const uint32 GetFaceCount()const;

	const Vec3& GetVertex(const uint32 cIndex) const;
	Vec3& GetVertex(const uint32 cIndex);
	void AllocateVertices(const uint32 cCount);
	const uint32 GetVertexCount()const;

	const Vec3 GetMinExt()const;
	void SetMinExt(const Vec3& crMinExt);
	const Vec3 GetMaxExt()const;
	void SetMaxExt(const Vec3& crMaxExt);

	const CObjCoor& GetTexCoord(const uint32 cIndex) const;
	CObjCoor& GetTexCoord(const uint32 cIndex);
	void AllocateTexCoords(const uint32 cCount);
	const uint32 GetTexCoordCount()const;

	void AllocateNormals(const uint32 cCount);
	const uint32 GetNormalCount()const;
	const Vec3& GetWSNormal(const uint32 cIndex) const;
	Vec3& GetWSNormal(const uint32 cIndex);
	const Vec3& GetNormal(const uint32 cIndex) const;
	Vec3& GetNormal(const uint32 cIndex);
	const Vec3& GetBiNormal(const uint32 cIndex) const;
	Vec3& GetBiNormal(const uint32 cIndex);
	const Vec3& GetTNormal(const uint32 cIndex) const;
	Vec3& GetTNormal(const uint32 cIndex);

protected:
	//lame implementation of mesh file name
	char m_MeshFilename[255];		//!< file name
	
	static const uint32 scMaxMats = 32;
	SAddMaterialProperty m_Materials[scMaxMats];//!< sh materials
	uint32 m_MaterialCount;//!< corresponding mat count

	SCoefficientExportPolicy m_ExportPolicy;//describes how to export the coefficients
	//!< geometry data
	CObjFace*				m_pFaces;					//!< face (triangle) indices to m_pVerts,m_pCoors,m_pNorms, smoothing group information, shaderID
	Vec3*						m_pVerts;					//!< vertex position
	CObjCoor*				m_pCoors;					//!< uv coordinates
	Vec3*						m_pWSNorms;				//!< world space normals
	Vec3*						m_pNorms;					//!< object space normals
	Vec3*						m_pBiNorms;				//!< object space binormals
	Vec3*						m_pTangentNorms;	//!< object space t-normals
	Vec3						m_Min;						//!< world space object minimum
	Vec3						m_Max;						//!< world space object maximum
	int							m_FaceCount;			//!< face (triangle) indices to m_pVerts,m_pCoors,m_pNorms, smoothing group information, shaderID
	int							m_VertCount;			//!< vertex position
	int							m_CoorCount;			//!< uv coordinates
	int							m_NormCount;			//!< normals

	TSHRotationMatrix m_WorldToObjectSpaceSHMatrix;	//!< world->object space matrix for post coefficient processing(coeffs must be in object space)
	Matrix33	m_WorldToObjectSpaceMatrix;	//!< world->object space matrix (coeffs must be in object space)

	bool			m_HasIdentityRotation;			//!< true if rotation matrix is the identity matrix (for optimization)
};

#include "SimpleIndexedMesh.inl"

#endif
