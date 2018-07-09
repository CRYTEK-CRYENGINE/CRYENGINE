// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "SceneElementCommon.h"

namespace FbxTool
{

struct SNode;
class CScene;

} // namespace FbxTool

class CSceneElementSourceNode : public CSceneElementCommon
{
public:
	CSceneElementSourceNode(CScene* pScene, int id);
	virtual ~CSceneElementSourceNode() {}

	const FbxTool::SNode*  GetNode() const;

	void SetSceneAndNode(FbxTool::CScene* pFbxScene, const FbxTool::SNode* pNode);

	void Serialize(Serialization::IArchive& ar);

	virtual ESceneElementType GetType() const override;
private:
	const FbxTool::SNode* m_pNode;
	FbxTool::CScene* m_pFbxScene;
};

