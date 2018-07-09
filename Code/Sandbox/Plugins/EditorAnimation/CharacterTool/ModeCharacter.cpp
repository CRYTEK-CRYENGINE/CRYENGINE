// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#include "ModeCharacter.h"
#include "ManipScene.h"
#include <Serialization/QPropertyTree/QPropertyTree.h>
#include <QToolBar>
#include <QAction>
#include "Expected.h"
#include "DisplayParameters.h"
#include "CharacterDocument.h"
#include "CharacterDefinition.h"
#include "CharacterToolForm.h"
#include "CharacterList.h"
#include "SceneContent.h"
#include "Serialization.h"
#include "GizmoSink.h"
#include "CharacterGizmoManager.h"
#include "CharacterToolSystem.h"
#include "TransformPanel.h"
#include "PropertiesPanel.h"
#include "QViewport.h"
#include <CryIcon.h>

namespace CharacterTool
{

ModeCharacter::ModeCharacter()
	: m_scene(new Manip::CScene())
	, m_ignoreSceneSelectionChange(false)
	, m_transformPanel()
	, m_posMouse(ZERO)
	, m_isCurBoneFree(true)
{
	EXPECTED(connect(m_scene.get(), SIGNAL(SignalUndo()), this, SLOT(OnSceneUndo())));
	EXPECTED(connect(m_scene.get(), SIGNAL(SignalRedo()), this, SLOT(OnSceneRedo())));
	EXPECTED(connect(m_scene.get(), SIGNAL(SignalElementsChanged(unsigned int)), this, SLOT(OnSceneElementsChanged(unsigned int))));
	EXPECTED(connect(m_scene.get(), SIGNAL(SignalElementContinuousChange(unsigned int)), this, SLOT(OnSceneElementContinuousChange(unsigned int))));
	EXPECTED(connect(m_scene.get(), SIGNAL(SignalSelectionChanged()), this, SLOT(OnSceneSelectionChanged())));
	EXPECTED(connect(m_scene.get(), SIGNAL(SignalPropertiesChanged()), this, SLOT(OnScenePropertiesChanged())));
	EXPECTED(connect(m_scene.get(), SIGNAL(SignalManipulationModeChanged()), this, SLOT(OnSceneManipulationModeChanged())));
}

void ModeCharacter::Serialize(Serialization::IArchive& ar)
{
	m_scene->Serialize(ar);
}

void ModeCharacter::EnterMode(const SModeContext& context)
{
	m_character = context.character;
	m_document = context.document;
	m_system = context.system;
	m_transformPanel = context.transformPanel;
	m_layerPropertyTrees = context.layerPropertyTrees;
	m_scene->SetSpaceProvider(m_system->characterSpaceProvider.get());
	m_system->gizmoSink->SetScene(m_scene.get());
	m_system->characterGizmoManager->ReadGizmos();
	m_window = context.window;

	EXPECTED(connect(m_document, SIGNAL(SignalBindPoseModeChanged()), SLOT(OnBindPoseModeChanged())));
	EXPECTED(connect(m_document, SIGNAL(SignalDisplayOptionsChanged(const DisplayOptions &)), SLOT(OnDisplayOptionsChanged())));
	EXPECTED(connect(m_system->characterGizmoManager.get(), SIGNAL(SignalSubselectionChanged(int)), SLOT(OnSubselectionChanged(int))));
	EXPECTED(connect(m_system->characterGizmoManager.get(), SIGNAL(SignalGizmoChanged()), SLOT(OnGizmoChanged())));

	EXPECTED(connect(m_transformPanel, SIGNAL(SignalChanged()), SLOT(OnTransformPanelChanged())));
	EXPECTED(connect(m_transformPanel, SIGNAL(SignalChangeFinished()), SLOT(OnTransformPanelChangeFinished())));
	EXPECTED(connect(m_transformPanel, SIGNAL(SignalSpaceChanged()), SLOT(OnTransformPanelSpaceChanged())));
	m_transformPanel->SetSpace(m_scene->TransformationSpace());
	WriteTransformPanel();

	QToolBar* toolBar = context.toolbar;
	m_actionMoveTool.reset(new QAction(CryIcon("icons:Navigation/Basics_Move.ico"), "Move", 0));
	m_actionMoveTool->setPriority(QAction::LowPriority);
	m_actionMoveTool->setCheckable(true);
	EXPECTED(connect(m_actionMoveTool.get(), SIGNAL(triggered()), this, SLOT(OnActionMoveTool())));
	toolBar->addAction(m_actionMoveTool.get());

	m_actionRotateTool.reset(new QAction(CryIcon("icons:Navigation/Basics_Rotate.ico"), "Rotate", 0));
	m_actionRotateTool->setPriority(QAction::LowPriority);
	m_actionRotateTool->setCheckable(true);
	EXPECTED(connect(m_actionRotateTool.get(), SIGNAL(triggered()), this, SLOT(OnActionRotateTool())));
	toolBar->addAction(m_actionRotateTool.get());

	m_actionScaleTool.reset(new QAction(CryIcon("icons:Navigation/Basics_Scale.ico"), "Scale", 0));
	EXPECTED(connect(m_actionScaleTool.get(), SIGNAL(triggered()), this, SLOT(OnActionScaleTool())));
	m_actionScaleTool->setPriority(QAction::LowPriority);
	m_actionScaleTool->setCheckable(true);
	toolBar->addAction(m_actionScaleTool.get());

	OnSceneManipulationModeChanged();

	UpdateToolbar();
}

void ModeCharacter::LeaveMode()
{
	EXPECTED(disconnect(m_transformPanel, SIGNAL(SignalChanged()), this, SLOT(OnTransformPanelChanged())));
	EXPECTED(disconnect(m_transformPanel, SIGNAL(SignalChangeFinished()), this, SLOT(OnTransformPanelChangeFinished())));
	EXPECTED(disconnect(m_transformPanel, SIGNAL(SignalSpaceChanged()), this, SLOT(OnTransformPanelSpaceChanged())));

	EXPECTED(disconnect(m_document, SIGNAL(SignalBindPoseModeChanged()), this, SLOT(OnBindPoseModeChanged())));
	EXPECTED(disconnect(m_system->characterGizmoManager.get(), SIGNAL(SignalSubselectionChanged(int)), this, SLOT(OnSubselectionChanged(int))));
	EXPECTED(disconnect(m_system->characterGizmoManager.get(), SIGNAL(SignalGizmoChanged()), this, SLOT(OnGizmoChanged())));
	m_system->gizmoSink->SetScene(0);
	m_document = 0;
	m_scene->SetSpaceProvider(0);
	m_layerPropertyTrees.clear();
}

void ModeCharacter::OnTransformPanelChanged()
{
	if (m_scene->TransformationSpace() != Manip::SPACE_LOCAL)
		m_scene->SetSelectionTransform(m_scene->TransformationSpace(), m_transformPanel->Transform());
	OnSceneElementContinuousChange((1 << GIZMO_LAYER_COUNT) - 1);
}

void ModeCharacter::OnTransformPanelSpaceChanged()
{
	m_scene->SetTransformationSpace(m_transformPanel->Space());

	WriteTransformPanel();
}

void ModeCharacter::OnTransformPanelChangeFinished()
{
	m_scene->SetSelectionTransform(m_scene->TransformationSpace(), m_transformPanel->Transform());
	OnSceneElementsChanged((1 << GIZMO_LAYER_COUNT) - 1);
}

void ModeCharacter::OnSubselectionChanged(int layer)
{
	const vector<const void*>& items = m_system->characterGizmoManager->Subselection(GizmoLayer(layer));

	CharacterDefinition* cdf = m_document->GetLoadedCharacterDefinition();
	if (!cdf)
		return;

	Manip::SSelectionSet selection;
	const Manip::SElements& elements = m_scene->Elements();
	for (size_t i = 0; i < elements.size(); ++i)
	{
		bool selected = std::find(items.begin(), items.end(), elements[i].originalHandle) != items.end();
		if (selected)
			selection.Add(elements[i].id);
	}

	m_ignoreSceneSelectionChange = true;
	m_scene->SetSelection(selection);
	m_ignoreSceneSelectionChange = false;
}

void ModeCharacter::OnGizmoChanged()
{
	WriteTransformPanel();
}

void ModeCharacter::OnBindPoseModeChanged()
{
	UpdateToolbar();
}

void ModeCharacter::OnDisplayOptionsChanged()
{
	DisplayOptions* displayOptions = m_system->document->GetDisplayOptions().get();
	m_scene->SetShowAttachementGizmos(displayOptions->attachmentGizmos);
	m_scene->SetShowPoseModifierGizmos(displayOptions->poseModifierGizmos);

	constexpr int animationFlag = (1 << GIZMO_LAYER_ANIMATION);

	if (m_system->document->GetDisplayOptions()->animation.animationEventGizmos)
		m_scene->SetVisibleLayerMask(0xffffffff);
	else
		m_scene->SetVisibleLayerMask(0xffffffff & ~animationFlag);
}

void ModeCharacter::OnSceneSelectionChanged()
{
	if (m_ignoreSceneSelectionChange)
		return;

	if (ExplorerEntry* entry = m_system->gizmoSink->ActiveEntry())
	{
		if (!m_system->document->IsExplorerEntrySelected(entry))
		{
			ExplorerEntries entries;
			entries.push_back(entry);
			m_document->SetSelectedExplorerEntries(entries, CharacterDocument::SELECT_DO_NOT_REWIND);
		}
	}

	vector<const void*> selection;
	GetPropertySelection(&selection);

	for (int layer = 0; layer < GIZMO_LAYER_COUNT; ++layer)
		m_system->characterGizmoManager->SetSubselection((GizmoLayer)layer, selection);

	WriteTransformPanel();
}

void ModeCharacter::WriteTransformPanel()
{
	if (!m_transformPanel)
		return;

	m_transformPanel->SetMode(m_scene->TransformationMode());
	m_transformPanel->SetTransform(m_scene->GetSelectionTransform(m_scene->TransformationSpace()));
	bool enabled = false;
	if (m_scene->TransformationMode() == Manip::MODE_TRANSLATE && m_scene->SelectionCanBeMoved())
		enabled = true;
	if (m_scene->TransformationMode() == Manip::MODE_ROTATE && m_scene->SelectionCanBeRotated())
		enabled = true;
	m_transformPanel->SetEnabled(enabled);
}

void ModeCharacter::OnSceneManipulationModeChanged()
{
	m_actionMoveTool->setChecked(m_scene->TransformationMode() == Manip::MODE_TRANSLATE);
	m_actionRotateTool->setChecked(m_scene->TransformationMode() == Manip::MODE_ROTATE);
	m_actionScaleTool->setChecked(m_scene->TransformationMode() == Manip::MODE_SCALE);

	WriteTransformPanel();
}

static ExplorerEntry* EntryForGizmoLayer(GizmoLayer layer, System* system)
{
	switch (layer)
	{
	case GIZMO_LAYER_CHARACTER:
		return system->document->GetActiveCharacterEntry();
	case GIZMO_LAYER_ANIMATION:
		return system->document->GetActiveAnimationEntry();
	default:
		return 0;
	}
}

void ModeCharacter::HandleSceneChange(int layerBits, bool continuous)
{
	unsigned int bitsLeft = layerBits;
	for (int layer = 0; bitsLeft != 0; bitsLeft = bitsLeft >> 1, ++layer)
	{
		if (layer >= m_layerPropertyTrees.size())
			break;
		QPropertyTree* tree = m_layerPropertyTrees[layer];
		if (bitsLeft & 1)
		{
			if (tree)
			{
				tree->apply(continuous);
				if (continuous)
					tree->revert();
			}
			if (layer == GIZMO_LAYER_SCENE)
			{
				m_system->scene->SignalChanged(continuous);
			}
			else if (ExplorerEntry* entry = EntryForGizmoLayer((GizmoLayer)layer, m_system))
				m_system->explorerData->CheckIfModified(entry, "Transform", continuous);

		}
	}

	if (continuous && layerBits & (1 << GIZMO_LAYER_CHARACTER))
	{
		if (ExplorerEntry* entry = m_system->document->GetActiveCharacterEntry())
		{
			EntryModifiedEvent ev;
			ev.subtree = entry->subtree;
			ev.id = entry->id;
			ev.continuousChange = true;
			m_document->OnCharacterModified(ev);
		}
	}

	WriteTransformPanel();
}

void ModeCharacter::OnSceneElementsChanged(unsigned int layerBits)
{
	HandleSceneChange(layerBits, false);
}

void ModeCharacter::OnSceneElementContinuousChange(unsigned int layerBits)
{
	HandleSceneChange(layerBits, true);
	;
}

void ModeCharacter::OnSceneUndo()
{
	ExplorerEntries entries;
	m_document->GetEntriesActiveInDocument(&entries);
	if (entries.empty())
		return;
	m_system->explorerData->UndoInOrder(entries);
	WriteTransformPanel();
	m_scene->RefreshAllElements();
}

void ModeCharacter::OnSceneRedo()
{
	ExplorerEntries entries;
	m_document->GetEntriesActiveInDocument(&entries);
	if (entries.empty())
		return;
	m_system->explorerData->RedoInOrder(entries);
	WriteTransformPanel();
	m_scene->RefreshAllElements();
}

void ModeCharacter::OnScenePropertiesChanged()
{
}

void ModeCharacter::GetPropertySelection(vector<const void*>* selection) const
{
	selection->clear();

	if (CharacterDefinition* cdf = m_document->GetLoadedCharacterDefinition())
	{
		Manip::SElements selectedElements;
		m_scene->GetSelectedElements(&selectedElements);
		for (size_t i = 0; i < selectedElements.size(); ++i)
			selection->push_back(selectedElements[i].originalHandle);
	}
}

void ModeCharacter::SetPropertySelection(const vector<const void*>& items)
{
}

void ModeCharacter::OnViewportRender(const SRenderContext& rc)
{
	m_scene->OnViewportRender(rc);

	if (QApplication::queryKeyboardModifiers() != Qt::ShiftModifier)
		m_curBoneName = "";
	IRenderAuxText::Draw2dLabel(m_posMouse.x + 6, m_posMouse.y - 6, 1.4f, ColorF(!m_isCurBoneFree, 1, 0), false, m_curBoneName);

	CharacterDefinition* cdf = m_document->GetLoadedCharacterDefinition();
	if (cdf && m_character)
	{
		bool applyPhys = cdf->m_physNeedsApply;
		if (m_window->ProxyMakingMode() && !cdf->m_physEdit)
		{
			DisplayOptions* opt = m_document->GetDisplayOptions().get();
			int changed = 0;
			if (opt->physics.showPhysicalProxies == DisplayPhysicsOptions::DISABLED)
			{
				opt->physics.showPhysicalProxies = DisplayPhysicsOptions::SOLID;
				++changed;
			}
			if (opt->physics.showRagdollJointLimits == DisplayPhysicsOptions::NONE)
			{
				opt->physics.showRagdollJointLimits = DisplayPhysicsOptions::ALL;
				++changed;
			}
			if (changed)
				m_document->DisplayOptionsChanged();
			cdf->LoadPhysProxiesFromCharacter(m_character);
			m_window->GetPropertiesPanel()->PropertyTree()->revert();
			m_window->GetPropertiesPanel()->OnChanged();
			applyPhys = true;
		}
		if (applyPhys)
		{
			EntryModifiedEvent ev;
			ev.id = m_system->document->GetActiveCharacterEntry()->id;
			m_document->OnCharacterModified(ev);
		}
	}
}

void ModeCharacter::OnViewportKey(const SKeyEvent& ev)
{
	m_scene->OnViewportKey(ev);
}


struct SFakeSizer : public ICrySizer 
{
	virtual void   Release() {}
	virtual size_t GetTotalSize() { return 0; }
	virtual size_t GetObjectCount() { return 0; }
	virtual void   Reset() {}
	virtual void   End() {}
	virtual struct IResourceCollector* GetResourceCollector() { return nullptr; }
	virtual void   SetResourceCollector(IResourceCollector* pColl) {}
	virtual void   Push(const char* szComponentName) {}
	virtual void   PushSubcomponent(const char* szSubcomponentName) {}
	virtual void   Pop() {}
	virtual bool   IsLoading() { return false; }
	virtual bool   AddObject(const void* ptr, size_t size, int nCount=1) { m_ptrLast = ptr; return true; }

