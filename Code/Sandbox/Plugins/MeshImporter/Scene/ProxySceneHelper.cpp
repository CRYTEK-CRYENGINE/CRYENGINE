// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ProxySceneHelper.h"
#include "SceneElementSourceNode.h"
#include "SceneElementPhysProxies.h"
#include "SceneElementProxyGeom.h"
#include "SceneElementTypes.h"
#include "Scene.h"
#include "ProxyGenerator/ProxyData.h"
#include "ProxyGenerator/ProxyGenerator.h"
#include "SceneModelCommon.h"
#include "SceneView.h"

#include <QMenu>

CSceneElementPhysProxies* AddPhysProxies(CSceneElementSourceNode* pSourceNodeElement, CProxyGenerator* pProxyGenerator)
{
	const Matrix34d mat = pSourceNodeElement->GetNode()->pScene->GetRootNode()->postTransform;
	SPhysProxies* const pPhysProxies = pProxyGenerator->AddPhysProxies(pSourceNodeElement->GetNode(), mat);

	CSceneElementPhysProxies* const pPhysProxiesElement = pSourceNodeElement->GetScene()->NewElement<CSceneElementPhysProxies>();
	pPhysProxiesElement->SetPhysProxies(pPhysProxies);

	pSourceNodeElement->AddChild(pPhysProxiesElement);

	return pPhysProxiesElement;
}

void AddProxyGeometries(CSceneElementPhysProxies* pPhysProxiesElement, CProxyGenerator* pProxyGenerator)
{
	std::vector<phys_geometry*> newPhysGeoms;
	pProxyGenerator->GenerateProxies(pPhysProxiesElement->GetPhysProxies(), newPhysGeoms);
}

void AddProxyGenerationContextMenu(QMenu* pMenu, CSceneModelCommon* pSceneModel, const QModelIndex& index, CProxyGenerator* pProxyGenerator)
{
	CSceneElementCommon* const pSceneElement = pSceneModel->GetSceneElementFromModelIndex(index);

	if (pSceneElement->GetType() == ESceneElementType::SourceNode)
	{
		CSceneElementSourceNode* const pSourceNodeElement = (CSceneElementSourceNode*)pSceneElement;
		const uint64 proxies = pProxyGenerator->GetProxyData()->HasPhysProxies(pSourceNodeElement->GetNode());

		QAction* const pAct = pMenu->addAction(QObject::tr(proxies ? "Add Remaining Auto Proxies" : "Add Auto Proxies"));
		QObject::connect(pAct, &QAction::triggered, [=]()
		{
			if (!index.isValid())
			{
				return;
			}

			CSceneElementCommon* const pSceneElement = pSceneModel->GetSceneElementFromModelIndex(index);
			CRY_ASSERT(pSceneElement->GetType() == ESceneElementType::SourceNode);
			CSceneElementPhysProxies* const pPhysProxiesElement = AddPhysProxies((CSceneElementSourceNode*)pSceneElement, pProxyGenerator);
			pPhysProxiesElement->GetPhysProxies()->params.islandMap = ~proxies;
		});
	}
	else if (pSceneElement->GetType() == ESceneElementType::PhysProxy || pSceneElement->GetType() == ESceneElementType::ProxyGeom)
	{
		QAction* const pAct = pMenu->addAction(QObject::tr("Remove"));
		QObject::connect(pAct, &QAction::triggered, [=]()
		{
			if (!index.isValid())
			{
				return;
			}

			CSceneElementCommon* const pSceneElement = pSceneModel->GetSceneElementFromModelIndex(index);
			if (pSceneElement->GetType() == ESceneElementType::PhysProxy)
			{
				CSceneElementPhysProxies* const pPhysProxiesElement = (CSceneElementPhysProxies*)pSceneElement;
				pProxyGenerator->GetProxyData()->RemovePhysProxies(pPhysProxiesElement->GetPhysProxies());
			}
			else
			{
				CSceneElementProxyGeom* const pProxyGeomElement = (CSceneElementProxyGeom*)pSceneElement;
				pProxyGenerator->GetProxyData()->RemovePhysGeometry(pProxyGeomElement->GetPhysGeom());
			}
		});
		if (pSceneElement->GetType() == ESceneElementType::ProxyGeom)
		{
			CSceneElementProxyGeom* const pProxyGeomElement = (CSceneElementProxyGeom*)pSceneElement;
			phys_geometry* const pPhysGeom = pProxyGeomElement->GetPhysGeom();
			if (pPhysGeom->pGeom->GetType() == GEOM_TRIMESH)
			{
				CRY_ASSERT(pSceneElement->GetParent() && pSceneElement->GetParent()->GetType() == ESceneElementType::PhysProxy);
				CSceneElementPhysProxies* const pPhysProxiesElement = (CSceneElementPhysProxies*)pSceneElement->GetParent();

				SPhysProxies* const pPhysProxies = pPhysProxiesElement->GetPhysProxies();	

				QAction* const pAct = pMenu->addAction(QObject::tr("Remove and Re-generate"));
				QObject::connect(pAct, &QAction::triggered, [=]()
				{
					std::vector<phys_geometry*> newPhysGeoms;
					pProxyGenerator->ResetAndRegenerate(pPhysProxies, pPhysGeom, newPhysGeoms);
				});
			}
		}
	}
}

CSceneElementPhysProxies* GetSelectedPhysProxiesElement(CSceneElementCommon* pSelectedElement, bool includeGeoms)
{
	if (!pSelectedElement)
	{
		return nullptr;
	}

	if (pSelectedElement->GetType() == ESceneElementType::PhysProxy)
	{
		return (CSceneElementPhysProxies*)pSelectedElement;
	}
	else if (includeGeoms && pSelectedElement->GetParent() && pSelectedElement->GetParent()->GetType() == ESceneElementType::PhysProxy)
	{
		return (CSceneElementPhysProxies*)pSelectedElement->GetParent();
	}

	return nullptr;
}

CSceneElementPhysProxies* FindSceneElementOfPhysProxies(CScene& scene, const SPhysProxies* pPhysProxies)
{	
	for (int i = 0; i < scene.GetElementCount(); ++i)
	{
		CSceneElementCommon* const pElement = scene.GetElementByIndex(i);
		if (pElement->GetType() == ESceneElementType::PhysProxy)
		{
			auto pPhysProxiesElement = (CSceneElementPhysProxies*)pElement;
			if (pPhysProxiesElement->GetPhysProxies() == pPhysProxies)
			{
				return pPhysProxiesElement;
			}
		}
	}

	return nullptr;
}

CSceneElementProxyGeom* FindSceneElementOfProxyGeom(CScene& scene, const phys_geometry* pProxyGeom)
{	
	for (int i = 0; i < scene.GetElementCount(); ++i)
	{
		CSceneElementCommon* const pElement = scene.GetElementByIndex(i);
		if (pElement->GetType() == ESceneElementType::ProxyGeom)
		{
			auto pProxyGeomElement = (CSceneElementProxyGeom*)pElement;
			if (pProxyGeomElement->GetPhysGeom() == pProxyGeom)
			{
				return pProxyGeomElement;
			}
		}
	}

	return nullptr;
}

