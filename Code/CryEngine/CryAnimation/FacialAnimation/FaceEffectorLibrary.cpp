// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "FaceEffectorLibrary.h"

#include "CharacterManager.h"
#include "FaceAnimation.h"

inline const char* EffectorTypeToName(EFacialEffectorType eftype)
{
	switch (eftype)
	{
	case EFE_TYPE_GROUP:
		return "group";
	case EFE_TYPE_EXPRESSION:
		return "expression";
	case EFE_TYPE_MORPH_TARGET:
		return "morph";
	case EFE_TYPE_BONE:
		return "bone";
	case EFE_TYPE_ATTACHMENT:
		return "attachment";
	case EFE_TYPE_MATERIAL:
		return "material";
	}
	return "unknown";
}

inline EFacialEffectorType NameToEffectorType(const char* name)
{
	if (!strcmp(name, "group"))
		return EFE_TYPE_GROUP;
	if (!strcmp(name, "expression"))
		return EFE_TYPE_EXPRESSION;
	if (!strcmp(name, "morph"))
		return EFE_TYPE_MORPH_TARGET;
	if (!strcmp(name, "bone"))
		return EFE_TYPE_BONE;
	if (!strcmp(name, "attachment"))
		return EFE_TYPE_ATTACHMENT;
	if (!strcmp(name, "material"))
		return EFE_TYPE_MATERIAL;

	return EFE_TYPE_GROUP;
}

//////////////////////////////////////////////////////////////////////////
CFacialEffectorsLibrary::CFacialEffectorsLibrary(CFacialAnimation* pFaceAnim)
{
	m_nRefCounter = 0;
	m_nLastIndex = 0;
	NewRoot();
	m_pFacialAnim = pFaceAnim;
	m_pFacialAnim->RegisterLib(this);
	m_indexToEffector.reserve(20);
}

//////////////////////////////////////////////////////////////////////////
CFacialEffectorsLibrary::~CFacialEffectorsLibrary()
{
	m_pFacialAnim->UnregisterLib(this);
}

//////////////////////////////////////////////////////////////////////////
IFacialEffector* CFacialEffectorsLibrary::Find(CFaceIdentifierHandle ident)
{
	CFacialEffector* pEffector = stl::find_in_map(m_crcToEffectorMap, ident.GetCRC32(), NULL);
	return pEffector;
}

IFacialEffector* CFacialEffectorsLibrary::Find(const char* identStr)
{
	return Find(g_pCharacterManager->GetFacialAnimation()->CreateIdentifierHandle(identStr));
}

//////////////////////////////////////////////////////////////////////////
IFacialEffector* CFacialEffectorsLibrary::GetRoot()
{
	return m_pRoot;
}

