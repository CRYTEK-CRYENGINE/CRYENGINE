#pragma once

#include "Cry_Geo.h"

struct CGeomExtent {
	bool operator!() { return false; }
	static void ReserveParts(int) {}
	static void AddPart(float) {}
	static int NumParts() { return 0; }
	static int RandomPart(CRndGen&) { return 0; }
	static float TotalExtent() { return 0; }
	struct PartSum { 
		Array<PosNorm> aPoints;
		int iPart; 
	};
	static Array<PartSum> RandomPartsAliasSum(Array<PosNorm> aPoints, CRndGen& seed) { return Array<PartSum>(); }
};
struct CGeomExtents : CGeomExtent {
	CGeomExtent& Make(EGeomForm) { return *this; }
	CGeomExtent& operator[](int) { return *this; }
	static void Clear() {}
	void *dummy;
};
inline float BoxExtent(EGeomForm,Vec3 const&) { return 0; }
inline float SphereExtent(EGeomForm,float) { return 0; }
inline float CircleExtent(EGeomForm,float) { return 0; }
inline float ScaleExtent(EGeomForm,float) { return 0; }
inline Vec2 CircleRandomPoint(CRndGen&,EGeomForm,float) { return Vec2(0); }
inline void BoxRandomPoints(Array<PosNorm>,CRndGen&,EGeomForm,const Vec3&) {}
inline void SphereRandomPoints(Array<PosNorm>,CRndGen&,EGeomForm,float) {}
inline int TriMeshPartCount(EGeomForm,int) { return 0; }
inline int TriIndices(int[3],int,EGeomForm) { return 0; }
inline float TriExtent(EGeomForm, Vec3 const [3], Vec3 const&) { return 0; }
inline void TriRandomPos(PosNorm&,CRndGen&,EGeomForm,PosNorm const[3], Vec3 const&, bool)	{}