	const void* m_ptrLast = nullptr;
};

void ModeCharacter::OnViewportMouse(const SMouseEvent& ev)
{
	if (m_document->GetLoadedCharacterDefinition() && m_character && QApplication::keyboardModifiers() == Qt::ShiftModifier && 
			m_window->ProxyMakingMode() && m_window->GetPropertiesPanel()->PropertyTree() && OnViewportMouseProxy(ev))
		return;

	m_scene->OnViewportMouse(ev);
}

bool ModeCharacter::OnViewportMouseProxy(const SMouseEvent& ev)
{
	CharacterDefinition* cdf = m_document->GetLoadedCharacterDefinition();
	auto tree = m_window->GetPropertiesPanel()->PropertyTree();

	Ray wray;
	ev.viewport->ScreenToWorldRay(&wray, ev.x, ev.y);
	ray_hit rhit;
	float mindist = 1e10f;
	int ibone0 = -1, nvtxTot = 0;
	if (m_character->GetPhysEntity() && gEnv->pPhysicalWorld->RayTraceEntity(m_character->GetPhysEntity(), wray.origin, wray.direction*100, &rhit))
		mindist = rhit.dist;

	auto RayCheckRMesh = [&](IRenderMesh* pRM, IAttachmentSkin* pSkin = nullptr)
	{
		if (!pRM)
			return;
		pRM->LockForThreadAccess();
		SFakeSizer sizer;
		if (pSkin)
			pSkin->GetMemoryUsage(&sizer);
		strided_pointer<Vec3> vtx;
		vtx.data = (Vec3*)pRM->GetPosPtr(vtx.iStride, FSL_READ);
		vtx_idx *idx = pRM->GetIndexPtr(FSL_READ);
		strided_pointer<SVF_W4B_I4S> skinData;
		skinData.data = (SVF_W4B_I4S*)pRM->GetHWSkinPtr(skinData.iStride, FSL_READ);
		for(int i = pRM->GetIndicesCount()-3; i >= 0; i-=3)
		{
			Vec3 pthit;
			if (Intersect::Ray_Triangle(wray, vtx[idx[i]],vtx[idx[i+1]],vtx[idx[i+2]], pthit) && (pthit-wray.origin)*wray.direction < mindist)
			{
				mindist = (pthit-wray.origin)*wray.direction;
				SVF_W4B_I4S& hitvtx = skinData[idx[i]];
				int imax3 = idxmax3(hitvtx.weights.bcolor);
				ibone0 = hitvtx.indices[hitvtx.weights.bcolor[3] > hitvtx.weights.bcolor[imax3] ? 3 : imax3];
				if (sizer.m_ptrLast)
					ibone0 = ((JointIdType*)sizer.m_ptrLast)[ibone0];
			}
		}
		nvtxTot += pRM->GetVerticesCount();
		pRM->UnLockForThreadAccess();
	};

	RayCheckRMesh(m_character->GetRenderMesh());
	IAttachmentManager *pAtman = m_character->GetIAttachmentManager();
	for(int i = pAtman->GetAttachmentCount()-1; i >= 0; i--)
		if (IAttachmentSkin *pSkin = pAtman->GetInterfaceByIndex(i)->GetIAttachmentSkin())
			RayCheckRMesh(pSkin->GetISkin()->GetIRenderMesh(0), pSkin);

	if (ibone0 < 0 && mindist < 1e10f)
		ibone0 = rhit.partid;

	IDefaultSkeleton& skel = m_character->GetIDefaultSkeleton();
	m_curBoneName = ibone0 > 0 ? skel.GetJointNameByID(ibone0) : "";
	m_posMouse.set(ev.x, ev.y);
	m_isCurBoneFree = true;

	if (ibone0 > 0)
	{
		if (skel.GetJointPhysGeom(ibone0) && skel.GetJointPhysGeom(ibone0)->pGeom->GetiForeignData() != -1)
		{
			m_isCurBoneFree = false;
			if (ev.type != SMouseEvent::TYPE_PRESS || ev.button != SMouseEvent::BUTTON_LEFT)
				return false;
			m_document->GetDisplayOptions()->physics.selectedBone = ibone0;
			char name[256] = "$";
			strcpy(name+1, m_character->GetIDefaultSkeleton().GetJointNameByID(ibone0));
			for(uint i = 0; i < cdf->attachments.size(); i++)
				if (!_stricoll(name, cdf->attachments[i].m_strSocketName.c_str())) 
				{
					if (tree->selectedRow())
						tree->collapseChildren(tree->selectedRow());
					if (tree->selectByAddress(&cdf->attachments[i], false))
					{
						tree->expandRow(tree->selectedRow());
						tree->ensureVisible(tree->selectedRow());
						std::vector<const void*> handles; handles.push_back(&cdf->attachments[i]);
						m_system->characterGizmoManager->SetSubselection(GIZMO_LAYER_CHARACTER, handles);
						return true;
					}
				}
		}
		else if (ev.type == SMouseEvent::TYPE_PRESS && ev.button == SMouseEvent::BUTTON_LEFT)
		{
			m_document->GetDisplayOptions()->physics.selectedBone = ibone0;
			strided_pointer<CryBonePhysics> bonePhys;
			bonePhys.data = skel.GetJointPhysInfo(0);
			bonePhys.iStride = (int)((INT_PTR)skel.GetJointPhysInfo(1) - (INT_PTR)bonePhys.data);
			phys_geometry *pgeom0 = bonePhys[ibone0].pPhysGeom;
			bonePhys[ibone0].pPhysGeom = nullptr;
			// mark that bone's subtree, stop at bones that are already physicalized
			std::function<void(int,const phys_geometry*,const phys_geometry*)> IterateChildren = [&](int ibone, const phys_geometry *pCheckVal, const phys_geometry *pMarkVal)
			{
				if (bonePhys[ibone].pPhysGeom != pCheckVal)
					return;
				bonePhys[ibone].pPhysGeom = const_cast<phys_geometry*>(pMarkVal);
				for(int i = skel.GetJointChildrenCountByID(ibone)-1; i >= 0; i--)
					IterateChildren(skel.GetJointChildIDAtIndexByID(ibone, i), pCheckVal, pMarkVal);
			};
			const phys_geometry* MARKED_BONE = (phys_geometry*)(INT_PTR)-1;
			IterateChildren(ibone0, nullptr, MARKED_BONE);

			// select vertices that are influenced by more than 50% by bones from that list
			std::vector<Vec3> vtxBone; vtxBone.reserve(nvtxTot);
			QuatT qBone = skel.GetDefaultAbsJointByID(ibone0).GetInverted();
			auto PickVertices = [&](IRenderMesh *pRM, const phys_geometry *pMarkVal, IAttachmentSkin *pSkin = nullptr)
			{
				if (!pRM)
					return;
				pRM->LockForThreadAccess();
				SFakeSizer sizer;
				JointIdType dummyId = 0, *mapJoints = &dummyId, mapMask = 0;
				if (pSkin)
				{
					pSkin->GetMemoryUsage(&sizer);
					if (sizer.m_ptrLast)
					{
						mapJoints = (JointIdType*)sizer.m_ptrLast;
						mapMask = ~0;
					}
				}
				strided_pointer<Vec3> vtx;
				vtx.data = (Vec3*)pRM->GetPosPtr(vtx.iStride, FSL_READ);
				strided_pointer<SVF_W4B_I4S> skinData;
				skinData.data = (SVF_W4B_I4S*)pRM->GetHWSkinPtr(skinData.iStride, FSL_READ);
				for(int i = pRM->GetVerticesCount()-1, w; i >= 0; i--)
				{
					for(int j = w = 0; j < 4; j++)
						w += bonePhys[skinData[i].indices[j] & ~mapMask | mapJoints[skinData[i].indices[j] & mapMask]].pPhysGeom == pMarkVal ? skinData[i].weights.bcolor[j] : 0;
					if (w > 127)
						vtxBone.push_back(qBone * vtx[i]);
				}
				pRM->UnLockForThreadAccess();
			};
			PickVertices(m_character->GetRenderMesh(), MARKED_BONE);
			for(int i = pAtman->GetAttachmentCount()-1; i >= 0; i--)
				if (IAttachmentSkin *pSkin = pAtman->GetInterfaceByIndex(i)->GetIAttachmentSkin())
					PickVertices(pSkin->GetISkin()->GetIRenderMesh(0), MARKED_BONE, pSkin);
			IterateChildren(ibone0, MARKED_BONE, nullptr);
			bonePhys[ibone0].pPhysGeom = pgeom0;

			if (vtxBone.size())
			{
				// build a convext hull of the selected vertices
				index_t *idxHull = nullptr;
				int trisHull = gEnv->pPhysicalWorld->GetPhysUtils()->qhull(vtxBone.data(), vtxBone.size(), idxHull);
				IGeomManager *pGeoman = gEnv->pPhysicalWorld->GetGeomManager();
				IGeometry *pCHull = pGeoman->CreateMesh(vtxBone.data(), strided_pointer<ushort>(idxHull), nullptr,nullptr, trisHull, mesh_AABB | mesh_OBB);
				const int primFlags[] = { mesh_approx_box, mesh_approx_capsule, mesh_approx_sphere };
				const float tol[] = { 1e6f, 1.5f, 0.3f }; 
				IGeometry *pBestPrim = nullptr;
				for(int i = 0; i < 3; i++)
				{
					IGeometry *pPrim = pGeoman->CreateMesh(vtxBone.data(), strided_pointer<ushort>(idxHull), nullptr,nullptr, trisHull, primFlags[i], tol[i]);
					if (!pBestPrim || pPrim->GetType() != GEOM_TRIMESH && pBestPrim->GetVolume() >= pPrim->GetVolume())
					{
						if (pBestPrim) pBestPrim->Release();
						pBestPrim = pPrim;
					} 
					else
						pPrim->Release();
				}

				CharacterAttachment att;
				att.m_strJointName = skel.GetJointNameByID(ibone0);
				att.m_strSocketName = string("$") + att.m_strJointName;
				att.m_attachmentType = CA_PROX;
				att.m_ProxyPurpose = CharacterAttachment::RAGDOLL;
				att.m_rotationSpace = CharacterAttachment::SPACE_JOINT;
				att.m_positionSpace = CharacterAttachment::SPACE_JOINT;
				extern bool ProxyDimFromGeom(IGeometry *pGeom, QuatT& trans, Vec4& dim);
				ProxyDimFromGeom(pBestPrim, att.m_jointSpacePosition, att.m_ProxyParams);
				att.m_ProxyParams *= min(1.0f, (1 + pow(pCHull->GetVolume()/pBestPrim->GetVolume(), 1.0f/3)) * 0.5f);
				att.m_prevProxyParams = att.m_ProxyParams;
				att.m_characterSpacePosition = (att.m_boneTrans = skel.GetDefaultAbsJointByID(ibone0)) * att.m_jointSpacePosition;
				att.m_ProxyType = att.m_prevProxyType = (geomtypes)pBestPrim->GetType();
				(att.m_proxySrc = new CharacterAttachment::ProxySource)->m_hullMesh = pCHull;
				att.m_meshSmooth = 3;
				att.m_prevMeshSmooth = 0;
				att.m_limits[0] = -(att.m_limits[1] = Vec3i(0, 45, 45));
				att.m_tension = Vec3(1);
				att.m_damping = Vec3(1);
				att.m_frame0 = Ang3(0);
				att.UpdateMirrorInfo(ibone0, skel);
				cdf->attachments.push_back(att);
				pBestPrim->Release();
				pCHull->Release();

				m_window->GetPropertiesPanel()->PropertyTree()->revert();
				m_window->GetPropertiesPanel()->OnChanged();
				EntryModifiedEvent ev;
				ev.id = m_system->document->GetActiveCharacterEntry()->id;
				m_document->OnCharacterModified(ev);

				if (tree->selectByAddress(&cdf->attachments.end()[-1], false))
				{
					tree->expandRow(tree->selectedRow());
					tree->ensureVisible(tree->selectedRow());
					std::vector<const void*> handles; handles.push_back(&cdf->attachments.end()[-1]);
					m_system->characterGizmoManager->SetSubselection(GIZMO_LAYER_CHARACTER, handles);
				}
				return true;
			}
		}
	}
	return false;
}

void ModeCharacter::CommenceRagdollTest()
{
#if CRY_PLATFORM_WINDOWS
	if (!m_character)
		return;
	gEnv->pSystem->SetThreadState(ESubsys_Physics, false);
	ISkeletonPose& skel = *m_character->GetISkeletonPose();
	skel.DestroyCharacterPhysics();
	const Vec3 pos0(10, 10, 100);
	pe_params_pos pp;
	pp.pos = pos0;
	pp.iSimClass = 2;
	IPhysicalEntity *pents[6];
	pents[0] = gEnv->pPhysicalWorld->CreatePhysicalEntity(PE_ARTICULATED, &pp);
	skel.BuildPhysicalEntity(pents[0], 80, 0, 0, 1);
	skel.CreateAuxilaryPhysics(pents[0], Matrix34(Vec3(1), pp.q, pp.pos), 1);
	pe_action_reset ar;
	pents[0]->Action(&ar, 1);
	pe_params_articulated_body pab;
	pab.bCollisionResp = 1;
	pab.bCheckCollisions = 1;
	pents[0]->SetParams(&pab, 1);
	pe_params_flags pf;
	pf.flagsOR = pef_update;
	pents[0]->SetParams(&pf, 1);
	pe_params_part ppart;
	for(int i = 0; (pents[1] = skel.GetCharacterPhysics(i)); i++)
	{
		pents[1]->SetParams(&pf, 1);
		for(ppart.ipart = 0; pents[1]->GetParams(&ppart); ppart.ipart++)
			ppart.pPhysGeom->nMats |= pef_update;
	}
	pe_status_pos sp;
	pents[0]->GetStatus(&sp);
	for(ppart.ipart = 0; pents[0]->GetParams(&ppart); ppart.ipart++)
		ppart.pPhysGeom->nMats |= pef_update;

	MARK_UNUSED pp.iSimClass;
	pp.q.SetIdentity();
	float h = sp.BBox[1].z - sp.BBox[0].z;
	primitives::box abox;
	IGeomManager *pGeoman = gEnv->pPhysicalWorld->GetGeomManager();
	pe_geomparams gp;
	abox.Basis.SetIdentity();
	abox.center.zero();
	abox.size.Set(h * 3, h * 3, h * 0.25f);
	phys_geometry *pgeom = pGeoman->RegisterGeometry(pGeoman->CreatePrimitive(primitives::box::type, &abox));
	pgeom->pGeom->Release(); pgeom->nMats |= pef_update;
	gp.pos.Set(0, 0, -abox.size.z);
	pp.pos = pos0;
	pents[1] = gEnv->pPhysicalWorld->CreatePhysicalEntity(PE_STATIC, &pp);
	pents[1]->SetParams(&pf, 1);
	pents[1]->AddGeometry(pgeom, &gp, -1, 1); --pgeom->nRefCount;
	abox.size.Set(h * 0.25f, h * 0.2f, h * 0.15f);
	pgeom = pGeoman->RegisterGeometry(pGeoman->CreatePrimitive(primitives::box::type, &abox));
	pgeom->pGeom->Release(); pgeom->nMats |= pef_update;
	gp.pos.Set(0, 0, abox.size.z);
	pp.pos = pos0 - Vec3(0, h * 0.6f, 0);
	pents[2] = gEnv->pPhysicalWorld->CreatePhysicalEntity(PE_STATIC, &pp);
	pents[2]->SetParams(&pf, 1);
	pents[2]->AddGeometry(pgeom, &gp, -1, 1); --pgeom->nRefCount;
	abox.size.Set(h * 0.45f, h * 0.25f, h * 0.03f);
	pgeom = pGeoman->RegisterGeometry(pGeoman->CreatePrimitive(primitives::box::type, &abox));
	pgeom->pGeom->Release(); pgeom->nMats |= pef_update;
	gp.pos.Set(0, 0, abox.size.z);
	pp.pos = pos0 - Vec3(0, h * 1.4f, h * -0.6f);
	pents[3] = gEnv->pPhysicalWorld->CreatePhysicalEntity(PE_RIGID, &pp);
	pents[3]->SetParams(&pf, 1);
	pents[3]->AddGeometry(pgeom, &gp, -1, 1); --pgeom->nRefCount;
	pe_action_set_velocity asv;
	asv.w.Set(0, 1, 0);
	pents[3]->Action(&asv, 1);
	const int tess = 64;
	const float cosa = cos(gf_PI * 2 / tess), sina = sin(gf_PI * 2 / tess);
	Vec3 *vtx = new Vec3[tess + 1];
	Vec3_tpl<short> *tri = new Vec3_tpl<short>[tess];
	Vec2 r(h * 5, 0);
	vtx[tess].Set(0, 0, -h * 6);
	for(int i = 0; i < tess; i++, r.set(r.x * cosa - r.y *sina, r.x * sina + r.y * cosa))
	{
		vtx[i] = r;	
		tri[i].Set(i, i + 1 & tess - 1, tess);
	}
	pgeom = pGeoman->RegisterGeometry(pGeoman->CreateMesh(vtx, &tri[0].x, nullptr, nullptr, tess, mesh_OBB | mesh_multicontact1 | mesh_no_filter | mesh_no_vtx_merge, 0));
	pgeom->pGeom->Release(); pgeom->nMats |= pef_update;
	gp.pos = -vtx[tess];
	delete[] tri; delete[] vtx;
	gp.q = Quat::CreateRotationAA(DEG2RAD(-20), Vec3(1, 0, 0));
	pp.pos = pos0 - Vec3(0, 0, h * 3);
	pents[4] = gEnv->pPhysicalWorld->CreatePhysicalEntity(PE_STATIC, &pp);
	pents[4]->SetParams(&pf, 1);
	pents[4]->AddGeometry(pgeom, &gp, -1, 1); --pgeom->nRefCount;
	
	pp.pos = pos0 + Vec3(0, h * 2, h * 0.5f);
	pp.q = Quat::CreateRotationAA(0, 1, Vec3(0, 0, 1));
	pents[5] = gEnv->pPhysicalWorld->CreatePhysicalEntity(PE_LIVING, &pp);
	pents[5]->SetParams(&pf, 1);
	pe_player_dimensions pdim;
	pdim.sizeCollider.zero();
	pe_player_dynamics pdyn;
	pdyn.bActive = 0;
	pents[5]->SetParams(&pdim, 1);
	pents[5]->SetParams(&pdyn, 1);

	char buf[MAX_PATH * 3 + 13 + 17] = "Cannot start ", *path = buf + 13, pathTmp[MAX_PATH], *fnameEnts, *fnameGeoms;
	CryGetExecutableFolder(MAX_PATH, path);
	strcat(path, "PhysDebugger.exe");
	GetTempPath(MAX_PATH, pathTmp);
	GetTempFileName(pathTmp, "ents", 0, fnameEnts = path + strlen(path) + 1);
	GetTempFileName(pathTmp, "geoms", 0, fnameGeoms = fnameEnts + strlen(fnameEnts) + 1);
	gEnv->pPhysicalWorld->SerializeGeometries(fnameGeoms, 1 + ent_flagged_only);
	gEnv->pPhysicalWorld->SerializeWorld(fnameEnts, 1 + ent_flagged_only);

	for(int i = 0; i < CRY_ARRAY_COUNT(pents); i++)
		gEnv->pPhysicalWorld->DestroyPhysicalEntity(pents[i]);
	skel.DestroyCharacterPhysics();
	EntryModifiedEvent ev;
	ev.id = m_system->document->GetActiveCharacterEntry()->id;
	m_document->OnCharacterModified(ev);
	gEnv->pSystem->SetThreadState(ESubsys_Physics, true);

	fnameGeoms[-1] = fnameEnts[-1] = ' ';
	STARTUPINFO si;	memset(&si, 0, sizeof(si));
	si.cb = sizeof(si);
	PROCESS_INFORMATION pi;
	if (!CreateProcess(nullptr, path, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi))
	{
		fnameGeoms[-1] = fnameEnts[-1] = 0;
		DeleteFile(fnameGeoms);
		DeleteFile(fnameEnts);
		CryMessageBox(buf, "Error", eMB_Error);
	}
#endif
}

void ModeCharacter::OnActionRotateTool()
{
	m_scene->SetTransformationMode(Manip::MODE_ROTATE);
	OnSceneManipulationModeChanged();
}

void ModeCharacter::OnActionMoveTool()
{
	m_scene->SetTransformationMode(Manip::MODE_TRANSLATE);
	OnSceneManipulationModeChanged();
}

void ModeCharacter::OnActionScaleTool()
{
	m_scene->SetTransformationMode(Manip::MODE_SCALE);
	OnSceneManipulationModeChanged();
}

void ModeCharacter::UpdateToolbar()
{
	if (!m_document)
		return;

	m_actionMoveTool->setEnabled(true);
	m_actionRotateTool->setEnabled(true);
	m_actionScaleTool->setEnabled(true);
}

}

