// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

namespace Designer
{
class DesignerObject;
class Converter
{
public:

	bool        CreateNewDesignerObject();
	bool        ConvertToDesignerObject();

	static bool ConvertSolidXMLToDesignerObject(XmlNodeRef pSolidNode, DesignerObject* pDesignerObject);
	static bool ConvertMeshToBrushDesigner(IIndexedMesh* pMesh, Model* pDesignerObject);

private:

	struct SolidPolygon
	{
		std::vector<uint16> vIndexList;
		int                 matID;
		TexInfo             texinfo;
	};

	static void LoadTexInfo(TexInfo* texinfo, const XmlNodeRef& node);
	static void LoadPolygon(SolidPolygon* polygon, const XmlNodeRef& polygonNode);
	static void LoadVertexList(std::vector<BrushVec3>& vertexlist, const XmlNodeRef& node);

	static void AddPolygonsToDesigner(const std::vector<SolidPolygon>& polygonList, const std::vector<BrushVec3>& vList, DesignerObject* pDesignerObject);

private:

	struct SelectedMesh
	{
		SelectedMesh()
		{
			m_pIndexedMesh = NULL;
			m_bLoadedIndexedMeshFromFile = false;
			m_pMaterial = NULL;
		}
		Matrix34                m_worldTM;
		CMaterial*              m_pMaterial;
		IIndexedMesh*           m_pIndexedMesh;
		bool                    m_bLoadedIndexedMeshFromFile;
		_smart_ptr<CBaseObject> m_pOriginalObject;
	};

	void            GetSelectedObjects(std::vector<SelectedMesh>& pObjects) const;

	DesignerObject* ConvertToDesignerObject(IIndexedMesh* pMesh);
	bool            ConvertMeshToDesignerObject(DesignerObject* pDesignerObject, IIndexedMesh* pMesh);

	void            ConvertToDesignerObjects(std::vector<SelectedMesh>& pSelectedMeshes, std::vector<DesignerObject*>& pOutDesignerObjects);

	static void     GenerateUVIslands(Model* pModel);
};
}

