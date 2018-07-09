// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FacialEdContext.h"
#include "OverwriteConfirmDialog.h"

#include <CryAnimation/IFacialAnimation.h>
#include <CryAnimation/ICryAnimation.h>
#include <CrySystem/ITimer.h>
#include <CryInput/IJoystick.h>
#include <CryCore/Containers/VectorSet.h>
#include <CryCore/Containers/VectorMap.h>
#include "FacialExpressionUtils.h"
#include "AnimUtils.h"
#include <CryCore/StlUtils.h>
#include <CrySystem/File/IAVI_Reader.h>
#include "Controls/QuestionDialog.h"
#include "FilePathUtil.h"
#include "IUndoObject.h"

//////////////////////////////////////////////////////////////////////////
class CUndoLibraryChange : public IUndoObject
{
public:
	CUndoLibraryChange(IFacialEffectorsLibrary* pLibrary, IUndoLibraryChangeContext* pContext)
		: m_pLibrary(pLibrary),
		m_pContext(pContext)
	{
		m_undo = XmlHelpers::CreateXmlNode("undo");
		m_redo = XmlHelpers::CreateXmlNode("redo");
		if (m_pLibrary && m_undo)
			m_pLibrary->Serialize(m_undo, false);
	}

	// IUndoObject
	virtual int GetSize()
	{
		return sizeof(*this);
	}

	virtual const char* GetDescription()
	{
		return "Library Change";
	}

	virtual void Undo(bool bUndo)
	{
		if (bUndo && m_pLibrary && m_redo)
			m_pLibrary->Serialize(m_redo, false);
		if (m_pLibrary && m_undo)
			m_pLibrary->Serialize(m_undo, true);
		if (m_pContext)
			m_pContext->OnLibraryUndo();
	}

	virtual void Redo()
	{
		if (m_pLibrary && m_redo)
			m_pLibrary->Serialize(m_redo, true);
		if (m_pContext)
			m_pContext->OnLibraryUndo();
	}

private:
	_smart_ptr<IFacialEffectorsLibrary> m_pLibrary;
	IUndoLibraryChangeContext*          m_pContext;
	XmlNodeRef                          m_undo;
	XmlNodeRef                          m_redo;
};

//////////////////////////////////////////////////////////////////////////
class CUndoSequenceChange : public IUndoObject
{
public:
	CUndoSequenceChange(IFacialAnimSequence* pSequence, IUndoSequenceChangeContext* pContext, SequenceChangeType changeType)
		: m_pSequence(pSequence), m_pContext(pContext), m_changeType(changeType)
	{
		m_serializeFlags = IFacialAnimSequence::SFLAG_ALL;
		if (changeType == FE_SEQ_CHANGE_SOUND_TIMES)
			m_serializeFlags = IFacialAnimSequence::SFLAG_SOUND_ENTRIES;

		m_undo = XmlHelpers::CreateXmlNode("undo");
		m_redo = XmlHelpers::CreateXmlNode("redo");
		if (m_pSequence && m_undo)
			m_pSequence->Serialize(m_undo, false, m_serializeFlags);
	}

	// IUndoObject
	virtual int GetSize()
	{
		return sizeof(*this);
	}

	virtual const char* GetDescription()
	{
		return "Sequence Change";
	}

	virtual void Undo(bool bUndo)
	{
		if (bUndo && m_pSequence && m_redo)
			m_pSequence->Serialize(m_redo, false, m_serializeFlags);
		if (m_pSequence && m_undo)
			m_pSequence->Serialize(m_undo, true, m_serializeFlags);
		if (m_pContext && bUndo)
			m_pContext->OnSequenceUndo(m_changeType);
	}

	virtual void Redo()
	{
		if (m_pSequence && m_redo)
			m_pSequence->Serialize(m_redo, true, m_serializeFlags);
		if (m_pContext)
			m_pContext->OnSequenceUndo(m_changeType);
	}

private:
	_smart_ptr<IFacialAnimSequence>          m_pSequence;
	IUndoSequenceChangeContext*              m_pContext;
	XmlNodeRef                               m_undo;
	XmlNodeRef                               m_redo;
	SequenceChangeType                       m_changeType;
	IFacialAnimSequence::ESerializationFlags m_serializeFlags;
};

//////////////////////////////////////////////////////////////////////////
class OverwriteConfirmer
{
public:
	OverwriteConfirmer(CWnd* parentWindow) : permanentDecision(PermanentDecisionNone), parentWindow(parentWindow) {}

	MergeCollisionAction AskConfirmation(const char* szFilename)
	{
		switch (permanentDecision)
		{
		case PermanentDecisionNone:
			{
			}
			break;

		case PermanentDecisionAllowAll:
			{
				return MergeCollisionActionOverwrite;
			}
			break;

		case PermanentDecisionAllowNone:
			{
				return MergeCollisionActionNoOverwrite;
			}
			break;

		default:
			assert(0);
			break;
		}

		string message;
		message.Format("Overwrite effector \"%s\"?", szFilename);
		COverwriteConfirmDialog dlg(parentWindow, message.c_str(), "Effector Already Exists");
		switch (dlg.DoModal())
		{
		case IDYES:
			return MergeCollisionActionOverwrite;
		case IDNO:
			return MergeCollisionActionNoOverwrite;
		case ID_YES_ALL:
			permanentDecision = PermanentDecisionAllowAll;
			return MergeCollisionActionOverwrite;
		case ID_NO_ALL:
			permanentDecision = PermanentDecisionAllowNone;
			return MergeCollisionActionNoOverwrite;
		}

		return MergeCollisionActionNoOverwrite;
	}

private:
	enum PermanentDecision
	{
		PermanentDecisionNone,
		PermanentDecisionAllowAll,
		PermanentDecisionAllowNone
	};

	PermanentDecision permanentDecision;

	CWnd*             parentWindow;
};

CFacialEdContext::CFacialEdContext()
{
	pCharacter = 0;
	pModel = 0;
	pInstance = 0;
	pLibrary = 0;
	pSelectedEffector = 0;
	pSelectedChannel = 0;
	pFaceState = 0;
	m_pSlidersPreviewEffector = 0;

	bLibraryModfied = false;
	bProjectModfied = false;
	bSequenceModfied = false;
	bJoysticksModfied = false;
	m_bPlaying = false;
	m_bSupressEvents = false;

	m_fSequenceTime = 0;

	m_pCurrentSequence = GetISystem()->GetIAnimationSystem()->GetIFacialAnimation()->CreateSequence();
	m_pJoysticks = GetISystem()->GetIAnimationSystem()->GetIFacialAnimation()->GetJoystickContext()->CreateJoystickSet();

	expressionClipboardFormat = RegisterClipboardFormat("FacialEditorExpression");
	channelClipboardFormat = RegisterClipboardFormat("FacialEditorChannel");

	m_bAnimateSkeleton = false;
	m_bAnimateCamera = true;
	m_bOverlapSounds = false;
	m_fPreviewWeight = 1.0f;

	REGISTER_CVAR(m_fC3DScale, 100.0f, VF_NULL, "Scale from C3D to our Joystick system");
}

//////////////////////////////////////////////////////////////////////////
CFacialEdContext::~CFacialEdContext()
{
	SAFE_RELEASE(pCharacter);
	SAFE_RELEASE(pLibrary);
}

//////////////////////////////////////////////////////////////////////////
void CFacialEdContext::SetCharacter(ICharacterInstance* pChar)
{
	pFaceState = 0;
	if (!pChar)
		return;

	IFacialInstance* pIFacialInstance = pChar->GetFacialInstance();
	if (pIFacialInstance == 0)
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, _T("Character has no facial Instance. Probably because there are no Facial Expressions in the chrparams file"));
		return;
	}
	if (pCharacter)
		pCharacter->Release();

	pCharacter = pChar;
	if (pCharacter)
		pCharacter->AddRef();
	pInstance = pChar->GetFacialInstance();
	pModel = pInstance->GetFacialModel();

	if (pModel)
	{
		IFacialEffectorsLibrary* pLib = pModel->GetLibrary();
		if (!pLib)
		{
			pLib = GetISystem()->GetIAnimationSystem()->GetIFacialAnimation()->CreateEffectorsLibrary();
		}
		SetLibrary(pLib);
	}

	if (pInstance)
	{
		pFaceState = pInstance->GetFaceState();

		pInstance->PlaySequence(m_pCurrentSequence, eFacialSequenceLayer_Preview);
		pInstance->PauseSequence(eFacialSequenceLayer_Preview, true);
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialEdContext::LoadCharacter(const CString& filename)
{
	ICharacterInstance* pChar = GetISystem()->GetIAnimationSystem()->CreateInstance(filename);
	if (!pChar)
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, _T("Failed to load character %s"), (const char*)filename);
		return;
	}
	SetCharacter(pChar);
}

