// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __INCLUDE_CRY3DENGINE_CLIPVOLUME_H
#define __INCLUDE_CRY3DENGINE_CLIPVOLUME_H

struct IBSPTree3D;

class CClipVolume : public IClipVolume
{
public:
	CClipVolume();

	////////////// IClipVolume implementation //////////////
	virtual void  GetClipVolumeMesh(_smart_ptr<IRenderMesh>& renderMesh, Matrix34& worldTM) const;
	virtual AABB  GetClipVolumeBBox() const;

	virtual uint8 GetStencilRef() const      { return m_nStencilRef; }
	virtual uint  GetClipVolumeFlags() const { return m_nFlags; }
	virtual bool  IsPointInsideClipVolume(const Vec3& vPos) const;
	////////////////////////////

	void SetName(const char* szName);
	void SetStencilRef(int nStencilRef) { m_nStencilRef = nStencilRef; }

	void Update(_smart_ptr<IRenderMesh> pRenderMesh, IBSPTree3D* pBspTree, const Matrix34& worldTM, uint32 flags);

	void GetMemoryUsage(class ICrySizer* pSizer) const;

private:
	uint8                   m_nStencilRef;
	uint32                  m_nFlags;
	Matrix34                m_WorldTM;
	Matrix34                m_InverseWorldTM;
	AABB                    m_BBoxWS;
	AABB                    m_BBoxLS;

	_smart_ptr<IRenderMesh> m_pRenderMesh;
	IBSPTree3D*             m_pBspTree;

	char                    m_sName[64];
};

#endif //__INCLUDE_CRY3DENGINE_CLIPVOLUME_H