//////////////////////////////////////////////////////////////////////////
void CFacialEffectorsLibrary::VisitEffectors(IFacialEffectorsLibraryEffectorVisitor* pVisitor)
{
	for (CrcToEffectorMap::iterator it = m_crcToEffectorMap.begin(), end = m_crcToEffectorMap.end(); it != end; ++it)
	{
		if (pVisitor)
			pVisitor->VisitEffector((*it).second);
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialEffectorsLibrary::NewRoot()
{
	m_pRoot = new CFacialEffector;
	CFacialAnimation* pFaceAnim = g_pCharacterManager->GetFacialAnimation();
	if (!pFaceAnim)
		return;
	m_pRoot->SetIdentifier(pFaceAnim->CreateIdentifierHandle("Root"));
	m_pRoot->SetFlags(EFE_FLAG_UI_EXTENDED | EFE_FLAG_ROOT);
	m_crcToEffectorMap[m_pRoot->GetIdentifier().GetCRC32()] = m_pRoot;
}

//////////////////////////////////////////////////////////////////////////
IFacialEffector* CFacialEffectorsLibrary::CreateEffector(EFacialEffectorType nType, CFaceIdentifierHandle ident)
{
	CFacialEffector* pEffector = (CFacialEffector*)Find(ident);
	if (pEffector)
	{
		CryWarning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_WARNING, "Effector with name %s already exist", ident.GetString());
		return pEffector;
	}
	switch (nType)
	{
	case EFE_TYPE_GROUP:
		pEffector = new CFacialEffector;
		break;
	case EFE_TYPE_EXPRESSION:
		pEffector = new CFacialEffectorExpression;
		break;
	case EFE_TYPE_MORPH_TARGET:
		pEffector = new CFacialMorphTarget(~0);
		SetEffectorIndex(pEffector);
		break;
	case EFE_TYPE_BONE:
		pEffector = new CFacialEffectorBone();
		SetEffectorIndex(pEffector);
		break;
	case EFE_TYPE_ATTACHMENT:
		pEffector = new CFacialEffectorAttachment();
		SetEffectorIndex(pEffector);
		break;
	}
	if (pEffector)
	{
		pEffector->SetLibrary(this);
		pEffector->SetIdentifier(ident);
		if (ident.GetCRC32())
			m_crcToEffectorMap[ident.GetCRC32()] = pEffector;
	}
	return pEffector;
}

IFacialEffector* CFacialEffectorsLibrary::CreateEffector(EFacialEffectorType nType, const char* identStr)
{
	CFacialAnimation* pFaceAnim = g_pCharacterManager->GetFacialAnimation();
	if (pFaceAnim)
	{
		return CreateEffector(nType, pFaceAnim->CreateIdentifierHandle(identStr));
	}
	return NULL;
}

//////////////////////////////////////////////////////////////////////////
void CFacialEffectorsLibrary::RemoveEffector(IFacialEffector* pEff)
{
	CFacialEffector* pEffector = (CFacialEffector*)pEff;

	if (pEffector->GetIndexInState() >= 0)
	{
		m_indexToEffector[m_nLastIndex - 1]->SetIndexInState(pEffector->GetIndexInState());
		m_indexToEffector[pEffector->GetIndexInState()] = m_indexToEffector[m_nLastIndex - 1];
		--m_nLastIndex;
		((CFacialEffector*)pEffector)->SetIndexInState(-1);
	}

	if (pEffector->GetIdentifier().GetCRC32())
	{
		CrcToEffectorMap::iterator itNameEntry = m_crcToEffectorMap.find(pEffector->GetIdentifier().GetCRC32());
		if (itNameEntry == m_crcToEffectorMap.end())
			return;

		if ((*itNameEntry).second != pEffector)
			return;

		m_crcToEffectorMap.erase(itNameEntry);
	}

	class Recurser
	{
	public:
		Recurser(CFacialEffectorsLibrary* pLibrary, IFacialEffector* pEffectorToRemove)
			: pLibrary(pLibrary),
			pEffectorToRemove(pEffectorToRemove)
		{
		}

		void operator()(IFacialEffector* pEffector)
		{
			Recurse(pEffector);
		}

	private:
		void Recurse(IFacialEffector* pEffector)
		{
			for (int i = 0; i < pEffector->GetSubEffectorCount(); )
			{
				if (pEffector->GetSubEffector(i) == pEffectorToRemove)
				{
					pEffector->RemoveSubEffector(pEffectorToRemove);
				}
				else
				{
					Recurse(pEffector->GetSubEffector(i));
					++i;
				}
			}
		}

		CFacialEffectorsLibrary* pLibrary;
		IFacialEffector*         pEffectorToRemove;
	};

	Recurser(this, pEffector)(GetRoot());
}

//////////////////////////////////////////////////////////////////////////
void CFacialEffectorsLibrary::SetEffectorIndex(CFacialEffector* pEffector)
{
	int nIndex = m_nLastIndex++;
	m_indexToEffector.resize(nIndex + 1);
	pEffector->SetIndexInState(nIndex);
	m_indexToEffector[nIndex] = pEffector;
}

//////////////////////////////////////////////////////////////////////////
void CFacialEffectorsLibrary::ReplaceEffector(CFacialEffector* pOriginalEffector, CFacialEffector* pNewEffector)
{
	if (pOriginalEffector->GetIdentifier().GetCRC32())
	{
		CrcToEffectorMap::iterator itNameEntry = m_crcToEffectorMap.find(pOriginalEffector->GetIdentifier().GetCRC32());
		if (itNameEntry == m_crcToEffectorMap.end())
			return;

		if ((*itNameEntry).second != pOriginalEffector)
			return;

		m_crcToEffectorMap.erase(itNameEntry);
	}

	if (pOriginalEffector->GetIndexInState() >= 0)
	{
		m_indexToEffector[m_nLastIndex - 1]->SetIndexInState(pOriginalEffector->GetIndexInState());
		m_indexToEffector[pOriginalEffector->GetIndexInState()] = m_indexToEffector[m_nLastIndex - 1];
		--m_nLastIndex;
		((CFacialEffector*)pOriginalEffector)->SetIndexInState(-1);
	}

	class Recurser
	{
	public:
		Recurser(CFacialEffectorsLibrary* pLibrary, IFacialEffector* pEffectorToRemove, IFacialEffector* pNewEffector)
			: pLibrary(pLibrary),
			pEffectorToRemove(pEffectorToRemove),
			pNewEffector(pNewEffector)
		{
		}

		void operator()(IFacialEffector* pEffector)
		{
			Recurse(pEffector);
		}

	private:
		void Recurse(IFacialEffector* pEffector)
		{
			for (int i = 0; i < pEffector->GetSubEffectorCount(); ++i)
			{
				if (pEffector->GetSubEffector(i) == pEffectorToRemove)
				{
					((CFacialEffCtrl*)pEffector->GetSubEffCtrl(i))->SetCEffector((CFacialEffector*)pNewEffector);
				}
				else
				{
					assert(pEffector != pEffector->GetSubEffector(i));
					Recurse(pEffector->GetSubEffector(i));
				}
			}
		}

		CFacialEffectorsLibrary* pLibrary;
		IFacialEffector*         pEffectorToRemove;
		IFacialEffector*         pNewEffector;
	};

	Recurser(this, pOriginalEffector, pNewEffector)(this->GetRoot());
}

//////////////////////////////////////////////////////////////////////////
CFacialEffector* CFacialEffectorsLibrary::GetEffectorFromIndex(int nIndex)
{
	if (nIndex >= 0 && nIndex < (int)m_indexToEffector.size())
	{
		return m_indexToEffector[nIndex];
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CFacialEffectorsLibrary::MergeLibrary(IFacialEffectorsLibrary* pMergeLibrary, const Functor1wRet<const char*, MergeCollisionAction>& collisionStrategy)
{
#ifdef FACE_STORE_ASSET_VALUES
	if (this == pMergeLibrary)
	{
		CryWarning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_WARNING, "Attempting to merge library with itself - aborting.");
		return;
	}

	typedef Functor1wRet<const char*, MergeCollisionAction> OverwriteConfirmationRequester;

	class Recurser
	{
	public:
		typedef std::map<IFacialEffector*, IFacialEffector*> CopiedEffectorsMap;

		Recurser(IFacialEffectorsLibrary* pLibrary, IFacialEffectorsLibrary* pMergeLibrary, const OverwriteConfirmationRequester& confirmer)
			: pLibrary(pLibrary),
			pMergeLibrary(pMergeLibrary),
			confirmer(confirmer)
		{
		}

		void operator()()
		{
			Recurse(pLibrary->GetRoot(), pMergeLibrary->GetRoot());
		}

	private:
		void Recurse(IFacialEffector* pEffector, IFacialEffector* pMergeEffector)
		{
			for (int subEffectorIndex = 0; subEffectorIndex < pMergeEffector->GetSubEffectorCount(); ++subEffectorIndex)
			{
				IFacialEffCtrl* pMergeSubCtrl = pMergeEffector->GetSubEffCtrl(subEffectorIndex);
				IFacialEffector* pMergeSubEffector = pMergeSubCtrl->GetEffector();

				IFacialEffector* pOriginalSubEffector = pLibrary->Find(pMergeSubEffector->GetName());

				bool updateSubEffector = true;
				if (pOriginalSubEffector && pOriginalSubEffector->GetType() != EFE_TYPE_GROUP && copiedEffectors.find(pMergeSubEffector) == copiedEffectors.end())
				{
					if (pOriginalSubEffector->GetType() == EFE_TYPE_MORPH_TARGET)
						updateSubEffector = false;
					else
						updateSubEffector = (confirmer(pMergeSubEffector->GetName()) == MergeCollisionActionOverwrite);
				}

				bool augmentSubEffector = false;
				if (pOriginalSubEffector && pOriginalSubEffector->GetType() == EFE_TYPE_GROUP && pMergeSubEffector->GetType() == EFE_TYPE_GROUP)
					augmentSubEffector = true;

				IFacialEffector* pNewSubEffector = 0;
				bool renameEffector = false;
				if (!updateSubEffector)
				{
					pNewSubEffector = pOriginalSubEffector;
					pOriginalSubEffector = 0;
				}
				else
				{
					if (augmentSubEffector)
					{
						pNewSubEffector = pOriginalSubEffector;
						pOriginalSubEffector = 0;
						if (copiedEffectors.find(pMergeSubEffector) == copiedEffectors.end())
						{
							copiedEffectors.insert(std::make_pair(pMergeSubEffector, pNewSubEffector));
							Recurse(pNewSubEffector, pMergeSubEffector);
						}
					}
					else
					{
						CopiedEffectorsMap::iterator itEffector = copiedEffectors.find(pMergeSubEffector);
						if (itEffector == copiedEffectors.end())
						{
							pNewSubEffector = pLibrary->CreateEffector(pMergeSubEffector->GetType(), CFaceIdentifierHandle());
							renameEffector = true;
							itEffector = copiedEffectors.insert(std::make_pair(pMergeSubEffector, pNewSubEffector)).first;

							pNewSubEffector->SetFlags(pMergeSubEffector->GetFlags());

							switch (pNewSubEffector->GetType())
							{
							case EFE_TYPE_GROUP:
								break;
							case EFE_TYPE_EXPRESSION:
								break;
							case EFE_TYPE_MORPH_TARGET:
								((CFacialMorphTarget*)pNewSubEffector)->SetMorphTargetId(((CFacialMorphTarget*)pMergeSubEffector)->GetMorphTargetId());
								break;
							case EFE_TYPE_BONE:
								((CFacialEffectorBone*)pNewSubEffector)->Set((CFacialEffectorBone*)pMergeSubEffector);
								break;
							case EFE_TYPE_MATERIAL:
								break;
							case EFE_TYPE_ATTACHMENT:
								CryFatalError("Attachment Effectors no longer supported in Facial Animation system.");
								break;
							default:
								assert(0);
								break;
							}

							Recurse(pNewSubEffector, pMergeSubEffector);
						}
						pNewSubEffector = (*itEffector).second;
						if (pOriginalSubEffector == pNewSubEffector)
							pOriginalSubEffector = 0;
					}
				}

				if (pNewSubEffector && pOriginalSubEffector)
					((CFacialEffectorsLibrary*)pLibrary)->ReplaceEffector((CFacialEffector*) pOriginalSubEffector, (CFacialEffector*) pNewSubEffector);

				if (pNewSubEffector && renameEffector)
					pNewSubEffector->SetName(pMergeSubEffector->GetName());

				// Check whether this effector already has this sub effector.
				IFacialEffCtrl* pExistingCtrl = 0;
				for (int i = 0; i < pEffector->GetSubEffectorCount() && pNewSubEffector; ++i)
				{
					if (pEffector->GetSubEffector(i) == pNewSubEffector)
						pExistingCtrl = pEffector->GetSubEffCtrl(i);
				}
				if (!pExistingCtrl && pNewSubEffector)
				{
					IFacialEffCtrl* pSubEffectorCtrl = pEffector->AddSubEffector(pNewSubEffector);
					CopyController(pSubEffectorCtrl, pMergeSubCtrl);
				}
			}
		}

		void CopyController(IFacialEffCtrl* pCloneSubCtrl, IFacialEffCtrl* pOriginalSubCtrl)
		{
			pCloneSubCtrl->SetFlags(pOriginalSubCtrl->GetFlags());
			pCloneSubCtrl->SetConstantBalance(pOriginalSubCtrl->GetConstantBalance());
			pCloneSubCtrl->SetConstantWeight(pOriginalSubCtrl->GetConstantWeight());

			// Copy the curve information.
			pCloneSubCtrl->SetType(pOriginalSubCtrl->GetType());
			ISplineInterpolator* pOriginalSpline = pOriginalSubCtrl->GetSpline();
			ISplineInterpolator* pCloneSpline = pCloneSubCtrl->GetSpline();
			while (pCloneSpline && pCloneSpline->GetKeyCount() > 0)
				pCloneSpline->RemoveKey(0);
			for (int key = 0; key < pOriginalSpline->GetKeyCount(); ++key)
			{
				ISplineInterpolator::ValueType value;
				pOriginalSpline->GetKeyValue(key, value);
				assert(pCloneSpline);
				PREFAST_ASSUME(pCloneSpline);
				int cloneKeyIndex = pCloneSpline->InsertKey(pOriginalSpline->GetKeyTime(key), value);
				pCloneSpline->SetKeyFlags(cloneKeyIndex, pOriginalSpline->GetKeyFlags(key));
				ISplineInterpolator::ValueType tin, tout;
				pOriginalSpline->GetKeyTangents(key, tin, tout);
				pCloneSpline->SetKeyTangents(cloneKeyIndex, tin, tout);
			}
		}

		IFacialEffectorsLibrary*              pLibrary;
		IFacialEffectorsLibrary*              pMergeLibrary;
		const OverwriteConfirmationRequester& confirmer;
		CopiedEffectorsMap                    copiedEffectors;
	};

	Recurser(this, pMergeLibrary, collisionStrategy)();
#else
	CryFatalError("Merging of Libraries is not supported on this platform");
#endif
}

//////////////////////////////////////////////////////////////////////////
void CFacialEffectorsLibrary::Serialize(XmlNodeRef& node, bool bLoading)
{
	if (bLoading)
	{
		CFacialAnimation* pFaceAnim = g_pCharacterManager->GetFacialAnimation();
		if (!pFaceAnim)
			return;

		m_nLastIndex = 0;
		m_crcToEffectorMap.clear();
		m_indexToEffector.clear();
		m_pRoot = 0;

		// Saving.
		XmlNodeRef effectorsNode = node->findChild("AllEffectors");
		if (effectorsNode)
		{
			for (int i = 0; i < effectorsNode->getChildCount(); i++)
			{
				XmlNodeRef effNode = effectorsNode->getChild(i);
				EFacialEffectorType efType = NameToEffectorType(effNode->getAttr("Type"));

				CFaceIdentifierHandle handle = pFaceAnim->CreateIdentifierHandle(effNode->getAttr("Name"));
				CFacialEffector* pEffector = (CFacialEffector*)CreateEffector(efType, handle);
				if (!pEffector)
					continue;
				int flags = 0;
				effNode->getAttr("Flags", flags);
				pEffector->SetFlags(flags);

				pEffector->Serialize(effNode, bLoading);

				if (flags & EFE_FLAG_ROOT)
				{
					// Assign root.
					m_pRoot = pEffector;
				}
			}
		}

		XmlNodeRef controllersNode = node->findChild("Controllers");
		if (controllersNode)
		{
			for (int i = 0; i < controllersNode->getChildCount(); i++)
			{
				XmlNodeRef ctrlNode = controllersNode->getChild(i);
				CFaceIdentifierHandle handle = pFaceAnim->CreateIdentifierHandle(ctrlNode->getAttr("Name"));
				CFacialEffector* pEffector = (CFacialEffector*)Find(handle);
				if (!pEffector)
					continue;
				SerializeEffectorSubCtrls(pEffector, ctrlNode, bLoading);
			}
		}
	}
	else
	{
		// Saving.
		XmlNodeRef effectorsNode = node->newChild("AllEffectors");

		//////////////////////////////////////////////////////////////////////////
		// Save all effectors first.
		//////////////////////////////////////////////////////////////////////////
		for (CrcToEffectorMap::iterator it = m_crcToEffectorMap.begin(); it != m_crcToEffectorMap.end(); ++it)
		{
			CFacialEffector* pEffector = it->second;
			//if (pEffector == m_pRoot) // Skip saving root effector.
			//continue;
			if (pEffector->GetFlags() & EFE_FLAG_UI_PREVIEW)
				continue;
			XmlNodeRef effectorNode = effectorsNode->newChild("Effector");
			SerializeEffectorSelf(pEffector, effectorNode, bLoading);
		}

		XmlNodeRef controllersNode = node->newChild("Controllers");
		//////////////////////////////////////////////////////////////////////////
		// Save all effectors first.
		//////////////////////////////////////////////////////////////////////////
		for (CrcToEffectorMap::iterator it = m_crcToEffectorMap.begin(); it != m_crcToEffectorMap.end(); ++it)
		{
			CFacialEffector* pEffector = it->second;
			//if (pEffector == m_pRoot) // Skip saving root effector.
			//continue;
			if (pEffector->GetFlags() & EFE_FLAG_UI_PREVIEW)
				continue;
			if (pEffector->GetSubEffectorCount() > 0)
			{
				XmlNodeRef ctrlsNode = controllersNode->newChild("Effector");
				SerializeEffectorSubCtrls(pEffector, ctrlsNode, bLoading);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialEffectorsLibrary::RenameEffector(CFacialEffector* pEffector, CFaceIdentifierHandle newIdent)
{
	CFaceIdentifierHandle ident = pEffector->GetIdentifier();
	if (ident.GetCRC32())
	{
		m_crcToEffectorMap.erase(ident.GetCRC32());
	}
	if (newIdent.GetCRC32())
		m_crcToEffectorMap[newIdent.GetCRC32()] = pEffector;
	pEffector->SetIdentifierByLibrary(newIdent);
}

//////////////////////////////////////////////////////////////////////////
void CFacialEffectorsLibrary::SerializeEffectorSelf(CFacialEffector* pEffector, XmlNodeRef& node, bool bLoading)
{
	if (bLoading)
	{
		int flags = 0;
		const char* sName = node->getAttr("Name");
		node->getAttr("Flags", flags);

		CFacialAnimation* pFaceAnim = g_pCharacterManager->GetFacialAnimation();
		if (!pFaceAnim)
			return;

		RenameEffector(pEffector, pFaceAnim->CreateIdentifierHandle(sName));

		pEffector->SetFlags(flags);

		pEffector->Serialize(node, bLoading);
	}
	else
	{
#ifdef FACE_STORE_ASSET_VALUES
		// Save
		node->setAttr("Name", pEffector->GetName());
		int nFlags = pEffector->GetFlags() & (~(EFE_FLAG_UI_MODIFIED | EFE_FLAG_UI_EXTENDED));
		if (nFlags != 0)
			node->setAttr("Flags", nFlags);
		node->setAttr("Type", EffectorTypeToName(pEffector->GetType()));

		pEffector->Serialize(node, bLoading);
#endif
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialEffectorsLibrary::SerializeEffectorSubCtrls(CFacialEffector* pEffector, XmlNodeRef& node, bool bLoading)
{
	if (bLoading)
	{
		pEffector->RemoveAllSubEffectors();
		for (int i = 0; i < node->getChildCount(); i++)
		{
			XmlNodeRef subNode = node->getChild(i);
			const char* sEffName = subNode->getAttr("Effector");
			IFacialEffector* pSubEffector = Find(sEffName);
			if (!pSubEffector)
				continue;
			CFacialEffCtrl* pCtrl = (CFacialEffCtrl*)pEffector->AddSubEffector(pSubEffector);

			int nType = CFacialEffCtrl::CTRL_LINEAR;
			pCtrl->m_fWeight = 0;
			subNode->getAttr("Type", nType);
			subNode->getAttr("Weight", pCtrl->m_fWeight);
			pCtrl->SetType((CFacialEffCtrl::ControlType)nType);
			if (pCtrl->m_type == CFacialEffCtrl::CTRL_SPLINE)
				pCtrl->SerializeSpline(subNode, bLoading);
		}
	}
	else
	{
#ifdef FACE_STORE_ASSET_VALUES
		// Save
		node->setAttr("Name", pEffector->GetName());

		// Save sub controllers.
		int numSub = pEffector->GetSubEffectorCount();
		for (int i = 0; i < numSub; ++i)
		{
			XmlNodeRef subNode = node->newChild("SubCtrl");
			CFacialEffCtrl* pCtrl = pEffector->GetSubCtrl(i);
			CRY_ASSERT(pCtrl);

			if (pCtrl->m_type != CFacialEffCtrl::CTRL_LINEAR)
				subNode->setAttr("Type", pCtrl->m_type);
			if (pCtrl->m_fWeight != 0)
				subNode->setAttr("Weight", pCtrl->m_fWeight);
			//if (pCtrl->m_type == CFacialEffCtrl::CTRL_SPLINE && pCtrl->m_pSpline)
			pCtrl->SerializeSpline(subNode, bLoading);
			subNode->setAttr("Effector", pCtrl->m_pEffector->GetName());
		}
#endif
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialEffectorsLibrary::SerializeEffector(IFacialEffector* pIEffector, XmlNodeRef& node, bool bLoading)
{
	SerializeEffectorSelf((CFacialEffector*)pIEffector, node, bLoading);
	SerializeEffectorSubCtrls((CFacialEffector*)pIEffector, node, bLoading);
}

void CFacialEffectorsLibrary::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->AddObject(this, sizeof(*this));
	pSizer->AddObject(m_nRefCounter);
	pSizer->AddObject(m_name);
	pSizer->AddObject(m_crcToEffectorMap);
	pSizer->AddObject(m_indexToEffector);
	pSizer->AddObject(m_nLastIndex);
	pSizer->AddObject(&m_indexToEffector, sizeof(m_indexToEffector) + m_indexToEffector.capacity() * sizeof(void*));
}
