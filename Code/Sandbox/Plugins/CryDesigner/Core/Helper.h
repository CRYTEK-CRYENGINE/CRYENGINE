// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Core/Model.h"

namespace Designer
{
class Model;
class ModelCompiler;
class DesignerObject;

bool CanDesignerEditObject(CBaseObject* pObj);
bool UpdateStatObjWithoutBackFaces(CBaseObject* pObj);
bool UpdateStatObj(CBaseObject* pObj);
bool UpdateGameResource(CBaseObject* pObj);
bool GetIStatObj(CBaseObject* pObj, _smart_ptr<IStatObj>* pOutStatObj);
bool GenerateGameFilename(CBaseObject* pObj, string& outFileName);
bool GetRenderFlag(CBaseObject* pObj, int& outRenderFlag);
bool GetCompiler(CBaseObject* pObj, ModelCompiler*& pCompiler);
bool GetModel(CBaseObject* pObj, Model*& pModel);
bool HitTest(const BrushRay& ray, MainContext& mc, HitContext& hc);
void RemovePolygonWithSpecificFlagsFromList(std::vector<PolygonPtr>& polygonList, int nFlags);
void RemovePolygonWithoutSpecificFlagsFromList(std::vector<PolygonPtr>& polygonList, int nFlags);
void SwitchToDesignerToolForObject(CBaseObject* pObj);

namespace MirrorUtil
{
void AddMirroredPolygon(Model* pModel, PolygonPtr pPolygon);
void RemoveMirroredPolygon(Model* pModel, PolygonPtr pPolygon, bool bLeaveFrame = false);
void UpdateMirroredPartWithPlane(Model* pModel, const BrushPlane& plane);
void EraseMirroredEdge(Model* pModel, const BrushEdge3D& edge);
void RemoveMirroredPolygons(std::vector<PolygonPtr>& polygonList);
void RemoveNonMirroredPolygons(std::vector<PolygonPtr>& polygonList);
}

bool                         MakeListConsistingOfArc(const BrushVec2& vOutsideVertex, const BrushVec2& vBaseVertex0, const BrushVec2& vBaseVertex1, int nSegmentCount, std::vector<BrushVec2>& outVertexList);
template<class T>
bool                         ComputeCircumradiusAndCircumcenter(const T& v0, const T& v1, const T& v2, BrushFloat* outCircumradius, T* outCircumcenter);
BrushVec3                    GetWorldBottomCenter(CBaseObject* pObject);
BrushVec3                    GetOffsetToAnother(CBaseObject* pObject, const BrushVec3& vTargetPos);
void                         PivotToCenter(MainContext& mc);
void                         PivotToPos(CBaseObject* pObject, Model* pModel, const BrushVec3& vPivot);
bool                         DoesEdgeIntersectRect(const CRect& rect, CViewport* pView, const Matrix34& worldTM, const BrushEdge3D& edge);
bool                         DoesSelectionBoxIntersectRect(CViewport* view, const Matrix34& worldTM, const CRect& rect, PolygonPtr pPolygon);
PolygonPtr                   MakePolygonFromRectangle(const CRect& rectangle);
void                         CopyPolygons(std::vector<PolygonPtr>& sourcePolygons, std::vector<PolygonPtr>& destPolygons);
PolygonPtr                   Convert2ViewPolygon(IDisplayViewport* pView, const BrushMatrix34& worldTM, PolygonPtr pPolygon, PolygonPtr* pOutPolygon);
bool                         IsFrameLeftInRemovingPolygon(CBaseObject* pObject);
bool                         BinarySearchForScale(BrushFloat fValidScale, BrushFloat fInvalidScale, int nCount, Polygon& polygon, BrushFloat& fOutScale);
BrushFloat                   ApplyScaleCheckingBoundary(PolygonPtr pPolygon, BrushFloat fScale);
void                         ResetXForm(MainContext& mc, int nResetFlag = eResetXForm_All);
void                         AddPolygonWithSubMatID(PolygonPtr pPolygon, bool bUnion = true);
void                         CreateMeshFacesFromPolygon(PolygonPtr pPolygon, FlexibleMesh& mesh, bool bCreateBackFaces);
void                         TransformUVs(const TexInfo& texInfo, FlexibleMesh& mesh);
void                         AssignMatIDToSelectedPolygons(int matID);
void                         AddPolygonToListCheckingDuplication(std::vector<PolygonPtr>& polygonList, PolygonPtr polygon);
std::vector<DesignerObject*> GetSelectedDesignerObjects();
bool                         IsDesignerRelated(CBaseObject* pObject);
bool                         CheckIfHasValidModel(CBaseObject* pObject);
void                         RemoveAllEmptyDesignerObjects();
void                         MakeRectangle(const BrushPlane& plane, const BrushVec2& startPos, const BrushVec2& endPos, std::vector<BrushVec3>& outVertices);
void                         ConvertMeshFacesToFaces(const std::vector<Vertex>& vertices, std::vector<SMeshFace>& meshFaces, std::vector<std::vector<Vec3>>& outFaces);
void                         MergeTwoObjects(const MainContext& mc0, const MainContext& mc1);
int                          AddVertex(std::vector<Vertex>& vertices, const Vertex& newVertex);
};

