// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "Impl.h"

#include "CVars.h"
#include "Listener.h"
#include "Object.h"
#include "AisacControl.h"
#include "AisacControlAdvanced.h"
#include "AisacEnvironment.h"
#include "AisacEnvironmentAdvanced.h"
#include "AisacState.h"
#include "Binary.h"
#include "Category.h"
#include "CategoryAdvanced.h"
#include "CategoryState.h"
#include "Cue.h"
#include "CueInstance.h"
#include "DspBus.h"
#include "GameVariable.h"
#include "GameVariableAdvanced.h"
#include "GameVariableState.h"
#include "SelectorLabel.h"
#include "Snapshot.h"
#include "DspBusSetting.h"
#include "IoInterface.h"

#include <FileInfo.h>
#include <CrySystem/IStreamEngine.h>
#include <CrySystem/File/ICryPak.h>
#include <CrySystem/IProjectManager.h>
#include <CryAudio/IAudioSystem.h>

#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
	#include <Logger.h>
	#include <DebugStyle.h>
	#include <CrySystem/ITimer.h>
#endif  // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE

#if CRY_PLATFORM_WINDOWS
	#include <cri_atom_wasapi.h>
#elif CRY_PLATFORM_DURANGO
	#include <cri_atom_xboxone.h>
#elif CRY_PLATFORM_ORBIS
	#include <cri_atom_ps4.h>
#elif CRY_PLATFORM_LINUX
	#include <cri_atom_linux.h>
#endif

