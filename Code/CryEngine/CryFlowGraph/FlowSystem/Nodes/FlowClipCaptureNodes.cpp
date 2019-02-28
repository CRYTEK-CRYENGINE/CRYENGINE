// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   Description: Node to record in-game video clips.

   -------------------------------------------------------------------------
   History:

*************************************************************************/

#include "StdAfx.h"

#include <CryCore/Platform/IPlatformOS.h>
#include <CryString/UnicodeFunctions.h>
#include <CryFlowGraph/IFlowBaseNode.h>

// ------------------------------------------------------------------------
class CClipCaptureManagement : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CClipCaptureManagement(SActivationInfo* pActInfo)
	{
	}

	~CClipCaptureManagement()
	{
	}

	void Serialize(SActivationInfo*, TSerialize ser)
	{
	}

	void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	enum EInputs
	{
		eI_Capture = 0,
		eI_DurationBefore,
		eI_DurationAfter,
		eI_ClipId,            // in Xbox One this is the MagicMomentID
		eI_LocalizedClipName, // in Xbox One this text is shown in the Toast message
		eI_Metadata,
	};

	enum EOutputs
	{
		eO_BeganCapture = 0,
		eO_Error,
	};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig_AnyType("Capture",           _HELP("Begins Capturing a Clip")),
			InputPortConfig<float>("DurationBefore",     20.0f,                                                                                                               _HELP("Record that many seconds before the Capture input is triggered")),
			InputPortConfig<float>("DurationAftere",     10.0f,                                                                                                               _HELP("Record that many seconds after the Capture input is triggered")),
			InputPortConfig<string>("ClipName",          _HELP("In XboxOne: MagicMoment ID used to look up the description string entered throuhg the Xbox Developr Portal")),
			InputPortConfig<string>("LocalizedClipName", _HELP("In XboxOne: Clip's Name shown during the Toast pop up")),
			InputPortConfig<string>("Metadata",          _HELP("Optional. Use it for instance to tag clips")),
			{ 0 }
		};

		static const SOutputPortConfig outputs[] = {
			OutputPortConfig_Void("BeganCapture", _HELP("The clip's capture has begun")),
			OutputPortConfig_Void("Error",        _HELP("An error happened during the capturing process")),
			{ 0 }
		};

		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Allows capturing clips while the game is running and (in XboxOne) save them locally or in the cloud");
		config.SetCategory(EFLN_APPROVED);
	}

#define ENABLE_GAMEDVR_NODE
	// Note: Be aware that the node may be placed in the levels in such a way that all users get a similar
	// recording or the same user gets many similar recordings. This may be unacceptable. In case you want to
	// disable video clip recording via flow graph nodes w/o removing the nodes,
	// please comment out #define ENABLE_GAMEDVR_NODE (above this comment)

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
#ifdef ENABLE_GAMEDVR_NODE
		switch (event)
		{
		case eFE_Activate:
			{
				if (IPlatformOS::IClipCaptureOS* pClipCapture = gEnv->pSystem->GetPlatformOS()->GetClipCapture())
				{
					if (pActInfo && IsPortActive(pActInfo, eI_Capture))
					{
						const IPlatformOS::IClipCaptureOS::SSpan span(GetPortFloat(pActInfo, eI_DurationBefore), GetPortFloat(pActInfo, eI_DurationAfter));
						const string& clipName = GetPortString(pActInfo, eI_ClipId);
						const string& toast = GetPortString(pActInfo, eI_LocalizedClipName);
						const string& metadata = GetPortString(pActInfo, eI_Metadata);

						string localizedToast;
						wstring wideToast;
						gEnv->pSystem->GetLocalizationManager()->LocalizeString(toast, localizedToast);
						Unicode::Convert(wideToast, localizedToast);

						IPlatformOS::IClipCaptureOS::SClipTextInfo clipTextInfo(
						  clipName.c_str(),
						  wideToast.c_str(),
						  metadata.c_str());

						const bool ok = pClipCapture->RecordClip(clipTextInfo, span);

						ActivateOutput(pActInfo, ok ? eO_BeganCapture : eO_Error, true);
					}
				}
			}
		}
#endif
	}

#undef ENABLE_GAMEDVR_NODE

};

REGISTER_FLOW_NODE("Video:ClipCapture", CClipCaptureManagement);
