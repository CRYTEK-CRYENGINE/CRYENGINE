#pragma once

#include <QWidget>
#include <Cry3DEngine/IIndexedMesh.h>
#include "QViewport.h"

class QViewport;
class CMaterial;
class CDisplayViewportAdapter;

class CCMeshBakerPopupPreview// : public QWidget
{
public:
	CCMeshBakerPopupPreview(QViewport * viewPort);
	virtual ~CCMeshBakerPopupPreview(){}

	void ReleaseObject();
	void SetObject(IStatObj * pObj);
	void SetMaterial(IMaterial* pMat);
	void SetRotate(bool rotate);
	void SetWireframe(bool wireframe);
	void SetGrid(bool grid);
	void Reset();


	void OnRender(const SRenderContext& rc);
	void OnKeyEvent(const SKeyEvent& sKeyEvent);
	void OnMouseEvent(const SMouseEvent& sMouseEvent);

	void EnableMaterialPrecaching(bool bPrecacheMaterial) { m_bPrecacheMaterial=bPrecacheMaterial; }
	void EnableWireframeRendering(bool bDrawWireframe) { m_bDrawWireFrame=bDrawWireframe; }

private:
	void RenderUnwrappedMesh(DisplayContext& dc,const SRenderContext& rc);
	void RenderUnwrappedLodPolygon(DisplayContext& dc, IIndexedMesh* pIIndexedMesh,const SRenderContext& rc);
	void RenderUnwrappedLodPolygonUV(DisplayContext& dc, IIndexedMesh* pIIndexedMesh);

	bool NoMaterialRender(const SRenderContext& rc);
	bool MaterialRender(const SRenderContext& rc);
	void RenderObject( IMaterial* pMaterial, SRenderingPassInfo& passInfo );

private:
	QString m_modelPath;

	bool m_Rotate;
	IStatObj* m_pObject;
	bool m_Grid;
	CMaterial* m_pMat;
	QViewport * m_ViewPort;
	std::shared_ptr<CDisplayViewportAdapter> m_pViewportAdapter;

	bool m_bPrecacheMaterial;
	bool m_bDrawWireFrame;
	bool m_bShowObject;
	bool m_bRotate;

	float m_rotateAngle;

	ColorF m_ambientColor;
	ColorF m_clearColor;
	f32 m_ambientMultiplier;

	std::vector<CDLight> m_lights;
};