namespace CryAudio
{
namespace Impl
{
namespace Adx2
{
SPoolSizes g_poolSizes;
std::map<ContextId, SPoolSizes> g_contextPoolSizes;

constexpr uint32 g_cueSynthCueAttribId = StringToId("CriMw.CriAtomCraft.AcCore.AcOoCueSynthCue");
constexpr uint32 g_cueFolderAttribId = StringToId("CriMw.CriAtomCraft.AcCore.AcOoCueFolder");
constexpr uint32 g_cueFolderPrivateAttribId = StringToId("CriMw.CriAtomCraft.AcCore.AcOoCueFolderPrivate");

#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
SPoolSizes g_debugPoolSizes;
CueInstances g_constructedCueInstances;
uint16 g_objectPoolSize = 0;
uint16 g_cueInstancePoolSize = 0;
std::vector<CListener*> g_constructedListeners;
#endif  // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE

//////////////////////////////////////////////////////////////////////////
void CountPoolSizes(XmlNodeRef const& node, SPoolSizes& poolSizes)
{
	uint16 numCues = 0;
	node->getAttr(g_szCuesAttribute, numCues);
	poolSizes.cues += numCues;

	uint16 numAisacControls = 0;
	node->getAttr(g_szAisacControlsAttribute, numAisacControls);
	poolSizes.aisacControls += numAisacControls;

	uint16 numAisacControlsAdvanced = 0;
	node->getAttr(g_szAisacControlsAdvancedAttribute, numAisacControlsAdvanced);
	poolSizes.aisacControlsAdvanced += numAisacControlsAdvanced;

	uint16 numAisacEnvironments = 0;
	node->getAttr(g_szAisacEnvironmentsAttribute, numAisacEnvironments);
	poolSizes.aisacEnvironments += numAisacEnvironments;

	uint16 numAisacEnvironmentsAdvanced = 0;
	node->getAttr(g_szAisacEnvironmentsAdvancedAttribute, numAisacEnvironmentsAdvanced);
	poolSizes.aisacEnvironmentsAdvanced += numAisacEnvironmentsAdvanced;

	uint16 numAisacStates = 0;
	node->getAttr(g_szAisacStatesAttribute, numAisacStates);
	poolSizes.aisacStates += numAisacStates;

	uint16 numCategories = 0;
	node->getAttr(g_szCategoriesAttribute, numCategories);
	poolSizes.categories += numCategories;

	uint16 numCategoriesAdvanced = 0;
	node->getAttr(g_szCategoriesAdvancedAttribute, numCategoriesAdvanced);
	poolSizes.categoriesAdvanced += numCategoriesAdvanced;

	uint16 numCategoryStates = 0;
	node->getAttr(g_szCategoryStatesAttribute, numCategoryStates);
	poolSizes.categoryStates += numCategoryStates;

	uint16 numGameVariables = 0;
	node->getAttr(g_szGameVariablesAttribute, numGameVariables);
	poolSizes.gameVariables += numGameVariables;

	uint16 numGameVariablesAdvanced = 0;
	node->getAttr(g_szGameVariablesAdvancedAttribute, numGameVariablesAdvanced);
	poolSizes.gameVariablesAdvanced += numGameVariablesAdvanced;

	uint16 numGameVariableStates = 0;
	node->getAttr(g_szGameVariableStatesAttribute, numGameVariableStates);
	poolSizes.gameVariableStates += numGameVariableStates;

	uint16 numSelectorLabels = 0;
	node->getAttr(g_szSelectorLabelsAttribute, numSelectorLabels);
	poolSizes.selectorLabels += numSelectorLabels;

	uint16 numBuses = 0;
	node->getAttr(g_szDspBusesAttribute, numBuses);
	poolSizes.dspBuses += numBuses;

	uint16 numSnapshots = 0;
	node->getAttr(g_szSnapshotsAttribute, numSnapshots);
	poolSizes.snapshots += numSnapshots;

	uint16 numDspBusSettings = 0;
	node->getAttr(g_szDspBusSettingsAttribute, numDspBusSettings);
	poolSizes.dspBusSettings += numDspBusSettings;

	uint16 numBinaries = 0;
	node->getAttr(g_szBinariesAttribute, numBinaries);
	poolSizes.binaries += numBinaries;
}

//////////////////////////////////////////////////////////////////////////
void AllocateMemoryPools(uint16 const objectPoolSize, uint16 const cueInstancePoolSize)
{
	CObject::CreateAllocator(objectPoolSize);
	CCueInstance::CreateAllocator(cueInstancePoolSize);
	CCue::CreateAllocator(g_poolSizes.cues);
	CAisacControl::CreateAllocator(g_poolSizes.aisacControls);
	CAisacControlAdvanced::CreateAllocator(g_poolSizes.aisacControlsAdvanced);
	CAisacEnvironment::CreateAllocator(g_poolSizes.aisacEnvironments);
	CAisacEnvironmentAdvanced::CreateAllocator(g_poolSizes.aisacEnvironmentsAdvanced);
	CAisacState::CreateAllocator(g_poolSizes.aisacStates);
	CCategory::CreateAllocator(g_poolSizes.categories);
	CCategoryAdvanced::CreateAllocator(g_poolSizes.categoriesAdvanced);
	CCategoryState::CreateAllocator(g_poolSizes.categoryStates);
	CGameVariable::CreateAllocator(g_poolSizes.gameVariables);
	CGameVariableAdvanced::CreateAllocator(g_poolSizes.gameVariablesAdvanced);
	CGameVariableState::CreateAllocator(g_poolSizes.gameVariableStates);
	CSelectorLabel::CreateAllocator(g_poolSizes.selectorLabels);
	CDspBus::CreateAllocator(g_poolSizes.dspBuses);
	CSnapshot::CreateAllocator(g_poolSizes.snapshots);
	CDspBusSetting::CreateAllocator(g_poolSizes.dspBusSettings);
	CBinary::CreateAllocator(g_poolSizes.binaries);
}

//////////////////////////////////////////////////////////////////////////
void FreeMemoryPools()
{
	CObject::FreeMemoryPool();
	CCueInstance::FreeMemoryPool();
	CCue::FreeMemoryPool();
	CAisacControl::FreeMemoryPool();
	CAisacControlAdvanced::FreeMemoryPool();
	CAisacEnvironment::FreeMemoryPool();
	CAisacEnvironmentAdvanced::FreeMemoryPool();
	CAisacState::FreeMemoryPool();
	CCategory::FreeMemoryPool();
	CCategoryAdvanced::FreeMemoryPool();
	CCategoryState::FreeMemoryPool();
	CGameVariable::FreeMemoryPool();
	CGameVariableAdvanced::FreeMemoryPool();
	CGameVariableState::FreeMemoryPool();
	CSelectorLabel::FreeMemoryPool();
	CDspBus::FreeMemoryPool();
	CSnapshot::FreeMemoryPool();
	CDspBusSetting::FreeMemoryPool();
	CBinary::FreeMemoryPool();
}

#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
using CueInfo = std::map<uint32, std::vector<std::pair<uint32, float>>>;
CueInfo g_cueRadiusInfo;
CueInfo g_cueFadeOutTimes;

//////////////////////////////////////////////////////////////////////////
void ParseAcbInfoFile(XmlNodeRef const& rootNode, uint32 const acbId)
{
	int const numCueNodes = rootNode->getChildCount();

	for (int i = 0; i < numCueNodes; ++i)
	{
		XmlNodeRef const cueNode = rootNode->getChild(i);

		if (cueNode.isValid())
		{
			uint32 const typeAttribId = StringToId(cueNode->getAttr("OrcaType"));

			switch (typeAttribId)
			{
			case g_cueSynthCueAttribId:
				{
					uint32 const cueId = StringToId(cueNode->getAttr("OrcaName"));

					if (cueNode->haveAttr("Pos3dDistanceMax"))
					{
						float distanceMax = 0.0f;
						cueNode->getAttr("Pos3dDistanceMax", distanceMax);

						g_cueRadiusInfo[cueId].emplace_back(acbId, distanceMax);
					}

					int const numTrackNodes = cueNode->getChildCount();
					float cueFadeOutTime = 0.0f;

					for (int j = 0; j < numTrackNodes; ++j)
					{
						XmlNodeRef const trackNode = cueNode->getChild(j);

						if (trackNode.isValid())
						{
							int const numWaveNodes = trackNode->getChildCount();

							for (int k = 0; k < numWaveNodes; ++k)
							{
								XmlNodeRef const waveNode = trackNode->getChild(k);

								if (waveNode->haveAttr("EgReleaseTimeMs"))
								{
									float trackFadeOutTime = 0.0f;
									waveNode->getAttr("EgReleaseTimeMs", trackFadeOutTime);
									cueFadeOutTime = std::max(cueFadeOutTime, trackFadeOutTime);
								}
							}
						}
					}

					if (cueFadeOutTime > 0.0f)
					{
						g_cueFadeOutTimes[cueId].emplace_back(acbId, cueFadeOutTime / 1000.0f);
					}

					break;
				}
			case g_cueFolderAttribId: // Intentional fall-through.
			case g_cueFolderPrivateAttribId:
				{
					ParseAcbInfoFile(cueNode, acbId);
					break;
				}
			default:
				{
					break;
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void LoadAcbInfos(CryFixedStringT<MaxFilePathLength> const& folderPath)
{
	_finddata_t fd;
	ICryPak* const pCryPak = gEnv->pCryPak;
	intptr_t const handle = pCryPak->FindFirst(folderPath + "/*_acb_info.xml", &fd);

	if (handle != -1)
	{
		do
		{
			CryFixedStringT<MaxFilePathLength> const fileName = fd.name;
			XmlNodeRef const rootNode = GetISystem()->LoadXmlFromFile(folderPath + "/" + fileName);

			if (rootNode.isValid())
			{
				XmlNodeRef const infoNode = rootNode->getChild(0);

				if (infoNode.isValid())
				{
					XmlNodeRef const infoRootNode = infoNode->getChild(0);

					if (infoRootNode.isValid())
					{
						XmlNodeRef const acbNode = infoRootNode->getChild(0);

						if ((acbNode.isValid()) && acbNode->haveAttr("AwbHash"))
						{
							ParseAcbInfoFile(acbNode, StringToId(acbNode->getAttr("OrcaName")));
						}
					}
				}
			}
		}
		while (pCryPak->FindNext(handle, &fd) >= 0);

		pCryPak->FindClose(handle);
	}
}

//////////////////////////////////////////////////////////////////////////
static void errorCallback(Char8 const* const errid, Uint32 const p1, Uint32 const p2, Uint32* const parray)
{
	Char8 const* errorMessage;
	errorMessage = criErr_ConvertIdToMessage(errid, p1, p2);
	Cry::Audio::Log(ELogType::Error, static_cast<char const*>(errorMessage));
}
#endif // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE

//////////////////////////////////////////////////////////////////////////
CriError selectIoFunc(CriChar8 const* szPath, CriFsDeviceId* pDeviceId, CriFsIoInterfacePtr* pIoInterface)
{
	CRY_ASSERT_MESSAGE(szPath != nullptr, "szPath is null pointer during %s", __FUNCTION__);
	CRY_ASSERT_MESSAGE(pDeviceId != nullptr, "pDeviceId is null pointer during %s", __FUNCTION__);
	CRY_ASSERT_MESSAGE(pIoInterface != nullptr, "pIoInterface is null pointer during %s", __FUNCTION__);

	*pDeviceId = CRIFS_DEFAULT_DEVICE;
	*pIoInterface = &g_IoInterface;

	return CRIERR_OK;
}

//////////////////////////////////////////////////////////////////////////
void* userMalloc(void* const pObj, CriUint32 const size)
{
	MEMSTAT_CONTEXT(EMemStatContextType::AudioImpl, "CryAudio::Impl::Adx2::userMalloc");
	void* const pMem = CryModuleMalloc(static_cast<size_t>(size));
	return pMem;
}

//////////////////////////////////////////////////////////////////////////
void userFree(void* const pObj, void* const pMem)
{
	CryModuleFree(pMem);
}

///////////////////////////////////////////////////////////////////////////
CImpl::CImpl()
	: m_pAcfBuffer(nullptr)
	, m_dbasId(CRIATOMEXDBAS_ILLEGAL_ID)
#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
	, m_name("ADX2 - " CRI_ATOM_VER_NUM)
#endif  // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE
{
	g_pImpl = this;
}

///////////////////////////////////////////////////////////////////////////
bool CImpl::Init(uint16 const objectPoolSize)
{
	bool isInitialized = true;

	if (g_cvars.m_cuePoolSize < 1)
	{
		g_cvars.m_cuePoolSize = 1;

#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
		Cry::Audio::Log(ELogType::Warning, R"(Cue pool size must be at least 1. Forcing the cvar "s_Adx2CuePoolSize" to 1!)");
#endif    // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE
	}

	g_constructedObjects.reserve(static_cast<size_t>(objectPoolSize));
	AllocateMemoryPools(objectPoolSize, static_cast<uint16>(g_cvars.m_cuePoolSize));

	m_regularSoundBankFolder = CRY_AUDIO_DATA_ROOT;
	m_regularSoundBankFolder += "/";
	m_regularSoundBankFolder += g_szImplFolderName;
	m_regularSoundBankFolder += "/";
	m_regularSoundBankFolder += g_szAssetsFolderName;
	m_localizedSoundBankFolder = m_regularSoundBankFolder;

#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
	g_constructedCueInstances.reserve(static_cast<size_t>(g_cvars.m_cuePoolSize));

	g_objectPoolSize = objectPoolSize;
	g_cueInstancePoolSize = static_cast<uint16>(g_cvars.m_cuePoolSize);

	LoadAcbInfos(m_regularSoundBankFolder);

	if (ICVar* pCVar = gEnv->pConsole->GetCVar("g_languageAudio"))
	{
		SetLanguage(pCVar->GetString());
		LoadAcbInfos(m_localizedSoundBankFolder);
	}

	criErr_SetCallback(errorCallback);
#endif  // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE

	criAtomEx_SetUserAllocator(userMalloc, userFree, nullptr);

	InitializeFileSystem();

	if (InitializeLibrary() && AllocateVoicePool() && CreateDbas())
	{
		RegisterAcf();
		SetListenerConfig();
		SetPlayerConfig();
		Set3dSourceConfig();
	}
	else
	{
		ShutDown();
		isInitialized = false;
	}

	return isInitialized;
}

///////////////////////////////////////////////////////////////////////////
void CImpl::ShutDown()
{
	UnregisterAcf();
	criAtomExDbas_Destroy(m_dbasId);
	criAtomExVoicePool_FreeAll();

#if CRY_PLATFORM_WINDOWS
	criAtomEx_Finalize_WASAPI();
#elif CRY_PLATFORM_DURANGO
	criAtomEx_Finalize_XBOXONE();
#elif CRY_PLATFORM_ORBIS
	criAtomEx_Finalize_PS4();
#elif CRY_PLATFORM_LINUX
	criAtomEx_FinalizeForUserPcmOutput_LINUX();
#endif

	criFs_FinalizeLibrary();
}

///////////////////////////////////////////////////////////////////////////
void CImpl::Release()
{
	delete this;
	g_pImpl = nullptr;
	g_cvars.UnregisterVariables();

	FreeMemoryPools();
}

//////////////////////////////////////////////////////////////////////////
void CImpl::SetLibraryData(XmlNodeRef const& node, ContextId const contextId)
{
	if (contextId == GlobalContextId)
	{
		CountPoolSizes(node, g_poolSizes);
	}
	else
	{
		CountPoolSizes(node, g_contextPoolSizes[contextId]);
	}
}

//////////////////////////////////////////////////////////////////////////
void CImpl::OnBeforeLibraryDataChanged()
{
	ZeroStruct(g_poolSizes);
	g_contextPoolSizes.clear();

#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
	ZeroStruct(g_debugPoolSizes);
#endif  // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE
}

//////////////////////////////////////////////////////////////////////////
void CImpl::OnAfterLibraryDataChanged(int const poolAllocationMode)
{
	if (!g_contextPoolSizes.empty())
	{
		if (poolAllocationMode <= 0)
		{
			for (auto const& poolSizePair : g_contextPoolSizes)
			{
				SPoolSizes const& iterPoolSizes = g_contextPoolSizes[poolSizePair.first];

				g_poolSizes.cues += iterPoolSizes.cues;
				g_poolSizes.aisacControls += iterPoolSizes.aisacControls;
				g_poolSizes.aisacControlsAdvanced += iterPoolSizes.aisacControlsAdvanced;
				g_poolSizes.aisacEnvironments += iterPoolSizes.aisacEnvironments;
				g_poolSizes.aisacEnvironmentsAdvanced += iterPoolSizes.aisacEnvironmentsAdvanced;
				g_poolSizes.aisacStates += iterPoolSizes.aisacStates;
				g_poolSizes.categories += iterPoolSizes.categories;
				g_poolSizes.categoriesAdvanced += iterPoolSizes.categoriesAdvanced;
				g_poolSizes.categoryStates += iterPoolSizes.categoryStates;
				g_poolSizes.gameVariables += iterPoolSizes.gameVariables;
				g_poolSizes.gameVariablesAdvanced += iterPoolSizes.gameVariablesAdvanced;
				g_poolSizes.gameVariableStates += iterPoolSizes.gameVariableStates;
				g_poolSizes.selectorLabels += iterPoolSizes.selectorLabels;
				g_poolSizes.dspBuses += iterPoolSizes.dspBuses;
				g_poolSizes.snapshots += iterPoolSizes.snapshots;
				g_poolSizes.dspBusSettings += iterPoolSizes.dspBusSettings;
				g_poolSizes.binaries += iterPoolSizes.binaries;
			}
		}
		else
		{
			SPoolSizes maxContextPoolSizes;

			for (auto const& poolSizePair : g_contextPoolSizes)
			{
				SPoolSizes const& iterPoolSizes = g_contextPoolSizes[poolSizePair.first];

				maxContextPoolSizes.cues = std::max(maxContextPoolSizes.cues, iterPoolSizes.cues);
				maxContextPoolSizes.aisacControls = std::max(maxContextPoolSizes.aisacControls, iterPoolSizes.aisacControls);
				maxContextPoolSizes.aisacControlsAdvanced = std::max(maxContextPoolSizes.aisacControlsAdvanced, iterPoolSizes.aisacControlsAdvanced);
				maxContextPoolSizes.aisacEnvironments = std::max(maxContextPoolSizes.aisacEnvironments, iterPoolSizes.aisacEnvironments);
				maxContextPoolSizes.aisacEnvironmentsAdvanced = std::max(maxContextPoolSizes.aisacEnvironmentsAdvanced, iterPoolSizes.aisacEnvironmentsAdvanced);
				maxContextPoolSizes.aisacStates = std::max(maxContextPoolSizes.aisacStates, iterPoolSizes.aisacStates);
				maxContextPoolSizes.categories = std::max(maxContextPoolSizes.categories, iterPoolSizes.categories);
				maxContextPoolSizes.categoriesAdvanced = std::max(maxContextPoolSizes.categoriesAdvanced, iterPoolSizes.categoriesAdvanced);
				maxContextPoolSizes.categoryStates = std::max(maxContextPoolSizes.categoryStates, iterPoolSizes.categoryStates);
				maxContextPoolSizes.gameVariables = std::max(maxContextPoolSizes.gameVariables, iterPoolSizes.gameVariables);
				maxContextPoolSizes.gameVariablesAdvanced = std::max(maxContextPoolSizes.gameVariablesAdvanced, iterPoolSizes.gameVariablesAdvanced);
				maxContextPoolSizes.gameVariableStates = std::max(maxContextPoolSizes.gameVariableStates, iterPoolSizes.gameVariableStates);
				maxContextPoolSizes.selectorLabels = std::max(maxContextPoolSizes.selectorLabels, iterPoolSizes.selectorLabels);
				maxContextPoolSizes.dspBuses = std::max(maxContextPoolSizes.dspBuses, iterPoolSizes.dspBuses);
				maxContextPoolSizes.snapshots = std::max(maxContextPoolSizes.snapshots, iterPoolSizes.snapshots);
				maxContextPoolSizes.dspBusSettings = std::max(maxContextPoolSizes.dspBusSettings, iterPoolSizes.dspBusSettings);
				maxContextPoolSizes.binaries = std::max(maxContextPoolSizes.binaries, iterPoolSizes.binaries);
			}

			g_poolSizes.cues += maxContextPoolSizes.cues;
			g_poolSizes.aisacControls += maxContextPoolSizes.aisacControls;
			g_poolSizes.aisacControlsAdvanced += maxContextPoolSizes.aisacControlsAdvanced;
			g_poolSizes.aisacEnvironments += maxContextPoolSizes.aisacEnvironments;
			g_poolSizes.aisacEnvironmentsAdvanced += maxContextPoolSizes.aisacEnvironmentsAdvanced;
			g_poolSizes.aisacStates += maxContextPoolSizes.aisacStates;
			g_poolSizes.categories += maxContextPoolSizes.categories;
			g_poolSizes.categoriesAdvanced += maxContextPoolSizes.categoriesAdvanced;
			g_poolSizes.categoryStates += maxContextPoolSizes.categoryStates;
			g_poolSizes.gameVariables += maxContextPoolSizes.gameVariables;
			g_poolSizes.gameVariablesAdvanced += maxContextPoolSizes.gameVariablesAdvanced;
			g_poolSizes.gameVariableStates += maxContextPoolSizes.gameVariableStates;
			g_poolSizes.selectorLabels += maxContextPoolSizes.selectorLabels;
			g_poolSizes.dspBuses += maxContextPoolSizes.dspBuses;
			g_poolSizes.snapshots += maxContextPoolSizes.snapshots;
			g_poolSizes.dspBusSettings += maxContextPoolSizes.dspBusSettings;
			g_poolSizes.binaries += maxContextPoolSizes.binaries;
		}
	}

#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
	// Used to hide pools without allocations in debug draw.
	g_debugPoolSizes = g_poolSizes;
#endif  // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE

	g_poolSizes.cues = std::max<uint16>(1, g_poolSizes.cues);
	g_poolSizes.aisacControls = std::max<uint16>(1, g_poolSizes.aisacControls);
	g_poolSizes.aisacControlsAdvanced = std::max<uint16>(1, g_poolSizes.aisacControlsAdvanced);
	g_poolSizes.aisacEnvironments = std::max<uint16>(1, g_poolSizes.aisacEnvironments);
	g_poolSizes.aisacEnvironmentsAdvanced = std::max<uint16>(1, g_poolSizes.aisacEnvironmentsAdvanced);
	g_poolSizes.aisacStates = std::max<uint16>(1, g_poolSizes.aisacStates);
	g_poolSizes.categories = std::max<uint16>(1, g_poolSizes.categories);
	g_poolSizes.categoriesAdvanced = std::max<uint16>(1, g_poolSizes.categoriesAdvanced);
	g_poolSizes.categoryStates = std::max<uint16>(1, g_poolSizes.categoryStates);
	g_poolSizes.gameVariables = std::max<uint16>(1, g_poolSizes.gameVariables);
	g_poolSizes.gameVariablesAdvanced = std::max<uint16>(1, g_poolSizes.gameVariablesAdvanced);
	g_poolSizes.gameVariableStates = std::max<uint16>(1, g_poolSizes.gameVariableStates);
	g_poolSizes.selectorLabels = std::max<uint16>(1, g_poolSizes.selectorLabels);
	g_poolSizes.dspBuses = std::max<uint16>(1, g_poolSizes.dspBuses);
	g_poolSizes.snapshots = std::max<uint16>(1, g_poolSizes.snapshots);
	g_poolSizes.dspBusSettings = std::max<uint16>(1, g_poolSizes.dspBusSettings);
	g_poolSizes.binaries = std::max<uint16>(1, g_poolSizes.binaries);
}

///////////////////////////////////////////////////////////////////////////
void CImpl::OnLoseFocus()
{
	MuteAllObjects(CRI_TRUE);
}

///////////////////////////////////////////////////////////////////////////
void CImpl::OnGetFocus()
{
	MuteAllObjects(CRI_FALSE);
}

///////////////////////////////////////////////////////////////////////////
void CImpl::MuteAll()
{
	MuteAllObjects(CRI_TRUE);
}

///////////////////////////////////////////////////////////////////////////
void CImpl::UnmuteAll()
{
	MuteAllObjects(CRI_FALSE);
}

///////////////////////////////////////////////////////////////////////////
void CImpl::PauseAll()
{
	PauseAllObjects(CRI_TRUE);
}

///////////////////////////////////////////////////////////////////////////
void CImpl::ResumeAll()
{
	PauseAllObjects(CRI_FALSE);
}

///////////////////////////////////////////////////////////////////////////
void CImpl::StopAllSounds()
{
	criAtomExPlayer_StopAllPlayers();
}

//////////////////////////////////////////////////////////////////////////
void CImpl::RegisterInMemoryFile(SFileInfo* const pFileInfo)
{
	if (pFileInfo != nullptr)
	{
		auto const pFileData = static_cast<CBinary*>(pFileInfo->pImplData);

		if (pFileData != nullptr)
		{
			CryFixedStringT<MaxFilePathLength> acbToAwbPath(pFileInfo->filePath);
			PathUtil::RemoveExtension(acbToAwbPath);
			acbToAwbPath += ".awb";

			CriChar8 const* const awbPath = (gEnv->pCryPak->IsFileExist(acbToAwbPath.c_str())) ? static_cast<CriChar8 const*>(acbToAwbPath.c_str()) : nullptr;

			pFileData->pAcb = criAtomExAcb_LoadAcbData(pFileInfo->pFileData, static_cast<CriSint32>(pFileInfo->size), nullptr, awbPath, nullptr, 0);

			if (pFileData->pAcb != nullptr)
			{
				CryFixedStringT<MaxFileNameLength> name(pFileInfo->fileName);
				PathUtil::RemoveExtension(name);
				g_acbHandles[StringToId(name.c_str())] = pFileData->pAcb;
			}
#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
			else
			{
				CryFixedStringT<MaxFileNameLength> const name(pFileInfo->fileName);
				Cry::Audio::Log(ELogType::Error, "Failed to load ACB %s\n", name.c_str());
			}
#endif      // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE
		}
#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
		else
		{
			Cry::Audio::Log(ELogType::Error, "Invalid FileData passed to the Adx2 implementation of %s", __FUNCTION__);
		}
#endif    // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE
	}
}

//////////////////////////////////////////////////////////////////////////
void CImpl::UnregisterInMemoryFile(SFileInfo* const pFileInfo)
{
	if (pFileInfo != nullptr)
	{
		auto const pFileData = static_cast<CBinary*>(pFileInfo->pImplData);

		if ((pFileData != nullptr) && (pFileData->pAcb != nullptr))
		{
			criAtomExAcb_Release(pFileData->pAcb);
		}
#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
		else
		{
			Cry::Audio::Log(ELogType::Error, "Invalid FileData passed to the Adx2 implementation of %s", __FUNCTION__);
		}
#endif    // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE
	}
}

//////////////////////////////////////////////////////////////////////////
bool CImpl::ConstructFile(XmlNodeRef const& rootNode, SFileInfo* const pFileInfo)
{
	bool isConstructed = false;

	if ((_stricmp(rootNode->getTag(), g_szBinaryTag) == 0) && (pFileInfo != nullptr))
	{
		char const* const szFileName = rootNode->getAttr(g_szNameAttribute);

		if (szFileName != nullptr && szFileName[0] != '\0')
		{
			char const* const szLocalized = rootNode->getAttr(g_szLocalizedAttribute);
			pFileInfo->isLocalized = (szLocalized != nullptr) && (_stricmp(szLocalized, g_szTrueValue) == 0);
			cry_strcpy(pFileInfo->fileName, szFileName);
			CryFixedStringT<MaxFilePathLength> const filePath = (pFileInfo->isLocalized ? m_localizedSoundBankFolder : m_regularSoundBankFolder) + "/" + szFileName;
			cry_strcpy(pFileInfo->filePath, filePath.c_str());

			// The Atom library accesses on-memory data with a 32-bit width.
			// The first address of the data must be aligned at a 4-byte boundary.
			pFileInfo->memoryBlockAlignment = 32;

			MEMSTAT_CONTEXT(EMemStatContextType::AudioImpl, "CryAudio::Impl::Adx2::CBinary");
			pFileInfo->pImplData = new CBinary();

			isConstructed = true;
		}
	}

	return isConstructed;
}

//////////////////////////////////////////////////////////////////////////
void CImpl::DestructFile(IFile* const pIFile)
{
	delete pIFile;
}

//////////////////////////////////////////////////////////////////////////
char const* CImpl::GetFileLocation(IFile* const pIFile) const
{
	return m_localizedSoundBankFolder.c_str();
}

//////////////////////////////////////////////////////////////////////////
void CImpl::GetInfo(SImplInfo& implInfo) const
{
#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
	cry_strcpy(implInfo.name, m_name.c_str());
#else
	cry_fixed_size_strcpy(implInfo.name, g_implNameInRelease);
#endif  // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE
	cry_strcpy(implInfo.folderName, g_szImplFolderName, strlen(g_szImplFolderName));
}

///////////////////////////////////////////////////////////////////////////
IObject* CImpl::ConstructObject(CTransformation const& transformation, IListeners const& listeners, char const* const szName /*= nullptr*/)
{
	auto const pListener = static_cast<CListener*>(listeners[0]); // ADX2 supports only one listener per player.

	MEMSTAT_CONTEXT(EMemStatContextType::AudioImpl, "CryAudio::Impl::Adx2::CObject");
	auto const pObject = new CObject(transformation, pListener);

#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
	pObject->SetName(szName);

	if (!stl::push_back_unique(g_constructedObjects, pObject))
	{
		Cry::Audio::Log(ELogType::Warning, "Trying to construct an already registered object.");
	}

	if (listeners.size() > 1)
	{
		Cry::Audio::Log(ELogType::Warning, R"(More than one listener is passed in %s. Adx2 supports only one listener per object. Listener "%s" will be used for object "%s".)",
		                __FUNCTION__, pListener->GetName(), pObject->GetName());
	}
#else
	stl::push_back_unique(g_constructedObjects, pObject);
#endif  // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE

	return static_cast<IObject*>(pObject);
}

///////////////////////////////////////////////////////////////////////////
void CImpl::DestructObject(IObject const* const pIObject)
{
	auto const pObject = static_cast<CObject const*>(pIObject);

	if (!stl::find_and_erase(g_constructedObjects, pObject))
	{
#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
		Cry::Audio::Log(ELogType::Warning, "Trying to delete a non-existing object.");
#endif    // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE
	}

	delete pObject;
}

///////////////////////////////////////////////////////////////////////////
IListener* CImpl::ConstructListener(CTransformation const& transformation, char const* const szName)
{
	IListener* pIListener = nullptr;

	CriAtomEx3dListenerHn const pHandle = criAtomEx3dListener_Create(&m_listenerConfig, nullptr, 0);

	if (pHandle != nullptr)
	{
		MEMSTAT_CONTEXT(EMemStatContextType::AudioImpl, "CryAudio::Impl::Adx2::CListener");
		auto const pListener = new CListener(transformation, pHandle);

#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
		pListener->SetName(szName);
		g_constructedListeners.push_back(pListener);
#endif    // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE

		pIListener = static_cast<IListener*>(pListener);
	}

	return pIListener;
}

///////////////////////////////////////////////////////////////////////////
void CImpl::DestructListener(IListener* const pIListener)
{
	auto const pListener = static_cast<CListener*>(pIListener);
	criAtomEx3dListener_Destroy(pListener->GetHandle());

#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
	auto iter(g_constructedListeners.begin());
	auto const iterEnd(g_constructedListeners.cend());

	for (; iter != iterEnd; ++iter)
	{
		if ((*iter) == pListener)
		{
			if (iter != (iterEnd - 1))
			{
				(*iter) = g_constructedListeners.back();
			}

			g_constructedListeners.pop_back();
			break;
		}
	}
#endif    // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE

	delete pListener;
}

//////////////////////////////////////////////////////////////////////////
void CImpl::GamepadConnected(DeviceId const deviceUniqueID)
{
}

//////////////////////////////////////////////////////////////////////////
void CImpl::GamepadDisconnected(DeviceId const deviceUniqueID)
{
}

///////////////////////////////////////////////////////////////////////////
ITriggerConnection* CImpl::ConstructTriggerConnection(XmlNodeRef const& rootNode, float& radius)
{
	ITriggerConnection* pITriggerConnection = nullptr;

	if (_stricmp(rootNode->getTag(), g_szCueTag) == 0)
	{
		char const* const szName = rootNode->getAttr(g_szNameAttribute);
		char const* const szCueSheetName = rootNode->getAttr(g_szCueSheetAttribute);
		uint32 const cueId = StringToId(szName);

		CCue::EActionType type = CCue::EActionType::Start;
		char const* const szType = rootNode->getAttr(g_szTypeAttribute);

		if ((szType != nullptr) && (szType[0] != '\0'))
		{
			if (_stricmp(szType, g_szStopValue) == 0)
			{
				type = CCue::EActionType::Stop;
			}
			else if (_stricmp(szType, g_szPauseValue) == 0)
			{
				type = CCue::EActionType::Pause;
			}
			else if (_stricmp(szType, g_szResumeValue) == 0)
			{
				type = CCue::EActionType::Resume;
			}
		}

#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
		uint32 const cueSheetId = StringToId(szCueSheetName);
		float fadeOutTime = 0.0f;

		if (type == CCue::EActionType::Start)
		{
			auto const iterRadius = g_cueRadiusInfo.find(cueId);

			if (iterRadius != g_cueRadiusInfo.end())
			{
				for (auto const& pair : iterRadius->second)
				{
					if (cueSheetId == pair.first)
					{
						radius = pair.second;
						break;
					}
				}
			}

			auto const iterFadeOutTime = g_cueFadeOutTimes.find(cueId);

			if (iterFadeOutTime != g_cueFadeOutTimes.end())
			{
				for (auto const& pair : iterFadeOutTime->second)
				{
					if (cueSheetId == pair.first)
					{
						fadeOutTime = pair.second;
						break;
					}
				}
			}
		}

		MEMSTAT_CONTEXT(EMemStatContextType::AudioImpl, "CryAudio::Impl::Adx2::CCue");
		pITriggerConnection = static_cast<ITriggerConnection*>(new CCue(cueId, szName, cueSheetId, type, szCueSheetName, fadeOutTime));
#else
		MEMSTAT_CONTEXT(EMemStatContextType::AudioImpl, "CryAudio::Impl::Adx2::CCue");
		pITriggerConnection = static_cast<ITriggerConnection*>(new CCue(cueId, szName, StringToId(szCueSheetName), type));
#endif    // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE
	}
	else if (_stricmp(rootNode->getTag(), g_szSnapshotTag) == 0)
	{
		char const* const szName = rootNode->getAttr(g_szNameAttribute);

		char const* const szType = rootNode->getAttr(g_szTypeAttribute);
		CSnapshot::EActionType const type =
			((szType != nullptr) && (szType[0] != '\0') && (_stricmp(szType, g_szStopValue) == 0)) ? CSnapshot::EActionType::Stop : CSnapshot::EActionType::Start;

		int changeoverTime = g_defaultChangeoverTime;
		rootNode->getAttr(g_szTimeAttribute, changeoverTime);

		MEMSTAT_CONTEXT(EMemStatContextType::AudioImpl, "CryAudio::Impl::Adx2::CSnapshot");
		pITriggerConnection = static_cast<ITriggerConnection*>(new CSnapshot(szName, type, static_cast<CriSint32>(changeoverTime)));
	}
#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
	else
	{
		Cry::Audio::Log(ELogType::Warning, "Unknown Adx2 tag: %s", rootNode->getTag());
	}
#endif    // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE

	return pITriggerConnection;
}

//////////////////////////////////////////////////////////////////////////
ITriggerConnection* CImpl::ConstructTriggerConnection(ITriggerInfo const* const pITriggerInfo)
{
#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
	ITriggerConnection* pITriggerConnection = nullptr;
	auto const pTriggerInfo = static_cast<STriggerInfo const*>(pITriggerInfo);

	if (pTriggerInfo != nullptr)
	{
		char const* const szName = pTriggerInfo->name;
		char const* const szCueSheetName = pTriggerInfo->cueSheet;
		uint32 const cueId = StringToId(szName);
		uint32 const cueSheetId = StringToId(szCueSheetName);

		float fadeOutTime = 0.0f;

		auto const iterFadeOutTime = g_cueFadeOutTimes.find(cueId);

		if (iterFadeOutTime != g_cueFadeOutTimes.end())
		{
			for (auto const& pair : iterFadeOutTime->second)
			{
				if (cueSheetId == pair.first)
				{
					fadeOutTime = pair.second;
					break;
				}
			}
		}

		MEMSTAT_CONTEXT(EMemStatContextType::AudioImpl, "CryAudio::Impl::Adx2::CCue");
		pITriggerConnection = static_cast<ITriggerConnection*>(new CCue(cueId, szName, StringToId(szCueSheetName), CCue::EActionType::Start, szCueSheetName, fadeOutTime));
	}

	return pITriggerConnection;
#else
	return nullptr;
#endif  // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE
}

///////////////////////////////////////////////////////////////////////////
void CImpl::DestructTriggerConnection(ITriggerConnection* const pITriggerConnection)
{
	auto const pBaseTriggerConnection = static_cast<CBaseTriggerConnection*>(pITriggerConnection);

	if (pBaseTriggerConnection->GetType() == CBaseTriggerConnection::EType::Cue)
	{
		auto const pCue = static_cast<CCue*>(pBaseTriggerConnection);
		pCue->SetFlag(ECueFlags::ToBeDestructed);

		if (pCue->CanBeDestructed())
		{
			delete pCue;
		}
	}
	else
	{
		delete pBaseTriggerConnection;
	}
}

///////////////////////////////////////////////////////////////////////////
IParameterConnection* CImpl::ConstructParameterConnection(XmlNodeRef const& rootNode)
{
	IParameterConnection* pIParameterConnection = nullptr;

	char const* const szTag = rootNode->getTag();

	if (_stricmp(szTag, g_szAisacControlTag) == 0)
	{
		char const* const szName = rootNode->getAttr(g_szNameAttribute);
		CriAtomExAisacControlId const id = criAtomExAcf_GetAisacControlIdByName(szName);
		bool isAdvanced = false;

		float minValue = g_defaultParamMinValue;
		float maxValue = g_defaultParamMaxValue;
		float multiplier = g_defaultParamMultiplier;
		float shift = g_defaultParamShift;

		isAdvanced |= rootNode->getAttr(g_szValueMinAttribute, minValue);
		isAdvanced |= rootNode->getAttr(g_szValueMaxAttribute, maxValue);
		isAdvanced |= rootNode->getAttr(g_szMutiplierAttribute, multiplier);
		isAdvanced |= rootNode->getAttr(g_szShiftAttribute, shift);

		if (isAdvanced)
		{
#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
			if (minValue > maxValue)
			{
				Cry::Audio::Log(ELogType::Warning, "Min value (%f) was greater than max value (%f) of %s", minValue, maxValue, szName);
			}
#endif      // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE

			MEMSTAT_CONTEXT(EMemStatContextType::AudioImpl, "CryAudio::Impl::Adx2::CAisacControlAdvanced");

#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
			pIParameterConnection = static_cast<IParameterConnection*>(new CAisacControlAdvanced(id, minValue, maxValue, multiplier, shift, szName));
#else
			pIParameterConnection = static_cast<IParameterConnection*>(new CAisacControlAdvanced(id, minValue, maxValue, multiplier, shift));
#endif      // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE
		}
		else
		{
			MEMSTAT_CONTEXT(EMemStatContextType::AudioImpl, "CryAudio::Impl::Adx2::CAisacControl");

#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
			pIParameterConnection = static_cast<IParameterConnection*>(new CAisacControl(id, szName));
#else
			pIParameterConnection = static_cast<IParameterConnection*>(new CAisacControl(id));
#endif      // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE
		}
	}
	else if (_stricmp(szTag, g_szSCategoryTag) == 0)
	{
		char const* const szName = rootNode->getAttr(g_szNameAttribute);
		bool isAdvanced = false;

		float minValue = g_defaultParamMinValue;
		float maxValue = g_defaultParamMaxValue;
		float multiplier = g_defaultParamMultiplier;
		float shift = g_defaultParamShift;

		isAdvanced |= rootNode->getAttr(g_szValueMinAttribute, minValue);
		isAdvanced |= rootNode->getAttr(g_szValueMaxAttribute, maxValue);
		isAdvanced |= rootNode->getAttr(g_szMutiplierAttribute, multiplier);
		isAdvanced |= rootNode->getAttr(g_szShiftAttribute, shift);

		if (isAdvanced)
		{
#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
			if (minValue > maxValue)
			{
				Cry::Audio::Log(ELogType::Warning, "Min value (%f) was greater than max value (%f) of %s", minValue, maxValue, szName);
			}
#endif      // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE

			MEMSTAT_CONTEXT(EMemStatContextType::AudioImpl, "CryAudio::Impl::Adx2::CCategoryAdvanced");
			pIParameterConnection = static_cast<IParameterConnection*>(new CCategoryAdvanced(szName, minValue, maxValue, multiplier, shift));
		}
		else
		{
			MEMSTAT_CONTEXT(EMemStatContextType::AudioImpl, "CryAudio::Impl::Adx2::CCategory");
			pIParameterConnection = static_cast<IParameterConnection*>(new CCategory(szName));
		}
	}
	else if (_stricmp(szTag, g_szGameVariableTag) == 0)
	{
		char const* const szName = rootNode->getAttr(g_szNameAttribute);
		bool isAdvanced = false;

		float minValue = g_defaultParamMinValue;
		float maxValue = g_defaultParamMaxValue;
		float multiplier = g_defaultParamMultiplier;
		float shift = g_defaultParamShift;

		isAdvanced |= rootNode->getAttr(g_szValueMinAttribute, minValue);
		isAdvanced |= rootNode->getAttr(g_szValueMaxAttribute, maxValue);
		isAdvanced |= rootNode->getAttr(g_szMutiplierAttribute, multiplier);
		isAdvanced |= rootNode->getAttr(g_szShiftAttribute, shift);

		if (isAdvanced)
		{
#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
			if (minValue > maxValue)
			{
				Cry::Audio::Log(ELogType::Warning, "Min value (%f) was greater than max value (%f) of %s", minValue, maxValue, szName);
			}
#endif      // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE

			MEMSTAT_CONTEXT(EMemStatContextType::AudioImpl, "CryAudio::Impl::Adx2::CGameVariableAdvanced");
			pIParameterConnection = static_cast<IParameterConnection*>(new CGameVariableAdvanced(szName, minValue, maxValue, multiplier, shift));
		}
		else
		{
			MEMSTAT_CONTEXT(EMemStatContextType::AudioImpl, "CryAudio::Impl::Adx2::CGameVariable");
			pIParameterConnection = static_cast<IParameterConnection*>(new CGameVariable(szName));
		}
	}
#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
	else
	{
		Cry::Audio::Log(ELogType::Warning, "Unknown Adx2 tag: %s", szTag);
	}
#endif    // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE

	return pIParameterConnection;
}

///////////////////////////////////////////////////////////////////////////
void CImpl::DestructParameterConnection(IParameterConnection const* const pIParameterConnection)
{
	delete pIParameterConnection;
}

//////////////////////////////////////////////////////////////////////////
bool ParseSelectorNode(
	XmlNodeRef const& node,
	CryFixedStringT<MaxFileNameLength>& selectorName,
	CryFixedStringT<MaxFileNameLength>& selectorLabelName)
{
	bool foundAttributes = false;

	char const* const szSelectorName = node->getAttr(g_szNameAttribute);

	if ((szSelectorName != nullptr) && (szSelectorName[0] != 0) && (node->getChildCount() == 1))
	{
		XmlNodeRef const labelNode(node->getChild(0));

		if (labelNode.isValid() && _stricmp(labelNode->getTag(), g_szSelectorLabelTag) == 0)
		{
			char const* const szSelectorLabelName = labelNode->getAttr(g_szNameAttribute);

			if ((szSelectorLabelName != nullptr) && (szSelectorLabelName[0] != 0))
			{
				selectorName = szSelectorName;
				selectorLabelName = szSelectorLabelName;
				foundAttributes = true;
			}
		}
	}
#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
	else
	{
		Cry::Audio::Log(ELogType::Warning, "An Adx2 Selector %s inside SwitchState needs to have exactly one Selector Label.", szSelectorName);
	}
#endif    // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE

	return foundAttributes;
}

///////////////////////////////////////////////////////////////////////////
ISwitchStateConnection* CImpl::ConstructSwitchStateConnection(XmlNodeRef const& rootNode)
{
	ISwitchStateConnection* pISwitchStateConnection = nullptr;

	char const* const szTag = rootNode->getTag();

	if (_stricmp(szTag, g_szSelectorTag) == 0)
	{
		CryFixedStringT<MaxFileNameLength> selectorName;
		CryFixedStringT<MaxFileNameLength> selectorLabelName;

		if (ParseSelectorNode(rootNode, selectorName, selectorLabelName))
		{
			MEMSTAT_CONTEXT(EMemStatContextType::AudioImpl, "CryAudio::Impl::Adx2::CSelectorLabel");
			pISwitchStateConnection = static_cast<ISwitchStateConnection*>(new CSelectorLabel(selectorName.c_str(), selectorLabelName));
		}
	}
	else if (_stricmp(szTag, g_szAisacControlTag) == 0)
	{
		char const* const szName = rootNode->getAttr(g_szNameAttribute);
		CriAtomExAisacControlId const id = criAtomExAcf_GetAisacControlIdByName(szName);

		float value = g_defaultStateValue;
		rootNode->getAttr(g_szValueAttribute, value);

		MEMSTAT_CONTEXT(EMemStatContextType::AudioImpl, "CryAudio::Impl::Adx2::CAisacState");

#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
		pISwitchStateConnection = static_cast<ISwitchStateConnection*>(new CAisacState(id, static_cast<CriFloat32>(value), szName));
#else
		pISwitchStateConnection = static_cast<ISwitchStateConnection*>(new CAisacState(id, static_cast<CriFloat32>(value)));
#endif    // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE
	}
	else if (_stricmp(szTag, g_szGameVariableTag) == 0)
	{
		char const* const szName = rootNode->getAttr(g_szNameAttribute);
		float value = g_defaultStateValue;
		rootNode->getAttr(g_szValueAttribute, value);

		MEMSTAT_CONTEXT(EMemStatContextType::AudioImpl, "CryAudio::Impl::Adx2::CGameVariableState");
		pISwitchStateConnection = static_cast<ISwitchStateConnection*>(new CGameVariableState(szName, static_cast<CriFloat32>(value)));
	}
	else if (_stricmp(szTag, g_szSCategoryTag) == 0)
	{
		char const* const szName = rootNode->getAttr(g_szNameAttribute);
		float value = g_defaultStateValue;
		rootNode->getAttr(g_szValueAttribute, value);

		MEMSTAT_CONTEXT(EMemStatContextType::AudioImpl, "CryAudio::Impl::Adx2::CCategoryState");
		pISwitchStateConnection = static_cast<ISwitchStateConnection*>(new CCategoryState(szName, static_cast<CriFloat32>(value)));
	}
#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
	else
	{
		Cry::Audio::Log(ELogType::Warning, "Unknown Adx2 tag: %s", szTag);
	}
#endif    // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE

	return pISwitchStateConnection;
}

///////////////////////////////////////////////////////////////////////////
void CImpl::DestructSwitchStateConnection(ISwitchStateConnection const* const pISwitchStateConnection)
{
	delete pISwitchStateConnection;
}

///////////////////////////////////////////////////////////////////////////
IEnvironmentConnection* CImpl::ConstructEnvironmentConnection(XmlNodeRef const& rootNode)
{
	IEnvironmentConnection* pEnvironmentConnection = nullptr;

	char const* const szTag = rootNode->getTag();

	if (_stricmp(szTag, g_szDspBusTag) == 0)
	{
		char const* const szName = rootNode->getAttr(g_szNameAttribute);

		MEMSTAT_CONTEXT(EMemStatContextType::AudioImpl, "CryAudio::Impl::Adx2::CDspBus");
		pEnvironmentConnection = static_cast<IEnvironmentConnection*>(new CDspBus(szName));
	}
	else if (_stricmp(szTag, g_szAisacControlTag) == 0)
	{
		char const* const szName = rootNode->getAttr(g_szNameAttribute);
		CriAtomExAisacControlId const id = criAtomExAcf_GetAisacControlIdByName(szName);
		bool isAdvanced = false;

		float multiplier = g_defaultParamMultiplier;
		float shift = g_defaultParamShift;

		isAdvanced |= rootNode->getAttr(g_szMutiplierAttribute, multiplier);
		isAdvanced |= rootNode->getAttr(g_szShiftAttribute, shift);

		if (isAdvanced)
		{
			MEMSTAT_CONTEXT(EMemStatContextType::AudioImpl, "CryAudio::Impl::Adx2::CAisacEnvironmentAdvanced");

#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
			pEnvironmentConnection = static_cast<IEnvironmentConnection*>(new CAisacEnvironmentAdvanced(id, multiplier, shift, szName));
#else
			pEnvironmentConnection = static_cast<IEnvironmentConnection*>(new CAisacEnvironmentAdvanced(id, multiplier, shift));
#endif      // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE
		}
		else
		{
			MEMSTAT_CONTEXT(EMemStatContextType::AudioImpl, "CryAudio::Impl::Adx2::CAisacEnvironment");

#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
			pEnvironmentConnection = static_cast<IEnvironmentConnection*>(new CAisacEnvironment(id, szName));
#else
			pEnvironmentConnection = static_cast<IEnvironmentConnection*>(new CAisacEnvironment(id));
#endif      // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE
		}
	}
#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
	else
	{
		Cry::Audio::Log(ELogType::Warning, "Unknown Adx2 tag: %s", szTag);
	}
#endif    // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE

	return pEnvironmentConnection;
}

///////////////////////////////////////////////////////////////////////////
void CImpl::DestructEnvironmentConnection(IEnvironmentConnection const* const pIEnvironmentConnection)
{
	delete pIEnvironmentConnection;
}

//////////////////////////////////////////////////////////////////////////
ISettingConnection* CImpl::ConstructSettingConnection(XmlNodeRef const& rootNode)
{
	ISettingConnection* pISettingConnection = nullptr;

	char const* const szTag = rootNode->getTag();

	if (_stricmp(szTag, g_szDspBusSettingTag) == 0)
	{
		char const* const szName = rootNode->getAttr(g_szNameAttribute);

		MEMSTAT_CONTEXT(EMemStatContextType::AudioImpl, "CryAudio::Impl::Adx2::CDspBusSetting");
		pISettingConnection = static_cast<ISettingConnection*>(new CDspBusSetting(szName));
	}
#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
	else
	{
		Cry::Audio::Log(ELogType::Warning, "Unknown Adx2 tag: %s", szTag);
	}
#endif    // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE

	return pISettingConnection;
}

//////////////////////////////////////////////////////////////////////////
void CImpl::DestructSettingConnection(ISettingConnection const* const pISettingConnection)
{
	delete pISettingConnection;
}

//////////////////////////////////////////////////////////////////////////
void CImpl::OnRefresh()
{
#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
	g_debugCurrentDspBusSettingName = g_debugNoneDspBusSetting;
	m_acfFileSize = 0;
	g_cueRadiusInfo.clear();
	g_cueFadeOutTimes.clear();
	LoadAcbInfos(m_regularSoundBankFolder);
	LoadAcbInfos(m_localizedSoundBankFolder);
#endif  // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE

	UnregisterAcf();
	RegisterAcf();
	g_acbHandles.clear();
}

//////////////////////////////////////////////////////////////////////////
void CImpl::SetLanguage(char const* const szLanguage)
{
	if (szLanguage != nullptr)
	{
		m_language = szLanguage;
		m_localizedSoundBankFolder = PathUtil::GetLocalizationFolder().c_str();
		m_localizedSoundBankFolder += "/";
		m_localizedSoundBankFolder += m_language.c_str();
		m_localizedSoundBankFolder += "/";
		m_localizedSoundBankFolder += CRY_AUDIO_DATA_ROOT;
		m_localizedSoundBankFolder += "/";
		m_localizedSoundBankFolder += g_szImplFolderName;
		m_localizedSoundBankFolder += "/";
		m_localizedSoundBankFolder += g_szAssetsFolderName;
	}
}

#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
//////////////////////////////////////////////////////////////////////////
CCueInstance* CImpl::ConstructCueInstance(TriggerInstanceId const triggerInstanceId, CriAtomExPlaybackId const playbackId, CCue& cue, CObject const& object)
{
	cue.IncrementNumInstances();

	auto const pCueInstance = new CCueInstance(triggerInstanceId, playbackId, cue, object);
	g_constructedCueInstances.push_back(pCueInstance);

	return pCueInstance;
}
#else
//////////////////////////////////////////////////////////////////////////
CCueInstance* CImpl::ConstructCueInstance(TriggerInstanceId const triggerInstanceId, CriAtomExPlaybackId const playbackId, CCue& cue)
{
	cue.IncrementNumInstances();

	MEMSTAT_CONTEXT(EMemStatContextType::AudioImpl, "CryAudio::Impl::Adx2::CCueInstance");
	return new CCueInstance(triggerInstanceId, playbackId, cue);
}
#endif  // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE

//////////////////////////////////////////////////////////////////////////
void CImpl::DestructCueInstance(CCueInstance const* const pCueInstance)
{
#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
	CRY_ASSERT_MESSAGE(pCueInstance != nullptr, "pCueInstance is nullpter during %s", __FUNCTION__);

	auto iter(g_constructedCueInstances.begin());
	auto const iterEnd(g_constructedCueInstances.cend());

	for (; iter != iterEnd; ++iter)
	{
		if ((*iter) == pCueInstance)
		{
			if (iter != (iterEnd - 1))
			{
				(*iter) = g_constructedCueInstances.back();
			}

			g_constructedCueInstances.pop_back();
			break;
		}
	}
#endif  // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE

	CCue* const pCue = &pCueInstance->GetCue();
	delete pCueInstance;

	pCue->DecrementNumInstances();

	if (pCue->CanBeDestructed())
	{
		delete pCue;
	}
}

//////////////////////////////////////////////////////////////////////////
bool CImpl::InitializeLibrary()
{
#if CRY_PLATFORM_WINDOWS
	CriAtomExConfig_WASAPI libraryConfig;
	criAtomEx_SetDefaultConfig_WASAPI(&libraryConfig);
#elif CRY_PLATFORM_DURANGO
	CriAtomExConfig_XBOXONE libraryConfig;
	criAtomEx_SetDefaultConfig_XBOXONE(&libraryConfig);
#elif CRY_PLATFORM_ORBIS
	CriAtomExConfig_PS4 libraryConfig;
	criAtomEx_SetDefaultConfig_PS4(&libraryConfig);
#elif CRY_PLATFORM_LINUX
	CriAtomExConfig_LINUX libraryConfig;
	criAtomEx_SetDefaultConfig_LINUX(&libraryConfig);
#endif

	libraryConfig.atom_ex.max_virtual_voices = static_cast<CriSint32>(g_cvars.m_maxVirtualVoices);
	libraryConfig.atom_ex.max_voice_limit_groups = static_cast<CriSint32>(g_cvars.m_maxVoiceLimitGroups);
	libraryConfig.atom_ex.max_categories = static_cast<CriSint32>(g_cvars.m_maxCategories);
	libraryConfig.atom_ex.categories_per_playback = static_cast<CriSint32>(g_cvars.m_categoriesPerPlayback);
	libraryConfig.atom_ex.max_tracks = static_cast<CriUint32>(g_cvars.m_maxTracks);
	libraryConfig.atom_ex.max_track_items = static_cast<CriUint32>(g_cvars.m_maxTrackItems);
	libraryConfig.atom_ex.max_faders = static_cast<CriUint32>(g_cvars.m_maxFaders);
	libraryConfig.atom_ex.max_pitch = static_cast<CriFloat32>(g_cvars.m_maxPitch);
	libraryConfig.asr.num_buses = static_cast<CriSint32>(g_cvars.m_numBuses);
	libraryConfig.asr.output_channels = static_cast<CriSint32>(g_cvars.m_outputChannels);
	libraryConfig.asr.output_sampling_rate = static_cast<CriSint32>(g_cvars.m_outputSamplingRate);

#if CRY_PLATFORM_WINDOWS
	criAtomEx_Initialize_WASAPI(&libraryConfig, nullptr, 0);
#elif CRY_PLATFORM_DURANGO
	criAtomEx_Initialize_XBOXONE(&libraryConfig, nullptr, 0);
#elif CRY_PLATFORM_ORBIS
	criAtomEx_Initialize_PS4(&libraryConfig, nullptr, 0);
#elif CRY_PLATFORM_LINUX
	criAtomEx_InitializeForUserPcmOutput_LINUX(&libraryConfig, nullptr, 0);
#endif

#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
	bool const isInitialized = (criAtomEx_IsInitialized() == CRI_TRUE);

	if (!isInitialized)
	{
		Cry::Audio::Log(ELogType::Error, "Failed to initialize CriAtomEx");
	}

	return isInitialized;
#else
	return criAtomEx_IsInitialized() == CRI_TRUE;
#endif  // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE
}

//////////////////////////////////////////////////////////////////////////
bool CImpl::AllocateVoicePool()
{
	CriAtomExStandardVoicePoolConfig voicePoolConfig;
	criAtomExVoicePool_SetDefaultConfigForStandardVoicePool(&voicePoolConfig);
	voicePoolConfig.num_voices = static_cast<CriSint32>(g_cvars.m_numVoices);
	voicePoolConfig.player_config.max_channels = static_cast<CriSint32>(g_cvars.m_maxChannels);
	voicePoolConfig.player_config.max_sampling_rate = static_cast<CriSint32>(g_cvars.m_maxSamplingRate);
	voicePoolConfig.player_config.streaming_flag = CRI_TRUE;

#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
	bool const isAllocated = (criAtomExVoicePool_AllocateStandardVoicePool(&voicePoolConfig, nullptr, 0) != nullptr);

	if (!isAllocated)
	{
		Cry::Audio::Log(ELogType::Error, "Failed to allocate voice pool");
	}

	return isAllocated;
#else
	return criAtomExVoicePool_AllocateStandardVoicePool(&voicePoolConfig, nullptr, 0) != nullptr;
#endif  // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE
}

//////////////////////////////////////////////////////////////////////////
bool CImpl::CreateDbas()
{
	CriAtomDbasConfig dbasConfig;
	criAtomDbas_SetDefaultConfig(&dbasConfig);
	dbasConfig.max_streams = static_cast<CriSint32>(g_cvars.m_maxStreams);
	m_dbasId = criAtomExDbas_Create(&dbasConfig, nullptr, 0);

#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
	bool const isCreated = (m_dbasId != CRIATOMEXDBAS_ILLEGAL_ID);

	if (!isCreated)
	{
		Cry::Audio::Log(ELogType::Error, "Failed to create D-BAS");
	}

	return isCreated;
#else
	return m_dbasId != CRIATOMEXDBAS_ILLEGAL_ID;
#endif  // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE
}

//////////////////////////////////////////////////////////////////////////
void CImpl::RegisterAcf()
{
	bool acfExists = false;

	CryFixedStringT<MaxFilePathLength> acfPath;
	CryFixedStringT<MaxFilePathLength> search(m_regularSoundBankFolder + "/*.acf");

	_finddata_t fd;
	intptr_t const handle = gEnv->pCryPak->FindFirst(search.c_str(), &fd);

	if (handle != -1)
	{
		do
		{
			char const* const fileName = fd.name;

			if (_stricmp(PathUtil::GetExt(fileName), "acf") == 0)
			{
				acfPath = m_regularSoundBankFolder + "/" + fileName;
				acfExists = true;
				break;
			}
		}
		while (gEnv->pCryPak->FindNext(handle, &fd) >= 0);

		gEnv->pCryPak->FindClose(handle);
	}

	if (acfExists)
	{
		FILE* const pAcfFile = gEnv->pCryPak->FOpen(acfPath.c_str(), "rb");
		size_t const acfFileSize = gEnv->pCryPak->FGetSize(acfPath.c_str());

		MEMSTAT_CONTEXT(EMemStatContextType::AudioImpl, "CryAudio::Impl::Adx2::CImpl::m_pAcfBuffer");
		m_pAcfBuffer = new uint8[acfFileSize];
		gEnv->pCryPak->FRead(m_pAcfBuffer, acfFileSize, pAcfFile);
		gEnv->pCryPak->FClose(pAcfFile);

		auto const pAcfData = static_cast<void*>(m_pAcfBuffer);

		criAtomEx_RegisterAcfData(pAcfData, static_cast<CriSint32>(acfFileSize), nullptr, 0);
		CriAtomExAcfInfo acfInfo;

		if (criAtomExAcf_GetAcfInfo(&acfInfo) == CRI_TRUE)
		{
			g_absoluteVelocityAisacId = criAtomExAcf_GetAisacControlIdByName(g_szAbsoluteVelocityAisacName);
			g_occlusionAisacId = criAtomExAcf_GetAisacControlIdByName(g_szOcclusionAisacName);

#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
			m_acfFileSize = acfFileSize;
#endif      // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE
		}
#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
		else
		{
			Cry::Audio::Log(ELogType::Error, "Failed to register ACF");
		}
#endif    // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE
	}
#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
	else
	{
		Cry::Audio::Log(ELogType::Error, "ACF not found.");
	}
#endif  // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE
}

//////////////////////////////////////////////////////////////////////////
void CImpl::UnregisterAcf()
{
	criAtomEx_UnregisterAcf();
	delete[] m_pAcfBuffer;
	m_pAcfBuffer = nullptr;

	g_absoluteVelocityAisacId = CRIATOMEX_INVALID_AISAC_CONTROL_ID;
	g_occlusionAisacId = CRIATOMEX_INVALID_AISAC_CONTROL_ID;
}

//////////////////////////////////////////////////////////////////////////
void CImpl::InitializeFileSystem()
{
	criFs_SetUserMallocFunction(userMalloc, nullptr);
	criFs_SetUserFreeFunction(userFree, nullptr);

	CriFsConfig fileSystemConfig;
	criFs_SetDefaultConfig(&fileSystemConfig);
	fileSystemConfig.max_files = static_cast<CriSint32>(g_cvars.m_maxFiles);

	criFs_InitializeLibrary(&fileSystemConfig, nullptr, 0);
	criFs_SetSelectIoCallback(selectIoFunc);
}

//////////////////////////////////////////////////////////////////////////
void CImpl::SetListenerConfig()
{
	criAtomEx3dListener_SetDefaultConfig(&m_listenerConfig);
}

//////////////////////////////////////////////////////////////////////////
void CImpl::SetPlayerConfig()
{
	criAtomExPlayer_SetDefaultConfig(&g_playerConfig);
	g_playerConfig.voice_allocation_method = static_cast<CriAtomExVoiceAllocationMethod>(g_cvars.m_voiceAllocationMethod);
}

//////////////////////////////////////////////////////////////////////////
void CImpl::Set3dSourceConfig()
{
	g_3dSourceConfig.enable_voice_priority_decay = CRI_TRUE;
}

//////////////////////////////////////////////////////////////////////////
void CImpl::MuteAllObjects(CriBool const shouldMute)
{
	for (auto const pObject : g_constructedObjects)
	{
		pObject->MutePlayer(shouldMute);
	}
}

//////////////////////////////////////////////////////////////////////////
void CImpl::PauseAllObjects(CriBool const shouldPause)
{
	for (auto const pObject : g_constructedObjects)
	{
		pObject->PausePlayer(shouldPause);
	}
}

//////////////////////////////////////////////////////////////////////////
void CImpl::DrawDebugMemoryInfo(IRenderAuxGeom& auxGeom, float const posX, float& posY, bool const drawDetailedInfo)
{
#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)

	float const headerPosY = posY;
	posY += Debug::g_systemHeaderLineSpacerHeight;

	size_t totalPoolSize = 0;

	{
		auto& allocator = CObject::GetAllocator();
		size_t const memAlloc = allocator.GetTotalMemory().nAlloc;
		totalPoolSize += memAlloc;

		if (drawDetailedInfo)
		{
			Debug::DrawMemoryPoolInfo(auxGeom, posX, posY, memAlloc, allocator.GetCounts(), "Objects", g_objectPoolSize);
		}
	}

	{
		auto& allocator = CCueInstance::GetAllocator();
		size_t const memAlloc = allocator.GetTotalMemory().nAlloc;
		totalPoolSize += memAlloc;

		if (drawDetailedInfo)
		{
			Debug::DrawMemoryPoolInfo(auxGeom, posX, posY, memAlloc, allocator.GetCounts(), "Cue Instances", g_cueInstancePoolSize);
		}
	}

	if (g_debugPoolSizes.cues > 0)
	{
		auto& allocator = CCue::GetAllocator();
		size_t const memAlloc = allocator.GetTotalMemory().nAlloc;
		totalPoolSize += memAlloc;

		if (drawDetailedInfo)
		{
			Debug::DrawMemoryPoolInfo(auxGeom, posX, posY, memAlloc, allocator.GetCounts(), "Cues", g_poolSizes.cues);
		}
	}

	if (g_debugPoolSizes.aisacControls > 0)
	{
		auto& allocator = CAisacControl::GetAllocator();
		size_t const memAlloc = allocator.GetTotalMemory().nAlloc;
		totalPoolSize += memAlloc;

		if (drawDetailedInfo)
		{
			Debug::DrawMemoryPoolInfo(auxGeom, posX, posY, memAlloc, allocator.GetCounts(), "Aisac-Controls", g_poolSizes.aisacControls);
		}
	}

	if (g_debugPoolSizes.aisacControlsAdvanced > 0)
	{
		auto& allocator = CAisacControlAdvanced::GetAllocator();
		size_t const memAlloc = allocator.GetTotalMemory().nAlloc;
		totalPoolSize += memAlloc;

		if (drawDetailedInfo)
		{
			Debug::DrawMemoryPoolInfo(auxGeom, posX, posY, memAlloc, allocator.GetCounts(), "Advanced Aisac-Controls", g_poolSizes.aisacControlsAdvanced);
		}
	}

	if (g_debugPoolSizes.aisacEnvironments > 0)
	{
		auto& allocator = CAisacEnvironment::GetAllocator();
		size_t const memAlloc = allocator.GetTotalMemory().nAlloc;
		totalPoolSize += memAlloc;

		if (drawDetailedInfo)
		{
			Debug::DrawMemoryPoolInfo(auxGeom, posX, posY, memAlloc, allocator.GetCounts(), "Aisac-Controls on Environments", g_poolSizes.aisacEnvironments);
		}
	}

	if (g_debugPoolSizes.aisacEnvironmentsAdvanced > 0)
	{
		auto& allocator = CAisacEnvironmentAdvanced::GetAllocator();
		size_t const memAlloc = allocator.GetTotalMemory().nAlloc;
		totalPoolSize += memAlloc;

		if (drawDetailedInfo)
		{
			Debug::DrawMemoryPoolInfo(auxGeom, posX, posY, memAlloc, allocator.GetCounts(), "Advanced Aisac-Controls on Environments", g_poolSizes.aisacEnvironmentsAdvanced);
		}
	}

	if (g_debugPoolSizes.aisacStates > 0)
	{
		auto& allocator = CAisacState::GetAllocator();
		size_t const memAlloc = allocator.GetTotalMemory().nAlloc;
		totalPoolSize += memAlloc;

		if (drawDetailedInfo)
		{
			Debug::DrawMemoryPoolInfo(auxGeom, posX, posY, memAlloc, allocator.GetCounts(), "Aisac-Controls on States", g_poolSizes.aisacStates);
		}
	}

	if (g_debugPoolSizes.categories > 0)
	{
		auto& allocator = CCategory::GetAllocator();
		size_t const memAlloc = allocator.GetTotalMemory().nAlloc;
		totalPoolSize += memAlloc;

		if (drawDetailedInfo)
		{
			Debug::DrawMemoryPoolInfo(auxGeom, posX, posY, memAlloc, allocator.GetCounts(), "Categories", g_poolSizes.categories);
		}
	}

	if (g_debugPoolSizes.categoriesAdvanced > 0)
	{
		auto& allocator = CCategoryAdvanced::GetAllocator();
		size_t const memAlloc = allocator.GetTotalMemory().nAlloc;
		totalPoolSize += memAlloc;

		if (drawDetailedInfo)
		{
			Debug::DrawMemoryPoolInfo(auxGeom, posX, posY, memAlloc, allocator.GetCounts(), "Advanced Categories", g_poolSizes.categoriesAdvanced);
		}
	}

	if (g_debugPoolSizes.categoryStates > 0)
	{
		auto& allocator = CCategoryState::GetAllocator();
		size_t const memAlloc = allocator.GetTotalMemory().nAlloc;
		totalPoolSize += memAlloc;

		if (drawDetailedInfo)
		{
			Debug::DrawMemoryPoolInfo(auxGeom, posX, posY, memAlloc, allocator.GetCounts(), "Categories on States", g_poolSizes.categoryStates);
		}
	}

	if (g_debugPoolSizes.gameVariables > 0)
	{
		auto& allocator = CGameVariable::GetAllocator();
		size_t const memAlloc = allocator.GetTotalMemory().nAlloc;
		totalPoolSize += memAlloc;

		if (drawDetailedInfo)
		{
			Debug::DrawMemoryPoolInfo(auxGeom, posX, posY, memAlloc, allocator.GetCounts(), "GameVariables", g_poolSizes.gameVariables);
		}
	}

	if (g_debugPoolSizes.gameVariablesAdvanced > 0)
	{
		auto& allocator = CGameVariableAdvanced::GetAllocator();
		size_t const memAlloc = allocator.GetTotalMemory().nAlloc;
		totalPoolSize += memAlloc;

		if (drawDetailedInfo)
		{
			Debug::DrawMemoryPoolInfo(auxGeom, posX, posY, memAlloc, allocator.GetCounts(), "Advanced GameVariables", g_poolSizes.gameVariablesAdvanced);
		}
	}

	if (g_debugPoolSizes.gameVariableStates > 0)
	{
		auto& allocator = CGameVariableState::GetAllocator();
		size_t const memAlloc = allocator.GetTotalMemory().nAlloc;
		totalPoolSize += memAlloc;

		if (drawDetailedInfo)
		{
			Debug::DrawMemoryPoolInfo(auxGeom, posX, posY, memAlloc, allocator.GetCounts(), "GameVariables on States", g_poolSizes.gameVariableStates);
		}
	}

	if (g_debugPoolSizes.selectorLabels > 0)
	{
		auto& allocator = CSelectorLabel::GetAllocator();
		size_t const memAlloc = allocator.GetTotalMemory().nAlloc;
		totalPoolSize += memAlloc;

		if (drawDetailedInfo)
		{
			Debug::DrawMemoryPoolInfo(auxGeom, posX, posY, memAlloc, allocator.GetCounts(), "Selector Labels", g_poolSizes.selectorLabels);
		}
	}

	if (g_debugPoolSizes.dspBuses > 0)
	{
		auto& allocator = CDspBus::GetAllocator();
		size_t const memAlloc = allocator.GetTotalMemory().nAlloc;
		totalPoolSize += memAlloc;

		if (drawDetailedInfo)
		{
			Debug::DrawMemoryPoolInfo(auxGeom, posX, posY, memAlloc, allocator.GetCounts(), "DSP Buses", g_poolSizes.dspBuses);
		}
	}

	if (g_debugPoolSizes.snapshots > 0)
	{
		auto& allocator = CSnapshot::GetAllocator();
		size_t const memAlloc = allocator.GetTotalMemory().nAlloc;
		totalPoolSize += memAlloc;

		if (drawDetailedInfo)
		{
			Debug::DrawMemoryPoolInfo(auxGeom, posX, posY, memAlloc, allocator.GetCounts(), "Snapshots", g_poolSizes.snapshots);
		}
	}

	if (g_debugPoolSizes.dspBusSettings > 0)
	{
		auto& allocator = CDspBusSetting::GetAllocator();
		size_t const memAlloc = allocator.GetTotalMemory().nAlloc;
		totalPoolSize += memAlloc;

		if (drawDetailedInfo)
		{
			Debug::DrawMemoryPoolInfo(auxGeom, posX, posY, memAlloc, allocator.GetCounts(), "DSP Bus Settings", g_poolSizes.dspBusSettings);
		}
	}

	if (g_debugPoolSizes.binaries > 0)
	{
		auto& allocator = CBinary::GetAllocator();
		size_t const memAlloc = allocator.GetTotalMemory().nAlloc;
		totalPoolSize += memAlloc;

		if (drawDetailedInfo)
		{
			Debug::DrawMemoryPoolInfo(auxGeom, posX, posY, memAlloc, allocator.GetCounts(), "Binaries", g_poolSizes.binaries);
		}
	}

	CryModuleMemoryInfo memInfo;
	ZeroStruct(memInfo);
	CryGetMemoryInfoForModule(&memInfo);

	CryFixedStringT<Debug::MaxMemInfoStringLength> memAllocSizeString;
	auto const memAllocSize = static_cast<size_t>(memInfo.allocated - memInfo.freed);
	Debug::FormatMemoryString(memAllocSizeString, memAllocSize);

	CryFixedStringT<Debug::MaxMemInfoStringLength> totalPoolSizeString;
	Debug::FormatMemoryString(totalPoolSizeString, totalPoolSize);

	CryFixedStringT<Debug::MaxMemInfoStringLength> acfSizeString;
	Debug::FormatMemoryString(acfSizeString, m_acfFileSize);

	CryFixedStringT<Debug::MaxMemInfoStringLength> totalMemSizeString;
	size_t const totalMemSize = memAllocSize + totalPoolSize + m_acfFileSize;
	Debug::FormatMemoryString(totalMemSizeString, totalMemSize);

	auxGeom.Draw2dLabel(posX, headerPosY, Debug::g_systemHeaderFontSize, Debug::s_globalColorHeader, false, "%s (System: %s | Pools: %s | ACF: %s | Total: %s)",
	                    m_name.c_str(), memAllocSizeString.c_str(), totalPoolSizeString.c_str(), acfSizeString.c_str(), totalMemSizeString.c_str());

	size_t const numCueInstances = g_constructedCueInstances.size();
	size_t const numListeners = g_constructedListeners.size();

	posY += Debug::g_systemLineHeight;
	auxGeom.Draw2dLabel(posX, posY, Debug::g_systemFontSize, Debug::s_systemColorTextSecondary, false,
	                    "Active Cues: %3" PRISIZE_T " | Listeners: %" PRISIZE_T " | Objects with Doppler: %u | DSP Bus Setting: %s | Active Snapshot: %s",
	                    numCueInstances, numListeners, g_numObjectsWithDoppler, g_debugCurrentDspBusSettingName.c_str(), g_debugActiveSnapShotName.c_str());

	for (auto const pListener : g_constructedListeners)
	{
		Vec3 const& listenerPosition = pListener->GetPosition();
		Vec3 const& listenerDirection = pListener->GetTransformation().GetForward();
		float const listenerVelocity = pListener->GetVelocity().GetLength();
		char const* const szName = pListener->GetName();

		posY += Debug::g_systemLineHeight;
		auxGeom.Draw2dLabel(posX, posY, Debug::g_systemFontSize, Debug::s_systemColorListenerActive, false, "Listener: %s | PosXYZ: %.2f %.2f %.2f | FwdXYZ: %.2f %.2f %.2f | Velocity: %.2f m/s",
		                    szName, listenerPosition.x, listenerPosition.y, listenerPosition.z, listenerDirection.x, listenerDirection.y, listenerDirection.z, listenerVelocity);
	}
#endif  // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE
}

//////////////////////////////////////////////////////////////////////////
void CImpl::DrawDebugInfoList(IRenderAuxGeom& auxGeom, float& posX, float posY, Vec3 const& camPos, float const debugDistance, char const* const szTextFilter) const
{
#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
	if ((g_cvars.m_debugListFilter & g_debugListMask) != 0)
	{
		CryFixedStringT<MaxControlNameLength> lowerCaseSearchString(szTextFilter);
		lowerCaseSearchString.MakeLower();

		float const initialPosY = posY;

		if ((g_cvars.m_debugListFilter & EDebugListFilter::CueInstances) != 0)
		{
			auxGeom.Draw2dLabel(posX, posY, Debug::g_listHeaderFontSize, Debug::s_globalColorHeader, false, "Adx2 Cues [%" PRISIZE_T "]", g_constructedCueInstances.size());
			posY += Debug::g_listHeaderLineHeight;

			for (auto const pCueInstance : g_constructedCueInstances)
			{
				Vec3 const& position = pCueInstance->GetObject().GetTransformation().GetPosition();
				float const distance = position.GetDistance(camPos);

				if ((debugDistance <= 0.0f) || ((debugDistance > 0.0f) && (distance < debugDistance)))
				{
					char const* const szCueName = pCueInstance->GetCue().GetName();
					CryFixedStringT<MaxControlNameLength> lowerCaseCueName(szCueName);
					lowerCaseCueName.MakeLower();
					bool const draw = ((lowerCaseSearchString.empty() || (lowerCaseSearchString == "0")) || (lowerCaseCueName.find(lowerCaseSearchString) != CryFixedStringT<MaxControlNameLength>::npos));

					if (draw)
					{
						ECueInstanceFlags const flags = pCueInstance->GetFlags();
						ColorF color = Debug::s_listColorItemActive;
						char const* const szListenerName = (pCueInstance->GetObject().GetListener() != nullptr) ? pCueInstance->GetObject().GetListener()->GetName() : "No Listener!";

						CryFixedStringT<MaxMiscStringLength> debugText;
						debugText.Format("%s on %s (%s)", szCueName, pCueInstance->GetObject().GetName(), szListenerName);

						if ((flags& ECueInstanceFlags::IsPending) != ECueInstanceFlags::None)
						{
							color = Debug::s_listColorItemLoading;
						}
						else if ((flags& ECueInstanceFlags::IsVirtual) != ECueInstanceFlags::None)
						{
							color = Debug::s_globalColorVirtual;
						}
						else if ((flags& ECueInstanceFlags::IsStopping) != ECueInstanceFlags::None)
						{
							float const fadeOutTime = pCueInstance->GetCue().GetFadeOutTime();
							float const remainingTime = std::max(0.0f, fadeOutTime - (gEnv->pTimer->GetAsyncTime().GetSeconds() - pCueInstance->GetTimeFadeOutStarted()));

							debugText.Format("%s on %s (%s) (%.2f sec)", szCueName, pCueInstance->GetObject().GetName(), szListenerName, remainingTime);
							color = Debug::s_listColorItemStopping;
						}

						auxGeom.Draw2dLabel(posX, posY, Debug::g_listFontSize, color, false, "%s", debugText.c_str());

						posY += Debug::g_listLineHeight;
					}
				}
			}

			posX += 600.0f;
		}

		if ((g_cvars.m_debugListFilter & EDebugListFilter::GameVariables) != 0)
		{
			posY = initialPosY;

			auxGeom.Draw2dLabel(posX, posY, Debug::g_listHeaderFontSize, Debug::s_globalColorHeader, false, "Adx2 GameVariables [%" PRISIZE_T "]", g_gameVariableValues.size());
			posY += Debug::g_listHeaderLineHeight;

			for (auto const& gameVariablePair : g_gameVariableValues)
			{
				char const* const szGameVariableName = gameVariablePair.first.c_str();
				CryFixedStringT<MaxControlNameLength> lowerCaseGameVariableName(szGameVariableName);
				lowerCaseGameVariableName.MakeLower();
				bool const draw = ((lowerCaseSearchString.empty() || (lowerCaseSearchString == "0")) || (lowerCaseGameVariableName.find(lowerCaseSearchString) != CryFixedStringT<MaxControlNameLength>::npos));

				if (draw)
				{
					auxGeom.Draw2dLabel(posX, posY, Debug::g_listFontSize, Debug::s_listColorItemActive, false, "%s : %3.2f", szGameVariableName, gameVariablePair.second);

					posY += Debug::g_listLineHeight;
				}
			}

			posX += 300.0f;
		}

		if ((g_cvars.m_debugListFilter & EDebugListFilter::Categories) != 0)
		{
			posY = initialPosY;

			auxGeom.Draw2dLabel(posX, posY, Debug::g_listHeaderFontSize, Debug::s_globalColorHeader, false, "Adx2 Categories [%" PRISIZE_T "]", g_categoryValues.size());
			posY += Debug::g_listHeaderLineHeight;

			for (auto const& categoryPair : g_categoryValues)
			{
				char const* const szCategoryName = categoryPair.first.c_str();
				CryFixedStringT<MaxControlNameLength> lowerCaseCategoryName(szCategoryName);
				lowerCaseCategoryName.MakeLower();
				bool const draw = ((lowerCaseSearchString.empty() || (lowerCaseSearchString == "0")) || (lowerCaseCategoryName.find(lowerCaseSearchString) != CryFixedStringT<MaxControlNameLength>::npos));

				if (draw)
				{
					auxGeom.Draw2dLabel(posX, posY, Debug::g_listFontSize, Debug::s_listColorItemActive, false, "%s : %3.2f", szCategoryName, categoryPair.second);

					posY += Debug::g_listLineHeight;
				}
			}

			posX += 300.0f;
		}
	}
#endif  // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE
}
} // namespace Adx2
} // namespace Impl
} // namespace CryAudio