//////////////////////////////////////////////////////////////////////////
void CFacialEdContext::RegisterListener(IFacialEdListener* pListener)
{
	stl::push_back_unique(m_listeners, pListener);
}

//////////////////////////////////////////////////////////////////////////
void CFacialEdContext::UnregisterListner(IFacialEdListener* pListener)
{
	stl::find_and_erase(m_listeners, pListener);
}

//////////////////////////////////////////////////////////////////////////
void CFacialEdContext::SetModified(IFacialEffector* pEffector)
{
	if (pEffector)
	{
		assert(pEffector);
		pEffector->SetFlags(pEffector->GetFlags() | EFE_FLAG_UI_MODIFIED);
		SendEvent(EFD_EVENT_CHANGE, pEffector);
	}
	bLibraryModfied = true;
}

//////////////////////////////////////////////////////////////////////////
void CFacialEdContext::SendEvent(EFacialEdEvent event, IFacialEffector* pEffector, int nNumChannels, IFacialAnimChannel** ppChannels)
{
	if (m_bSupressEvents)
		return;
	for (int i = 0; i < (int)m_listeners.size(); i++)
	{
		m_listeners[i]->OnFacialEdEvent(event, pEffector, nNumChannels, ppChannels);
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialEdContext::SelectEffector(IFacialEffector* pEffector)
{
	if (pSelectedEffector != pEffector)
	{
		pSelectedEffector = pEffector;
		SendEvent(EFD_EVENT_SELECT_EFFECTOR, pSelectedEffector);

		if (pSelectedEffector && pInstance && pSelectedEffector->GetType() != EFE_TYPE_GROUP)
		{
			if (m_pCurrentSequence)
				pInstance->StopSequence(eFacialSequenceLayer_Preview);
			pInstance->PreviewEffector(pSelectedEffector, 1.0f);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialEdContext::SelectChannel(IFacialAnimChannel* pChannel)
{
	if (pSelectedChannel != pChannel)
	{
		assert(pChannel != reinterpret_cast<IFacialAnimChannel*>(static_cast<uintptr_t>(0xCCCCCCCC)));
		assert(pChannel != reinterpret_cast<IFacialAnimChannel*>(static_cast<uintptr_t>(0xCDCDCDCD)));
		assert(pChannel != reinterpret_cast<IFacialAnimChannel*>(static_cast<uintptr_t>(0xFEFEFEFE)));
		pSelectedChannel = pChannel;
		SendEvent(EFD_EVENT_SELECT_CHANNEL, 0);
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialEdContext::ClearHighlightedChannels()
{
	m_highlightedChannels.clear();
}

//////////////////////////////////////////////////////////////////////////
void CFacialEdContext::AddHighlightedChannel(IFacialAnimChannel* pSelectedChannel)
{
	m_highlightedChannels.insert(pSelectedChannel);
}

//////////////////////////////////////////////////////////////////////////
void CFacialEdContext::SetFromSliders(std::vector<float>& weights, std::vector<float>& balances)
{
	if (!pInstance)
		return;

	if (m_pCurrentSequence)
		pInstance->StopSequence(eFacialSequenceLayer_Preview);
	if (m_pSlidersPreviewEffector)
	{
		static std::vector<IFacialEffector*> effectors;
		effectors.resize(0);
		GetAllEffectors(effectors, 0, EFE_TYPE_MORPH_TARGET);

		m_pSlidersPreviewEffector->RemoveAllSubEffectors();
		m_weights = weights;
		for (int i = 0; i < weights.size(); i++)
		{
			float fWeight = weights[i];
			if (fabs(fWeight) > 0.01f)
			{
				if (i >= effectors.size())
					break;

				IFacialEffector* pSubEff = effectors[i];
				IFacialEffCtrl* pCtrl = m_pSlidersPreviewEffector->AddSubEffector(pSubEff);
				pCtrl->SetConstantWeight(fWeight);
				pCtrl->SetConstantBalance(balances[i]);
			}
		}
		pInstance->PreviewEffector(m_pSlidersPreviewEffector, 1);
	}
	SendEvent(EFD_EVENT_SLIDERS_CHANGE, 0);
}

//////////////////////////////////////////////////////////////////////////
void CFacialEdContext::PreviewEffector(IFacialEffector* pEffector, float fWeight)
{
	if (pInstance)
	{
		if (m_pCurrentSequence)
			pInstance->StopSequence(eFacialSequenceLayer_Preview);
		pInstance->PreviewEffector(pEffector, fWeight);
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialEdContext::StopPreviewingEffector()
{
	PreviewEffector(0, 0.0f);
	if (pInstance)
	{
		if (!pInstance->IsPlaySequence(m_pCurrentSequence, eFacialSequenceLayer_Preview))
		{
			pInstance->PlaySequence(m_pCurrentSequence, eFacialSequenceLayer_Preview, true);
			pInstance->PauseSequence(eFacialSequenceLayer_Preview, true);
		}
		pInstance->SeekSequence(eFacialSequenceLayer_Preview, m_fSequenceTime);
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialEdContext::BindJoysticks()
{
	const char* joystickFile = (m_pJoysticks ? m_pJoysticks->GetName() : 0);
	if (m_pCurrentSequence && joystickFile && joystickFile[0])
		m_pCurrentSequence->SetJoystickFile(joystickFile);
	GetISystem()->GetIAnimationSystem()->GetIFacialAnimation()->BindJoystickSetToSequence(m_pJoysticks, m_pCurrentSequence);
}

//////////////////////////////////////////////////////////////////////////
void CFacialEdContext::CopyExpressionNameToClipboard(IFacialEffector* pEffector)
{
	if (!pEffector)
		return;

	const char* szEffectName(pEffector->GetName());
	if (!szEffectName)
	{
		assert(false && "Null name facial effector name pointer in CFacialEdContext::CopyExpressionNameToClipboard.");
		return;
	}

	size_t nMemLenght(strlen(szEffectName) + 1);

	HGLOBAL hMem(::GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE | GMEM_ZEROINIT, nMemLenght));
	if (!hMem)
	{
		assert(false && "Failed to allocate global memory in CFacialEdContext::CopyExpressionNameToClipboard.");
		return;
	}

	LPVOID pMem(::GlobalLock(hMem));
	if (!pMem)
	{
		assert(false && "Failed to lock global memory in CFacialEdContext::CopyExpressionNameToClipboard.");
		::GlobalFree(hMem);
		return;
	}

	memcpy(pMem, szEffectName, nMemLenght);

	BOOL bStillLocked(::GlobalUnlock(hMem));
	if (bStillLocked)
	{
		::GlobalFree(hMem);
		assert(false && "Failed to unlock global memory in CFacialEdContext::CopyExpressionNameToClipboard.");
		return;
	}

	BOOL bCliboardOpen(::OpenClipboard(NULL));
	if (!bCliboardOpen)
	{
		::GlobalFree(hMem);
		assert(false && "Failed to open clipboad in CFacialEdContext::CopyExpressionNameToClipboard.");
		return;
	}

	HANDLE hClipoard(::SetClipboardData(expressionClipboardFormat, hMem));
	if (!hClipoard)
	{
		// If SetClipboard data is sucessfull, this is not needed anymore.
		::GlobalFree(hMem);
		assert(false && "Failed to add data to the clipboad in CFacialEdContext::CopyExpressionNameToClipboard.");
		// The lack of a return here is intended, as it would be needed to close the clipboard regardless.
	}

	BOOL bClosed(::CloseClipboard());
	if (bClosed == FALSE)
	{
		assert(false && "Failed to close clipboad in CFacialEdContext::CopyExpressionNameToClipboard.");
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialEdContext::PasteExpressionFromClipboard()
{
	if (!pLibrary || !pSelectedEffector)
		return;

	IFacialEffector* pEffector = pLibrary->Find(GetExpressionNameFromClipboard());
	if (pEffector)
	{
		if (pEffector == pSelectedEffector)
		{

			CQuestionDialog::SWarning(QObject::tr(""), QObject::tr("You can't copy an item and paste it as its own sub-item."));
			return;
		}

		CUndo undo("Paste Expression");
		StoreLibraryUndo();

		FacialExpressionUtils::RemoveFromGarbage(pLibrary, pEffector);
		pSelectedEffector->AddSubEffector(pEffector);
		SendEvent(EFD_EVENT_ADD, pEffector);
		SetModified(pEffector);
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialEdContext::DoChannelDragDrop(IFacialAnimChannel* pChannel)
{
	COleDataSource* dataSource = CreateChannelDescriptorDataSource(pChannel);

	if (dataSource)
	{
		DROPEFFECT dropEffect = dataSource->DoDragDrop(DROPEFFECT_MOVE | DROPEFFECT_LINK);
		//dataSource->InternalRelease();
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialEdContext::HandleDropToSequence(IFacialAnimChannel* pParentChannel, COleDataObject* pDataObject)
{
	if (GetChannelDescriptorFromDataSource(pDataObject).size())
		HandleChannelDropToSequence(pParentChannel, pDataObject);
	else if (GetExpressionNameFromDataSource(pDataObject).size())
		HandleExpressionNameDropToSequence(pParentChannel, pDataObject);
}

//////////////////////////////////////////////////////////////////////////
void CFacialEdContext::HandleChannelDropToSequence(IFacialAnimChannel* pParentChannel, COleDataObject* pDataObject)
{
	if (!m_pCurrentSequence || !pDataObject)
		return;

	IFacialAnimChannel* pChannel = GetChannelFromDataSource(pDataObject);

	// Make sure we don't add the channel as a child of itself.
	for (IFacialAnimChannel* pAncestor = pParentChannel; pChannel && pAncestor; pAncestor = pAncestor->GetParent())
	{
		if (pAncestor == pChannel)
			pChannel = 0;
	}

	IFacialAnimChannel* pExistingChannel = 0;
	if (pChannel)
	{
		// Check whether the channel will replace an existing one.
		string potentialDescriptor = GetDescriptorForChannel(pParentChannel) + pChannel->GetName() + "::";
		pExistingChannel = GetChannelFromDescriptor(potentialDescriptor);
	}

	if (pExistingChannel)
	{
		if (QDialogButtonBox::StandardButton::No == CQuestionDialog::SQuestion(QObject::tr(""), QObject::tr("This folder contains a channel with the same name - do you want to overwrite it?")))
		{
			pChannel = 0;
		}
	}

	if (pChannel)
	{
		CUndo undo("Drag n Drop Channel");
		StoreSequenceUndo();

		if (pExistingChannel)
			m_pCurrentSequence->RemoveChannel(pExistingChannel);

		pChannel->SetParent(pParentChannel);
		SendEvent(EFD_EVENT_SEQUENCE_CHANGE);
		bSequenceModfied = true;
	}
}

//////////////////////////////////////////////////////////////////////////
IFacialAnimChannel* CFacialEdContext::GetChannelFromDataSource(COleDataObject* pDataObject)
{
	string descriptor = GetChannelDescriptorFromDataSource(pDataObject);
	return GetChannelFromDescriptor(descriptor);
}

//////////////////////////////////////////////////////////////////////////
IFacialAnimChannel* CFacialEdContext::GetChannelFromDescriptor(const string& descriptor)
{
	IFacialAnimChannel* pChannel = 0;
	for (int i = 0, end = m_pCurrentSequence->GetChannelCount(); i < end; ++i)
	{
		IFacialAnimChannel* p = m_pCurrentSequence->GetChannel(i);
		if (GetDescriptorForChannel(p) == descriptor)
			pChannel = p;
	}

	return pChannel;
}

//////////////////////////////////////////////////////////////////////////
void CFacialEdContext::HandleExpressionNameDropToSequence(IFacialAnimChannel* pParentChannel, COleDataObject* pDataObject)
{
	if (!m_pCurrentSequence || !pLibrary || !pDataObject)
		return;

	string expression = GetExpressionNameFromDataSource(pDataObject);
	IFacialEffector* pExpression = pLibrary->Find(expression.c_str());

	if (pExpression)
	{
		CUndo undo("Drag n Drop Expression");
		StoreSequenceUndo();

		IFacialAnimChannel* pChannel = m_pCurrentSequence->CreateChannel();
		pChannel->SetEffector(pExpression);
		pChannel->SetParent(pParentChannel);
		SendEvent(EFD_EVENT_SEQUENCE_CHANGE);
		bSequenceModfied = true;
	}
}

//////////////////////////////////////////////////////////////////////////
string CFacialEdContext::GetChannelDescriptorFromDataSource(COleDataObject* dataObject)
{
	string channelDescriptor;
	HGLOBAL hData = dataObject->GetGlobalData(channelClipboardFormat);
	if (hData != NULL)
	{
		channelDescriptor = static_cast<char*>(GlobalLock(hData));
		GlobalUnlock(hData);
	}

	return channelDescriptor;
}

//////////////////////////////////////////////////////////////////////////
COleDataSource* CFacialEdContext::CreateChannelDescriptorDataSource(IFacialAnimChannel* pChannel)
{
	if (pChannel)
	{
		CSharedFile sf(GMEM_MOVEABLE | GMEM_DDESHARE | GMEM_ZEROINIT);
		CString text = GetDescriptorForChannel(pChannel);
		sf.Write(text, text.GetLength() + 1);
		HGLOBAL hMem = sf.Detach();
		if (!hMem)
			return 0;

		COleDataSource* dataSource = new COleDataSource();
		dataSource->CacheGlobalData(channelClipboardFormat, hMem);

		return dataSource;
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////
string CFacialEdContext::GetDescriptorForChannel(IFacialAnimChannel* pChannel)
{
	string fullName;
	IFacialAnimChannel* pAncestor = pChannel;
	while (pAncestor)
	{
		const char* szChannelName = pAncestor ? pAncestor->GetName() : 0;
		szChannelName = szChannelName ? szChannelName : "";
		fullName = string(szChannelName) + "::" + fullName;
		pAncestor = pAncestor->GetParent();
	}

	return fullName;
}

//////////////////////////////////////////////////////////////////////////
void CFacialEdContext::DoExpressionNameDragDrop(IFacialEffector* pExpression, IFacialEffector* pParentExpression)
{
	COleDataSource* dataSource = CreateExpressionNameDataSource(pExpression);

	if (dataSource)
	{
		DROPEFFECT dropEffect = dataSource->DoDragDrop(DROPEFFECT_COPY /* | DROPEFFECT_MOVE*/);
		//dataSource->InternalRelease();

		if (pParentExpression && (dropEffect & DROPEFFECT_MOVE))
		{
			CUndo undo("Drag n Drop Expression");
			StoreLibraryUndo();

			pParentExpression->RemoveSubEffector(pExpression);
			SendEvent(EFD_EVENT_REMOVE, pExpression);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialEdContext::HandleExpressionNameDropToLibrary(IFacialEffector* pParentExpression, COleDataObject* pDataObject, const bool isToGarbageFolder)
{
	if (!pLibrary || !pParentExpression || !pDataObject)
		return;

	IFacialEffector* pEffector = pLibrary->Find(GetExpressionNameFromDataSource(pDataObject));
	if (pEffector && pEffector != pParentExpression)
	{
		CUndo undo("Drag n Drop Expression");
		StoreLibraryUndo();

		if (!isToGarbageFolder) // Should not remove from Garbage if we drop it to the garbage folder
			FacialExpressionUtils::RemoveFromGarbage(pLibrary, pEffector);

		pParentExpression->AddSubEffector(pEffector);
		SendEvent(EFD_EVENT_ADD, pEffector);
		SetModified(pEffector);
	}
}

//////////////////////////////////////////////////////////////////////////
COleDataSource* CFacialEdContext::CreateExpressionNameDataSource(IFacialEffector* pEffector)
{
	if (pEffector)
	{
		CSharedFile sf(GMEM_MOVEABLE | GMEM_DDESHARE | GMEM_ZEROINIT);
		CString text = pEffector->GetName();
		sf.Write(text, text.GetLength() + 1);
		HGLOBAL hMem = sf.Detach();
		if (!hMem)
			return 0;

		COleDataSource* dataSource = new COleDataSource();
		dataSource->CacheGlobalData(expressionClipboardFormat, hMem);

		return dataSource;
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////
string CFacialEdContext::GetExpressionNameFromClipboard()
{
	if (!pLibrary)
		return "";

	COleDataObject clipboardData;
	clipboardData.AttachClipboard();
	return GetExpressionNameFromDataSource(&clipboardData);
}

//////////////////////////////////////////////////////////////////////////
string CFacialEdContext::GetExpressionNameFromDataSource(COleDataObject* dataObject)
{
	string expressionName;
	HGLOBAL hData = dataObject->GetGlobalData(expressionClipboardFormat);
	if (hData != NULL)
	{
		expressionName = static_cast<char*>(GlobalLock(hData));
		GlobalUnlock(hData);
	}

	return expressionName;
}

//////////////////////////////////////////////////////////////////////////
class PoseExpressionEntry
{
public:
	IFacialEffector* pEffector;
	float            fPoseValue;
};
void CFacialEdContext::SetPoseFromExpression(IFacialEffector* pEffector, float weight, float time)
{
	CUndo undo("Set Pose From Expression");
	StoreSequenceUndo();

	// Create a map of expressions to the value that expression should have, based on the
	// sub-effectors in the given expression.
	typedef VectorMap<string, PoseExpressionEntry, stl::less_stricmp<string>> PoseExpressionMap;
	PoseExpressionMap poseExpressionMap;
	{
		PoseExpressionMap::container_type poseExpressionEntries;

		int subEffectorCount = (pEffector ? pEffector->GetSubEffectorCount() : 0);
		poseExpressionEntries.reserve(subEffectorCount);
		for (int subEffectorIndex = 0; subEffectorIndex < subEffectorCount; ++subEffectorIndex)
		{
			IFacialEffCtrl* pControl = (pEffector ? pEffector->GetSubEffCtrl(subEffectorIndex) : 0);
			float value = (pControl ? pControl->Evaluate(weight) : 0.0f);
			IFacialEffector* pSubEffector = (pControl ? pControl->GetEffector() : 0);
			if (pSubEffector)
			{
				PoseExpressionEntry entry = { pSubEffector, value };
				poseExpressionEntries.push_back(std::make_pair(pSubEffector->GetName(), entry));
			}
		}

		poseExpressionMap.SwapElementsWithVector(poseExpressionEntries);
	}

	float timeThreshold = max(0.001f, (m_pCurrentSequence ? (m_pCurrentSequence->GetTimeRange().end - m_pCurrentSequence->GetTimeRange().start) / 1000.0f : 0.1f));

	// Loop through all the channels in the sequence.
	int channelCount = (m_pCurrentSequence ? m_pCurrentSequence->GetChannelCount() : 0);
	for (int channelIndex = 0; channelIndex < channelCount; ++channelIndex)
	{
		IFacialAnimChannel* pChannel = (m_pCurrentSequence ? m_pCurrentSequence->GetChannel(channelIndex) : 0);
		const unsigned nonExpressionChannelMask =
		  IFacialAnimChannel::FLAG_GROUP |
		  IFacialAnimChannel::FLAG_PHONEME_STRENGTH |
		  IFacialAnimChannel::FLAG_VERTEX_DRAG |
		  IFacialAnimChannel::FLAG_BALANCE |
		  IFacialAnimChannel::FLAG_CATEGORY_BALANCE |
		  IFacialAnimChannel::FLAG_PROCEDURAL_STRENGTH |
		  IFacialAnimChannel::FLAG_LIPSYNC_CATEGORY_STRENGTH;
		if (pChannel && (pChannel->GetFlags() & nonExpressionChannelMask) == 0)
		{
			const char* effectorName = (pChannel ? pChannel->GetEffectorName() : 0);
			PoseExpressionMap::iterator itPoseExpression = poseExpressionMap.end();
			if (effectorName)
				itPoseExpression = poseExpressionMap.find(effectorName);
			ISplineInterpolator* pSpline = (pChannel ? pChannel->GetInterpolator(0) : 0);
			if (pSpline && itPoseExpression != poseExpressionMap.end())
			{
				pSpline->RemoveKeysInRange(time - timeThreshold, time + timeThreshold);
				pSpline->InsertKeyFloat(FacialEditorSnapTimeToFrame(time), (*itPoseExpression).second.fPoseValue);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
float CFacialEdContext::GetTimelineLength()
{
	IFacialAnimSequence* pSequence = GetSequence();
	float fSequenceLength = (pSequence ? pSequence->GetTimeRange().end : 0.0f);
	return fSequenceLength;
}

//////////////////////////////////////////////////////////////////////////
void CFacialEdContext::GetFromSliders(std::vector<float>& weights, std::vector<float>& balances)
{
	weights = m_weights;
	balances = m_balances;
}

//////////////////////////////////////////////////////////////////////////
void CFacialEdContext::UpdateSelectedFromSliders()
{
	static std::vector<IFacialEffector*> effectors;
	effectors.resize(0);
	GetAllEffectors(effectors, 0, EFE_TYPE_MORPH_TARGET);

	int nMaxEffectors = (int)effectors.size();

	if (pSelectedEffector && pSelectedEffector->GetType() == EFE_TYPE_EXPRESSION)
	{
		pSelectedEffector->RemoveAllSubEffectors(); //???
		IFacialEffector* pCurrent = pSelectedEffector;

		for (int i = 0; i < m_weights.size(); i++)
		{
			if (i >= nMaxEffectors)
				break;
			IFacialEffector* pEffector = pModel->GetEffector(i);

			float fWeight = m_weights[i];
			if (fabs(fWeight) > 0.0001f)
			{
				IFacialEffector* pLibEffector = effectors[i];
				if (pLibEffector)
				{
					pCurrent->AddSubEffector(pLibEffector);
					int nLast = pCurrent->GetSubEffectorCount() - 1;
					IFacialEffCtrl* pController = pCurrent->GetSubEffCtrl(nLast);
					pController->SetConstantWeight(fWeight);
				}
			}
		}

		SelectEffector(NULL);
		SetModified(pCurrent);
		SelectEffector(pCurrent);
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialEdContext::UpdateSelectedFromSequenceTime(float minimumWeight)
{
	// Create an expression from the current blended pose in the sequence
	if (pSelectedEffector && pSelectedEffector->GetType() == EFE_TYPE_EXPRESSION)
	{
		// Remove everything that is already there - the effector will be completely wiped and reinitialized
		pSelectedEffector->RemoveAllSubEffectors();
		IFacialEffector* pCurrent = pSelectedEffector;

		// Go through all the channels in the sequence
		// find the non-zero expressions and add the to the expression (excluding the current one of course)
		int channelCount = (m_pCurrentSequence ? m_pCurrentSequence->GetChannelCount() : 0);
		for (int channelIndex = 0; channelIndex < channelCount; ++channelIndex)
		{
			IFacialAnimChannel* pChannel = (m_pCurrentSequence ? m_pCurrentSequence->GetChannel(channelIndex) : 0);
			const unsigned nonExpressionChannelMask =
			  IFacialAnimChannel::FLAG_GROUP |
			  IFacialAnimChannel::FLAG_PHONEME_STRENGTH |
			  IFacialAnimChannel::FLAG_VERTEX_DRAG |
			  IFacialAnimChannel::FLAG_BALANCE |
			  IFacialAnimChannel::FLAG_CATEGORY_BALANCE |
			  IFacialAnimChannel::FLAG_PROCEDURAL_STRENGTH |
			  IFacialAnimChannel::FLAG_LIPSYNC_CATEGORY_STRENGTH;
			if (pChannel && (pChannel->GetFlags() & nonExpressionChannelMask) == 0)
			{
				// Get the expression
				// Add it with the current weights to the new expression
				//    if weight is non zero
				//    if expression is not the one we are currently initializing

				const char* channelName = pChannel->GetName();
				float channelWeight = 0.0f;
				pChannel->GetInterpolator(0)->InterpolateFloat(m_fSequenceTime, channelWeight);

				if (fabs(channelWeight) > minimumWeight)
				{
					IFacialEffector* pLibEffector = GetLibrary()->Find(channelName);
					if (pLibEffector && (stricmp(pCurrent->GetName(), pLibEffector->GetName()) != 0))
					{
						// go through all sub effectors of this effector and add them as sub effectors to this one
						// if they already exist, simply add the weights together
						int subEffectorCount = pLibEffector->GetSubEffectorCount();
						if (subEffectorCount != 0)
						{
							for (int subEffector = 0; subEffector < subEffectorCount; ++subEffector)
							{
								IFacialEffector* pSubEffector = pLibEffector->GetSubEffector(subEffector);
								const char* subEffName = pSubEffector->GetName();
								float effectorWeight = pLibEffector->GetSubEffCtrl(subEffector)->GetConstantWeight();
								float bal = pLibEffector->GetSubEffCtrl(subEffector)->GetConstantBalance();
								float mergedWeight = channelWeight * effectorWeight;
								if (fabs(mergedWeight) < minimumWeight)
									continue;

								// does this effector already exist in this expression? (if so, only add to the weight)
								IFacialEffCtrl* pEffCtrl = pCurrent->GetSubEffCtrlByName(pSubEffector->GetName());
								if (pEffCtrl)
								{
									pEffCtrl->SetConstantWeight(pEffCtrl->GetConstantWeight() + mergedWeight);
								}
								else
								{
									pCurrent->AddSubEffector(pSubEffector);
									int nLast = pCurrent->GetSubEffectorCount() - 1;
									IFacialEffCtrl* pController = pCurrent->GetSubEffCtrl(nLast);
									pController->SetConstantWeight(mergedWeight);
								}
							}
						}
						else
						{
							// not an expression with sub effectors, but a direct effector (bone or morph)
							pCurrent->AddSubEffector(pLibEffector);
							int nLast = pCurrent->GetSubEffectorCount() - 1;
							IFacialEffCtrl* pController = pCurrent->GetSubEffCtrl(nLast);
							pController->SetConstantWeight(channelWeight);
						}
					}
				}

			} // end of for loop over all channels in the sequence

		} // end of for loop over all channels in the sequence

		SelectEffector(NULL);
		SetModified(pCurrent);
		SelectEffector(pCurrent);
	}
}

//////////////////////////////////////////////////////////////////////////
bool CFacialEdContext::LoadProject(const CString& filename)
{
	projectFile = PathUtil::ToUnixPath(filename.GetString()).c_str();

	SelectEffector(NULL);

	XmlNodeRef root = XmlHelpers::LoadXmlFromFile(projectFile);
	if (!root)
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, _T("Failed to load facial project %s"), (const char*)projectFile);
		return false;
	}
	XmlNodeRef nodeLibrary = root->findChild("ExpressionsLibrary");
	if (nodeLibrary)
	{
		bLibraryModfied = false;
		const char* str = nodeLibrary->getAttr("File");
		SAFE_RELEASE(pLibrary);
		IFacialEffectorsLibrary* pLib = GetISystem()->GetIAnimationSystem()->GetIFacialAnimation()->LoadEffectorsLibrary(str);
		if (!pLib)
		{
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, _T("Failed to load expressions library %s"), str);
			return false;
		}
		SetLibrary(pLib);
	}
	XmlNodeRef nodeCharacter = root->findChild("Character");
	if (nodeCharacter)
	{
		const char* str = nodeCharacter->getAttr("File");
		ICharacterInstance* pChar = GetISystem()->GetIAnimationSystem()->CreateInstance(str);
		if (!pChar)
		{
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, _T("Failed to load character %s"), str);
			return false;
		}
		SetCharacter(pChar);
	}

	XmlNodeRef nodeSequence = root->findChild("Sequence");
	if (nodeSequence)
	{
		const char* str = nodeSequence->getAttr("File");

		if (!str)
		{
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, _T("Failed to load Sequence"), str);
			return false;
		}

		LoadSequence(str);
	}

	XmlNodeRef nodeJoysticks = root->findChild("Joysticks");
	if (nodeJoysticks)
	{
		const char* str = nodeJoysticks->getAttr("File");

		if (!str)
		{
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, _T("Failed to load Joysticks"), str);
			return false;
		}

		LoadJoystickSet(str);
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CFacialEdContext::SaveProject(const CString& filename)
{
	projectFile = PathUtil::ToUnixPath(filename.GetString()).c_str();

	XmlNodeRef root = XmlHelpers::CreateXmlNode("FacialEditorProject");
	if (pLibrary)
	{
		XmlNodeRef nodeLibrary = root->newChild("ExpressionsLibrary");
		nodeLibrary->setAttr("File", pLibrary->GetName());
	}

	if (pCharacter)
	{
		XmlNodeRef nodeCharacter = root->newChild("Character");
		nodeCharacter->setAttr("File", pCharacter->GetFilePath());
	}

	if (GetSequence())
	{
		XmlNodeRef nodeCharacter = root->newChild("Sequence");
		nodeCharacter->setAttr("File", GetSequence()->GetName());
	}

	if (GetJoystickSet())
	{
		XmlNodeRef nodeJoysticks = root->newChild("Joysticks");
		nodeJoysticks->setAttr("File", GetJoystickSet()->GetName());
	}

	if (XmlHelpers::SaveXmlNode(root, projectFile))
	{
		bProjectModfied = false;
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CFacialEdContext::NewLibrary()
{
	bLibraryModfied = false;
	SAFE_RELEASE(pLibrary);
	SetLibrary(GetISystem()->GetIAnimationSystem()->GetIFacialAnimation()->CreateEffectorsLibrary());
}

//////////////////////////////////////////////////////////////////////////
bool CFacialEdContext::LoadLibrary(const CString& filename)
{
	CString file = PathUtil::ToUnixPath(filename.GetString()).c_str();

	SelectEffector(NULL);

	bLibraryModfied = false;
	SAFE_RELEASE(pLibrary);

	ISystem* pSystem = GetISystem();
	ICharacterManager* pCharacterManager = (pSystem ? pSystem->GetIAnimationSystem() : 0);
	IFacialAnimation* pFacialAnimation = (pCharacterManager ? pCharacterManager->GetIFacialAnimation() : 0);
	if (pFacialAnimation)
		pFacialAnimation->ClearEffectorsLibraryFromCache(filename);
	IFacialEffectorsLibrary* pLib = (pFacialAnimation ? pFacialAnimation->LoadEffectorsLibrary(file) : 0);
	if (!pLib)
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, _T("Failed to load expressions library %s"), (const char*)file);
		return false;
	}
	SetLibrary(pLib);

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CFacialEdContext::ImportLibrary(const CString& filename, CWnd* parentWindow)
{
	CString file = PathUtil::ToUnixPath(filename.GetString()).c_str();

	SelectEffector(NULL);

	bLibraryModfied = true;

	_smart_ptr<IFacialEffectorsLibrary> pImportLibrary = GetISystem()->GetIAnimationSystem()->GetIFacialAnimation()->LoadEffectorsLibrary(file);
	if (!pImportLibrary)
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, _T("Failed to load expressions library %s"), (const char*)file);
		return false;
	}

	if (!ImportLibrary(pImportLibrary, parentWindow))
		return false;

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CFacialEdContext::ImportLibrary(IFacialEffectorsLibrary* pImportLibrary, CWnd* parentWindow)
{
	pLibrary->MergeLibrary(pImportLibrary, functor_ret(OverwriteConfirmer(parentWindow), &OverwriteConfirmer::AskConfirmation));
	SetLibrary(pLibrary);

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CFacialEdContext::SaveLibrary(const CString& filename, IFacialEffectorsLibrary* pLibrary)
{
	if (pLibrary == 0)
		pLibrary = this->pLibrary;

	string file = PathUtil::ToUnixPath(filename.GetString());

	if (pLibrary)
	{
		pLibrary->SetName(file.c_str());
		XmlNodeRef root = XmlHelpers::CreateXmlNode("ExpressionLibrary");
		pLibrary->Serialize(root, false);
		if (XmlHelpers::SaveXmlNode(root, PathUtil::GamePathToCryPakPath(file)))
		{
			bLibraryModfied = false;
			return true;
		}
		return false;
	}
	bLibraryModfied = false;
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CFacialEdContext::StoreLibraryUndo()
{
	if (CUndo::IsRecording() && pLibrary)
		CUndo::Record(new CUndoLibraryChange(pLibrary, this));
}

//////////////////////////////////////////////////////////////////////////
void CFacialEdContext::SetLibrary(IFacialEffectorsLibrary* pLib)
{
	if (pLibrary != pLib)
	{
		pLibrary = pLib;
		pLibrary->AddRef();

		m_pSlidersPreviewEffector = pLibrary->CreateEffector(EFE_TYPE_EXPRESSION, "Preview");
		m_pSlidersPreviewEffector->SetFlags(m_pSlidersPreviewEffector->GetFlags() | EFE_FLAG_UI_PREVIEW);
	}

	if (pModel)
		pModel->AssignLibrary(pLibrary);
	if (pInstance)
		pInstance->OnExpressionLibraryLoad();

	SendEvent(EFD_EVENT_LIBRARY_CHANGE, 0);
}

//////////////////////////////////////////////////////////////////////////
IFacialEffectorsLibrary* CFacialEdContext::GetLibrary()
{
	IFacialEffectorsLibrary* pLibrary = 0;
	if (pModel)
		pLibrary = pModel->GetLibrary();
	return pLibrary;
}

//////////////////////////////////////////////////////////////////////////
void CFacialEdContext::NewSequence()
{
	while (GetVideoFrameReader().GetAVICount())
		GetVideoFrameReader().DeleteAVI(0);
	GetVideoFrameReader().DeleteUnusedAVIs();

	bLibraryModfied = false;
	m_pCurrentSequence = GetISystem()->GetIAnimationSystem()->GetIFacialAnimation()->CreateSequence();
	SetSequence(m_pCurrentSequence);
}

//////////////////////////////////////////////////////////////////////////
bool CFacialEdContext::LoadSequence(const CString& filename)
{
	while (GetVideoFrameReader().GetAVICount())
		GetVideoFrameReader().DeleteAVI(0);
	GetVideoFrameReader().DeleteUnusedAVIs();

	ISystem* pSystem = GetISystem();
	ICharacterManager* pCharacterManager = (pSystem ? pSystem->GetIAnimationSystem() : 0);
	IFacialAnimation* pFacialAnimation = (pCharacterManager ? pCharacterManager->GetIFacialAnimation() : 0);
	if (pFacialAnimation)
		pFacialAnimation->ClearSequenceFromCache(filename);
	_smart_ptr<IFacialAnimSequence> seq = (pFacialAnimation ? pFacialAnimation->LoadSequence(filename) : 0);
	if (seq)
	{
		//ConvertSequenceToCorrectFrameRate(seq);
		SetSequence(seq);
		SendEvent(EFD_EVENT_SEQUENCE_LOAD, 0);
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CFacialEdContext::ImportSequence(const CString& filename, CWnd* parentWindow)
{
	CString file = PathUtil::ToUnixPath(filename.GetString()).c_str();

	SelectEffector(NULL);

	bSequenceModfied = true;

	_smart_ptr<IFacialAnimSequence> pImportSequence = GetISystem()->GetIAnimationSystem()->GetIFacialAnimation()->LoadSequence(file);
	if (!pImportSequence)
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, _T("Failed to load sequence %s"), (const char*)file);
		return false;
	}

	if (!ImportSequence(pImportSequence, parentWindow))
		return false;

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CFacialEdContext::ImportSequence(IFacialAnimSequence* pImportSequence, CWnd* parentWindow)
{
	m_pCurrentSequence->MergeSequence(pImportSequence, functor_ret(OverwriteConfirmer(parentWindow), &OverwriteConfirmer::AskConfirmation));
	SetSequence(m_pCurrentSequence);
	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CFacialEdContext::SaveSequence(const CString& filename, IFacialAnimSequence* pSequence)
{
	bool undirtySequence = false;
	if (pSequence == 0)
	{
		pSequence = this->m_pCurrentSequence;
		undirtySequence = true;
	}

	CString file = PathUtil::ToUnixPath(filename.GetString()).c_str();

	if (pSequence)
	{
		pSequence->SetName(file);
		XmlNodeRef root = XmlHelpers::CreateXmlNode("FacialSequence");
		pSequence->Serialize(root, false);
		if (XmlHelpers::SaveXmlNode(root, PathUtil::GamePathToCryPakPath(file.GetString())))
		{
			if (undirtySequence)
				bSequenceModfied = false;
			return true;
		}
		return false;
	}
	if (undirtySequence)
		bSequenceModfied = false;
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CFacialEdContext::StoreSequenceUndo(SequenceChangeType changeType)
{
	if (CUndo::IsRecording() && m_pCurrentSequence)
		CUndo::Record(new CUndoSequenceChange(m_pCurrentSequence, this, changeType));
}

//////////////////////////////////////////////////////////////////////////
void CFacialEdContext::NewJoystickSet()
{
	bLibraryModfied = false;
	_smart_ptr<IJoystickSet> pJoysticks = GetISystem()->GetIAnimationSystem()->GetIFacialAnimation()->GetJoystickContext()->CreateJoystickSet();
	SetJoystickSet(pJoysticks);
}

//////////////////////////////////////////////////////////////////////////
bool CFacialEdContext::LoadJoystickSet(const CString& filename)
{
	_smart_ptr<IJoystickSet> pJoysticks = GetISystem()->GetIAnimationSystem()->GetIFacialAnimation()->GetJoystickContext()->LoadJoystickSet(filename);
	if (pJoysticks)
	{
		pJoysticks->SetName(filename);
		SetJoystickSet(pJoysticks);
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CFacialEdContext::SaveJoystickSet(const CString& filename)
{
	string file = PathUtil::ToUnixPath(filename.GetString());

	if (m_pJoysticks)
	{
		m_pJoysticks->SetName(filename);

		XmlNodeRef root = XmlHelpers::CreateXmlNode("Joysticks");
		m_pJoysticks->Serialize(root, false);
		if (XmlHelpers::SaveXmlNode(root, PathUtil::GamePathToCryPakPath(file)))
		{
			bJoysticksModfied = false;
			return true;
		}
		return false;
	}
	bJoysticksModfied = false;
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CFacialEdContext::SetJoystickSet(IJoystickSet* pJoystickSet)
{
	m_pJoysticks = pJoystickSet;
	BindJoysticks();
	bSequenceModfied = true;
	SendEvent(EFD_EVENT_JOYSTICK_SET_CHANGED, 0);
}

//////////////////////////////////////////////////////////////////////////
IJoystickSet* CFacialEdContext::GetJoystickSet()
{
	return m_pJoysticks;
}

//////////////////////////////////////////////////////////////////////////
void CFacialEdContext::SetSequence(IFacialAnimSequence* pSequence)
{
	m_pCurrentSequence = pSequence;
	BindJoysticks();
	if (pInstance)
	{
		pInstance->PlaySequence(m_pCurrentSequence, eFacialSequenceLayer_Preview, true);
		pInstance->PauseSequence(eFacialSequenceLayer_Preview, true);
	}
	UpdateSkeletonAnimationStatus();

	SendEvent(EFD_EVENT_SEQUENCE_CHANGE, 0);
}

//////////////////////////////////////////////////////////////////////////
void CFacialEdContext::PlaySequence(bool bPlay)
{
	if (m_bPlaying != bPlay)
	{
		m_bPlaying = bPlay;

		UpdateSkeletonAnimationStatus();

		SendEvent(EFD_EVENT_SEQUENCE_PLAY_OR_STOP, 0);
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialEdContext::SetSequenceTime(float fTime)
{
	if (!pInstance)
		return;
	m_fSequenceTime = fTime;
	if (m_pCurrentSequence != NULL && pInstance != NULL)
	{
		if (!pInstance->IsPlaySequence(m_pCurrentSequence, eFacialSequenceLayer_Preview))
		{
			pInstance->PlaySequence(m_pCurrentSequence, eFacialSequenceLayer_Preview, true);
			pInstance->PauseSequence(eFacialSequenceLayer_Preview, true);
		}
		pInstance->SeekSequence(eFacialSequenceLayer_Preview, m_fSequenceTime);
	}

	UpdateSkeletonAnimationStatus();

	pInstance->PreviewEffector(NULL, 0);
	SendEvent(EFD_EVENT_SEQUENCE_TIME, 0);
}

//////////////////////////////////////////////////////////////////////////
void CFacialEdContext::SetSkeletonAnimation(const CString& filename, float startTime)
{
	if (m_pCurrentSequence)
	{
		CUndo undo("Load Skeleton Animation File");
		StoreSequenceUndo();

		while (m_pCurrentSequence && m_pCurrentSequence->GetSkeletonAnimationEntryCount() > 0)
			m_pCurrentSequence->DeleteSkeletonAnimationEntry(0);

		int skeletonAnimationEntry = (m_pCurrentSequence ? m_pCurrentSequence->GetSkeletonAnimationEntryCount() : 0);
		m_pCurrentSequence->InsertSkeletonAnimationEntry(skeletonAnimationEntry);
		IFacialAnimSkeletonAnimationEntry* pEntry = m_pCurrentSequence->GetSkeletonAnimationEntry(skeletonAnimationEntry);
		if (pEntry && pCharacter)
		{
			pEntry->SetName(filename);
			pEntry->SetStartTime(startTime);

			IAnimationSet* pAnimSet = pCharacter->GetIAnimationSet();
			uint32 animID = pAnimSet->GetAnimIDByName(filename.GetString());
			float endTime =
			  startTime + pAnimSet->GetDuration_sec(animID);
			pEntry->SetEndTime(endTime);
		}

		UpdateSkeletonAnimationStatus();

		bSequenceModfied = true;
	}
	SendEvent(EFD_EVENT_SKELETON_ANIMATION_CHANGE, 0);
}

//////////////////////////////////////////////////////////////////////////
inline bool SortFacialEffectorsByNameLess(IFacialEffector* p1, IFacialEffector* p2)
{
	return stricmp(p1->GetName(), p2->GetName()) < 0;
}

//////////////////////////////////////////////////////////////////////////
static void RecursiveGetAllEffectors(std::vector<IFacialEffector*>& effectors, IFacialEffector* pRootEffector, EFacialEffectorType ofType)
{
	for (int i = 0; i < pRootEffector->GetSubEffectorCount(); i++)
	{
		IFacialEffector* pSubEffector = pRootEffector->GetSubEffector(i);
		if (pSubEffector->GetType() == EFE_TYPE_GROUP)
			RecursiveGetAllEffectors(effectors, pSubEffector, ofType);
		else if (pSubEffector->GetType() == ofType)
			stl::push_back_unique(effectors, pSubEffector);
		else if (pSubEffector->GetType() == EFE_TYPE_EXPRESSION)
			RecursiveGetAllEffectors(effectors, pSubEffector, ofType);
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialEdContext::GetAllEffectors(std::vector<IFacialEffector*>& effectors, IFacialEffector* pRootEffector, EFacialEffectorType ofType)
{
	if (!pLibrary)
		return;

	if (!pRootEffector)
		pRootEffector = pLibrary->GetRoot();
	if (!pRootEffector)
		return;

	RecursiveGetAllEffectors(effectors, pRootEffector, ofType);
	std::sort(effectors.begin(), effectors.end(), SortFacialEffectorsByNameLess);
}

typedef VectorSet<string, stl::less_stricmp<string>> MorphSet;
class MorphCheckExpressionHandler
{
public:
	MorphCheckExpressionHandler(const MorphSet& morphSet, CFacialEdContext::IMorphCheckHandler* handler)
		: m_morphSet(morphSet),
		m_handler(handler)
	{
	}

	void operator()(IFacialEffector* pEffector)
	{
		if (pEffector && pEffector->GetType() == EFE_TYPE_MORPH_TARGET)
		{
			const char* effectorName = (pEffector ? pEffector->GetName() : 0);
			if (effectorName && m_morphSet.find(effectorName) == m_morphSet.end())
				m_handler->HandleMorphError(effectorName);
		}
	}

private:
	const MorphSet&                       m_morphSet;
	CFacialEdContext::IMorphCheckHandler* m_handler;
};

//////////////////////////////////////////////////////////////////////////
void CFacialEdContext::CheckMorphs(IMorphCheckHandler* handler)
{
	MorphSet morphSet;
	{
		MorphSet::container_type morphSetElements;
		IFacialInstance* pFacialInstance = (pCharacter ? pCharacter->GetFacialInstance() : 0);
		IFacialModel* pFacialModel = (pFacialInstance ? pFacialInstance->GetFacialModel() : 0);
		morphSetElements.reserve(pFacialModel ? pFacialModel->GetMorphTargetCount() : 0);
		for (int morphTargetIndex = 0; pFacialModel && morphTargetIndex < pFacialModel->GetMorphTargetCount(); ++morphTargetIndex)
			morphSetElements.push_back(pFacialModel->GetMorphTargetName(morphTargetIndex));
		morphSet.SwapElementsWithVector(morphSetElements);
	}

	ForEachEffector(MorphCheckExpressionHandler(morphSet, handler));
}

//////////////////////////////////////////////////////////////////////////
template<typename H> void CFacialEdContext::ForEachEffectorRecurse(H& handler, IFacialEffector* pEffector, EffectorSet& visitedEffectors)
{
	if (pEffector && visitedEffectors.insert(pEffector).second)
	{
		handler(pEffector);
		for (int subEffectorIndex = 0; pEffector && subEffectorIndex < pEffector->GetSubEffectorCount(); ++subEffectorIndex)
			ForEachEffectorRecurse(handler, pEffector->GetSubEffector(subEffectorIndex), visitedEffectors);
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialEdContext::OnLibraryUndo()
{
	SendEvent(EFD_EVENT_LIBRARY_UNDO, 0);
}

//////////////////////////////////////////////////////////////////////////
void CFacialEdContext::OnSequenceUndo(SequenceChangeType changeType)
{
	switch (changeType)
	{
	case FE_SEQ_CHANGE_TOTAL:
		SendEvent(EFD_EVENT_SEQUENCE_UNDO, 0);
		break;
	case FE_SEQ_CHANGE_SOUND_TIMES:
		SendEvent(EFD_EVENT_SEQUENCE_TIMES_CHANGE, 0);
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
template<typename H> void CFacialEdContext::ForEachEffector(H& handler)
{
	EffectorSet visitedEffectors;
	if (pLibrary)
		ForEachEffectorRecurse(handler, pLibrary->GetRoot(), visitedEffectors);
}

//////////////////////////////////////////////////////////////////////////
void CFacialEdContext::MoveToKey(MoveToKeyDirection direction)
{
	float currentTime = m_fSequenceTime;

	float previousKeyTime = 0.0f;
	float nextKeyTime = (m_pCurrentSequence ? m_pCurrentSequence->GetTimeRange().end : 0.0f);
	for (int channelIndex = 0, end = (m_pCurrentSequence ? m_pCurrentSequence->GetChannelCount() : 0); channelIndex < end; ++channelIndex)
	{
		IFacialAnimChannel* pChannel = (m_pCurrentSequence ? m_pCurrentSequence->GetChannel(channelIndex) : 0);
		if (m_highlightedChannels.find(pChannel) != m_highlightedChannels.end())
		{
			ISplineInterpolator* pSpline = (pChannel ? pChannel->GetLastInterpolator() : 0);
			for (int key = 0; pSpline && key < pSpline->GetKeyCount(); ++key)
			{
				float time = (pSpline ? pSpline->GetKeyTime(key) : 0);
				if (time > previousKeyTime && time < currentTime - 0.02f)
					previousKeyTime = time;
				if (time < nextKeyTime && time > currentTime + 0.02f)
					nextKeyTime = time;
			}
		}
	}

	float newTime = currentTime;
	switch (direction)
	{
	case MoveToKeyDirectionForward:
		newTime = nextKeyTime;
		break;
	case MoveToKeyDirectionBackward:
		newTime = previousKeyTime;
		break;
	}

	newTime = FacialEditorSnapTimeToFrame(newTime);
	if (newTime <= GetTimelineLength())
		SetSequenceTime(newTime);
}

//////////////////////////////////////////////////////////////////////////
void CFacialEdContext::MoveToFrame(MoveToKeyDirection direction)
{
	float diff = 0.0f;
	switch (direction)
	{
	case MoveToKeyDirectionForward:
		diff = 1.0f;
		break;
	case MoveToKeyDirectionBackward:
		diff = -1.0f;
		break;
	}
	float newTime = FacialEditorSnapTimeToFrame(m_fSequenceTime + diff / FACIAL_EDITOR_FPS);
	if (newTime <= GetTimelineLength())
		SetSequenceTime(newTime);
}

//////////////////////////////////////////////////////////////////////////
void CFacialEdContext::SetAnimateSkeleton(bool bAnimateSkeleton)
{
	if (m_bAnimateSkeleton != bAnimateSkeleton)
		m_bAnimateSkeleton = bAnimateSkeleton;
	UpdateSkeletonAnimationStatus();
	SendEvent(EFD_EVENT_ANIMATE_SKELETON_CHANGED, 0, 0, 0);
}

//////////////////////////////////////////////////////////////////////////
bool CFacialEdContext::GetAnimateSkeleton() const
{
	return m_bAnimateSkeleton;
}

//////////////////////////////////////////////////////////////////////////
void CFacialEdContext::SetAnimateCamera(bool bAnimateCamera)
{
	m_bAnimateCamera = bAnimateCamera;
	SendEvent(EFD_EVENT_ANIMATE_CAMERA_CHANGED, 0, 0, 0);
}

//////////////////////////////////////////////////////////////////////////
bool CFacialEdContext::GetAnimateCamera() const
{
	return m_bAnimateCamera;
}

//////////////////////////////////////////////////////////////////////////
void CFacialEdContext::SetOverlapSounds(bool bSetOverlapSounds)
{
	m_bOverlapSounds = bSetOverlapSounds;
}

//////////////////////////////////////////////////////////////////////////
bool CFacialEdContext::GetOverlapSounds() const
{
	return m_bOverlapSounds;
}

//////////////////////////////////////////////////////////////////////////
void CFacialEdContext::SetPreviewWeight(float fPreviewWeight)
{
	m_fPreviewWeight = fPreviewWeight;
}

//////////////////////////////////////////////////////////////////////////
float CFacialEdContext::GetPreviewWeight() const
{
	return m_fPreviewWeight;
}

//////////////////////////////////////////////////////////////////////////
void CFacialEdContext::ConvertSequenceToCorrectFrameRate(IFacialAnimSequence* pSequence)
{
	for (int channelIndex = 0, channelCount = (pSequence ? pSequence->GetChannelCount() : 0); channelIndex < channelCount; ++channelIndex)
	{
		IFacialAnimChannel* pChannel = (pSequence ? pSequence->GetChannel(channelIndex) : 0);
		ISplineInterpolator* pSpline = (pChannel && pChannel->GetInterpolatorCount() ? pChannel->GetInterpolator(0) : 0);

		bool requiresConversion = false;
		for (int keyIndex = 0, keyCount = (pSpline ? pSpline->GetKeyCount() : 0); keyIndex < keyCount; ++keyIndex)
		{
			float time = (pSpline ? pSpline->GetKeyTime(keyIndex) : 0);
			float frame = time * FACIAL_EDITOR_FPS;
			if (fabs(frame - floor(frame + 0.5f)) > 0.015f)
				requiresConversion = true;
		}

		if (requiresConversion)
		{
			int oldKeyCount = (pSpline ? pSpline->GetKeyCount() : 0);
			float length = (oldKeyCount && pSpline ? pSpline->GetKeyTime(pSpline->GetKeyCount() - 1) : 0.0f);
			std::vector<float> keys(length * FACIAL_EDITOR_FPS);
			for (int frameIndex = 0, frameCount = keys.size(); frameIndex < frameCount; ++frameIndex)
			{
				if (pSpline)
					pSpline->InterpolateFloat(float(frameIndex) / FACIAL_EDITOR_FPS, keys[frameIndex]);
			}

			for (int keyIndex = 0, keyCount = (pSpline ? pSpline->GetKeyCount() : 0); keyIndex < keyCount; ++keyIndex)
				pSpline->RemoveKey(0);

			for (int frameIndex = 0, frameCount = keys.size(); frameIndex < frameCount; ++frameIndex)
			{
				if (pSpline)
					pSpline->InsertKeyFloat(float(frameIndex) / FACIAL_EDITOR_FPS, keys[frameIndex]);
				pSpline->InterpolateFloat(frameIndex / FACIAL_EDITOR_FPS, keys[frameIndex]);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialEdContext::UpdateSkeletonAnimationStatus()
{
	if (pCharacter)
	{
		AnimUtils::StopAnimations(pCharacter);

		// Find the sequence to play.
		int skeletonAnimationToPlayIndex = -1;
		for (int skeletonAnimationIndex = 0, skeletonAnimationCount = (m_pCurrentSequence ? m_pCurrentSequence->GetSkeletonAnimationEntryCount() : 0); skeletonAnimationIndex < skeletonAnimationCount; ++skeletonAnimationIndex)
		{
			IFacialAnimSkeletonAnimationEntry* pEntry = (m_pCurrentSequence ? m_pCurrentSequence->GetSkeletonAnimationEntry(skeletonAnimationIndex) : 0);
			float animationStartTime = (pEntry ? pEntry->GetStartTime() : 0.0f);
			if (animationStartTime <= m_fSequenceTime)
				skeletonAnimationToPlayIndex = skeletonAnimationIndex;
		}

		IFacialAnimSkeletonAnimationEntry* pEntry = (m_pCurrentSequence && skeletonAnimationToPlayIndex >= 0 ? m_pCurrentSequence->GetSkeletonAnimationEntry(skeletonAnimationToPlayIndex) : 0);
		const char* animationFile = 0;
		if (m_bAnimateSkeleton)
			animationFile = (pEntry ? pEntry->GetName() : 0);
		if (!animationFile || !*animationFile)
			animationFile = "null";
		float fSequenceStartTime = pEntry ? pEntry->GetStartTime() : 0;
		float fSequenceEndTime = pEntry ? pEntry->GetEndTime() : 0;
		float fDuration = fSequenceEndTime - fSequenceStartTime;

		if (animationFile && *animationFile && pCharacter && (fDuration > 0.f))
		{
			AnimUtils::StartAnimation(pCharacter, animationFile);
			AnimUtils::SetAnimationTime(pCharacter, max(0.0f, m_fSequenceTime - fSequenceStartTime) / fDuration);
		}
	}
}

CFacialVideoFrameReader::CFacialVideoFrameReader()
	: m_lastLoadedIndex(-1)
{
}

CFacialVideoFrameReader::~CFacialVideoFrameReader()
{
	while (GetAVICount())
		DeleteAVI(0);
	DeleteUnusedAVIs();
	assert(m_avis.empty());
}

int CFacialVideoFrameReader::AddAVI(const char* filename, float time)
{
	// Check whether the avi is already loaded.
	AVIMap::iterator itAVI = m_avis.find(filename);
	if (itAVI == m_avis.end())
	{
		IAVI_Reader* pAVI = GetISystem()->CreateAVIReader();
		bool loadedFile = (pAVI ? pAVI->OpenFile(filename) : false);
		if (!loadedFile && pAVI)
		{
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Facial Editor: Unable to load video file: \"%s\".", (filename ? filename : "<MISSING FILENAME>"));
			GetISystem()->ReleaseAVIReader((*itAVI).second.pAVI);
		}
		if (pAVI && loadedFile)
		{
			itAVI = m_avis.insert(std::make_pair(filename, AVIEntry())).first;
			(*itAVI).second.pAVI = pAVI;
		}
	}

	int index = -1;
	if (itAVI != m_avis.end())
	{
		index = int(m_aviEntities.size());
		m_aviEntities.resize(m_aviEntities.size() + 1);
		m_aviEntities[index].itAVI = itAVI;
		m_aviEntities[index].time = time;
		++(*itAVI).second.refCount;
		m_lastLoadedIndex = index;
	}

	return index;
}

void CFacialVideoFrameReader::DeleteAVI(int index)
{
	if (index < 0 || index >= int(m_avis.size()))
	{
		CryLogAlways("CFacialVideoFrameReader::DeleteAVI(): Attempting to delete an AVI entry that doesn't exist.");
	}
	else
	{
		--(*m_aviEntities[index].itAVI).second.refCount;
		m_aviEntities.erase(m_aviEntities.begin() + index);

		if (m_lastLoadedIndex == index)
			m_lastLoadedIndex = -1;
		else if (m_lastLoadedIndex > index)
			--m_lastLoadedIndex;
	}
}

void CFacialVideoFrameReader::DeleteUnusedAVIs()
{
	for (AVIMap::iterator itAVI = m_avis.begin(), itEnd = m_avis.end(); itAVI != itEnd; )
	{
		if ((*itAVI).second.refCount <= 0)
		{
			GetISystem()->ReleaseAVIReader((*itAVI).second.pAVI);
			m_avis.erase(itAVI++);
		}
		else
		{
			++itAVI;
		}
	}
}

int CFacialVideoFrameReader::GetLastLoadedAVIFrameFromTime(float time)
{
	if (m_lastLoadedIndex == -1)
		return -1;

	IAVI_Reader* pCurrentAVI = (*m_aviEntities[m_lastLoadedIndex].itAVI).second.pAVI;
	int fps = (pCurrentAVI ? pCurrentAVI->GetFPS() : 1);
	int frameCount = (pCurrentAVI ? pCurrentAVI->GetFrameCount() : 0);
	int frameIndex = int(floor((time - m_aviEntities[m_lastLoadedIndex].time) * fps));
	if (pCurrentAVI && frameIndex >= 0 && frameIndex < frameCount)
		return frameIndex;
	return -1;
}

int CFacialVideoFrameReader::GetAVICount() const
{
	return int(m_aviEntities.size());
}

int CFacialVideoFrameReader::GetWidth(float time)
{
	IAVI_Reader* pAVI = 0;
	int frame = -1;
	FindAVIFrame(time, pAVI, frame);
	if (pAVI && frame >= 0 && frame < pAVI->GetFrameCount())
		return pAVI->GetWidth();
	return -1;
}

int CFacialVideoFrameReader::GetHeight(float time)
{
	IAVI_Reader* pAVI = 0;
	int frame = -1;
	FindAVIFrame(time, pAVI, frame);
	if (pAVI && frame >= 0 && frame < pAVI->GetFrameCount())
		return pAVI->GetHeight();
	return -1;
}

const unsigned char* CFacialVideoFrameReader::GetFrame(float time)
{
	IAVI_Reader* pAVI = 0;
	int frame = -1;
	FindAVIFrame(time, pAVI, frame);
	if (pAVI && frame >= 0 && frame < pAVI->GetFrameCount())
		return pAVI->QueryFrame(frame);
	return 0;
}

void CFacialVideoFrameReader::FindAVIFrame(float time, IAVI_Reader*& pAVI, int& frame)
{
	pAVI = 0;
	frame = -1;
	for (int entityIndex = 0, entityCount = (m_aviEntities.size()); entityIndex < entityCount; ++entityIndex)
	{
		IAVI_Reader* pCurrentAVI = (*m_aviEntities[entityIndex].itAVI).second.pAVI;
		int fps = (pCurrentAVI ? pCurrentAVI->GetFPS() : 1);
		int frameCount = (pCurrentAVI ? pCurrentAVI->GetFrameCount() : 0);
		int frameIndex = int(floor((time - m_aviEntities[entityIndex].time) * fps));
		if (pCurrentAVI && frameIndex >= 0 && frameIndex < frameCount)
		{
			pAVI = pCurrentAVI;
			frame = frameIndex;
		}
	}
}

