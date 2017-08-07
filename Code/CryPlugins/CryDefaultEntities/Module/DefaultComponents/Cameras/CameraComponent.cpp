#include "StdAfx.h"
#include "CameraComponent.h"

namespace Cry
{
	namespace DefaultComponents
	{
		void CCameraComponent::Register(Schematyc::CEnvRegistrationScope& componentScope)
		{
			// Functions
			{
				auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CCameraComponent::Activate, "{56EFC341-8541-4A85-9870-68F2774BDED4}"_cry_guid, "Activate");
				pFunction->SetDescription("Makes this camera active");
				pFunction->SetFlags(Schematyc::EEnvFunctionFlags::Construction);
				componentScope.Register(pFunction);
			}
			{
				auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CCameraComponent::IsActive, "{5DBFFF7C-7D82-4645-ACE3-561748376568}"_cry_guid, "IsActive");
				pFunction->SetDescription("Is this camera active?");
				pFunction->SetFlags(Schematyc::EEnvFunctionFlags::Construction);
				pFunction->BindOutput(0, 'iact', "IsActive");
				componentScope.Register(pFunction);
			}
			{
				auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CCameraComponent::OverrideAudioListenerTransform, "{46FA7961-95AF-4BF3-B7E8-A67ECE233EE3}"_cry_guid, "OverrideAudioListenerTransform");
				pFunction->SetDescription("Overrides the transformation of this camera's audio listener. Using this function will set Automatic Audio Listener to false, meaning that the audio listener will stay at the overridden position until this function is called again.");
				pFunction->SetFlags(Schematyc::EEnvFunctionFlags::Construction);
				pFunction->BindInput(1, 'tran', "Transform");
				componentScope.Register(pFunction);
			}
		}
	}
}