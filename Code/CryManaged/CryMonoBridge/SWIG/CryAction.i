%include "CryEngine.swig"

%import "CryCommon.i"

%import "CryEntitySystem.i"
%import "CryAnimation.i"

%ignore GetGameObjectExtensionRMIData;

%{
#include <CryNetwork/INetwork.h>
#include <IGameObject.h>
#include <CryEntitySystem/IEntityComponent.h>
#include <GameObjects/GameObject.h>
#include <ILevelSystem.h>
#include <CryAction/IActionMapManager.h>
#include <IActorSystem.h>
#include <IAnimatedCharacter.h>
#include <CryAudio/Dialog/IDialogSystem.h>
#include <CryAction/IMaterialEffects.h>
#include <IEffectSystem.h>

#include <ICheckpointSystem.h>
#include <IAnimationGraph.h>
#include <ICooperativeAnimationManager.h>
#include <IItemSystem.h>
#include <CryAction/ICustomActions.h>
#include <CryAction/ICustomEvents.h>
#include <CryAction/IDebugHistory.h>
#include <IGameObjectSystem.h>
#include <ILoadGame.h>
#include <ISaveGame.h>
#include <IViewSystem.h>
#include <IVehicleSystem.h>
#include <IGameRulesSystem.h>
#include <CrySystem/Scaleform/IFlashUI.h>
#include <IPlayerProfiles.h>
#include <ISubtitleManager.h>
#include <IForceFeedbackSystem.h>
#include <IGameplayRecorder.h>
#include <IGameSessionHandler.h>
#include <TestSystem/IGameStateRecorder.h>
#include <IMetadataRecorder.h>
#include <IUIDraw.h>
#include <IWeapon.h>
#include <IWorldQuery.h>
#include <CryAction/ILipSyncProvider.h>
%}

%ignore IGameFrameworkEngineModule;

%feature("director") IGameFrameworkListener;
%feature("director") IGameWarningsListener;
%feature("director") IBreakEventListener;
%include "../../../../CryEngine/CryCommon/CryGame/IGameFramework.h"

%import "../../../../CryEngine/CryCommon/CryNetwork/INetwork.h"

%csconstvalue("0xFFFFFFFF") eEA_All;
%typemap(csbase) EEntityAspects "uint"
%ignore GameWarning;

%feature("director") ILevelSystemListener;
%include "../../../CryEngine/CryAction/ILevelSystem.h"
%feature("director") IActionListener;
%feature("director") IBlockingActionListener;
%template(IActionFilterIteratorPtr) _smart_ptr<IActionFilterIterator>;
%template(IActionMapActionIteratorPtr) _smart_ptr<IActionMapActionIterator>;
%template(IActionMapIteratorPtr) _smart_ptr<IActionMapIterator>;
%template(IActorIteratorPtr) _smart_ptr<IActorIterator>;
%include "../../../CryEngine/CryCommon/CryAction/IActionMapManager.h"
%include "../../../CryEngine/CryAction/IActorSystem.h"
%csconstvalue("EMCMSlot.eMCMSlot_Game") eMCMSlotStack_Begin;
%csconstvalue("EMCMSlot.eMCMSlot_Animation+1") eMCMSlotStack_End;
%ignore animatedcharacter::Preload;
%ignore g_szMCMString;
%ignore g_szColliderModeString;
%ignore g_szColliderModeLayerString;
%include "../../../CryEngine/CryAction/IAnimatedCharacter.h"
%include "../../../../CryEngine/CryCommon/CryAudio/Dialog/IDialogSystem.h"
%csconstvalue("0") eFDT_Void;
%csconstvalue("1") eFDT_Int;
%csconstvalue("2") eFDT_Float;
%csconstvalue("3") eFDT_EntityId;
%csconstvalue("4") eFDT_Vec3;
%csconstvalue("5") eFDT_String;
%csconstvalue("6") eFDT_Bool;

%ignore SMFXParticleListNode::Create;
%ignore SMFXParticleListNode::Destroy;
%ignore SMFXParticleListNode::FreePool;
%ignore SMFXAudioListNode::Create;
%ignore SMFXAudioListNode::Destroy;
%ignore SMFXAudioListNode::FreePool;
%ignore SMFXFlowGraphListNode::Create;
%ignore SMFXFlowGraphListNode::Destroy;
%ignore SMFXFlowGraphListNode::FreePool;
%ignore SMFXDecalListNode::Create;
%ignore SMFXDecalListNode::Destroy;
%ignore SMFXDecalListNode::FreePool;
%ignore SMFXForceFeedbackListNode::Create;
%ignore SMFXForceFeedbackListNode::Destroy;
%ignore SMFXForceFeedbackListNode::FreePool;
%ignore SMFXResourceList::Create;
%ignore SMFXResourceList::FreePool;
%include "../../../../CryEngine/CryCommon/CryAction/IMaterialEffects.h"
%include "../../../CryEngine/CryAction/IEffectSystem.h"

%include "../../../CryEngine/CryAction/ICheckpointSystem.h"
%include "../../../CryEngine/CryAction/IAnimationGraph.h"
%include "../../../CryEngine/CryAction/ICooperativeAnimationManager.h"
%csconstvalue("2") eIPT_Float;
%csconstvalue("1") eIPT_Int;
%csconstvalue("4") eIPT_Vec3;
%csconstvalue("5") eIPT_String;
%ignore IInventory::SerializeInventoryForLevelChange;
%include "../../../CryEngine/CryAction/IItemSystem.h"
%ignore IItem::SerializeLTL;
%include "../../../CryEngine/CryAction/IItem.h"
%include "../../../../CryEngine/CryCommon/CryAction/ICustomActions.h"
%include "../../../../CryEngine/CryCommon/CryAction/ICustomEvents.h"
%include "../../../../CryEngine/CryCommon/CryAction/IDebugHistory.h"
%include "../../../CryEngine/CryAction/IGameObjectSystem.h"
%ignore ILoadGame::GetSection; // problem creating: std::unique_ptr< TSerialize >
%include "../../../CryEngine/CryAction/ILoadGame.h"
%ignore ISaveGame::AddSection;
%include "../../../CryEngine/CryAction/ISaveGame.h"
%include "../../../CryEngine/CryAction/IViewSystem.h"
%include "../../../CryEngine/CryAction/IVehicleSystem.h"
%ignore HitInfo::SerializeWith;
%ignore HitInfo::IsPartIDInvalid;
%ignore ExplosionInfo::SerializeWith;
%include "../../../CryEngine/CryAction/IGameRulesSystem.h"
%csconstvalue("6") eUIDT_Bool;
%csconstvalue("0") eUIDT_Int;
%csconstvalue("1") eUIDT_Float;
%csconstvalue("2") eUIDT_EntityId;
%csconstvalue("3") eUIDT_Vec3;
%csconstvalue("4") eUIDT_String;
%csconstvalue("5") eUIDT_WString;
%ignore IUIElement::CreateVariable; //ambiguous
%ignore IUIElement::CreateArray; //ambiguous
%include "../../../../CryEngine/CryCommon/CrySystem/Scaleform/IFlashUI.h"
%include "../../../CryEngine/CryAction/IPlayerProfiles.h"
%include "../../../CryEngine/CryAction/ISubtitleManager.h"
%include "../../../CryEngine/CryAction/IForceFeedbackSystem.h"
%include "../../../CryEngine/CryAction/IGameplayRecorder.h"
%include "../../../CryEngine/CryAction/IGameSessionHandler.h"
%include "../../../CryEngine/CryAction/TestSystem/IGameStateRecorder.h"
%include "../../../../CryEngine/CryCommon/CryAction/ILipSyncProvider.h"
%ignore IMetadata::CreateInstance;
%ignore IMetadata::Delete;
%ignore IMetadataRecorder::CreateInstance;
%ignore IMetadataRecorder::Delete;
%include "../../../CryEngine/CryAction/IMetadataRecorder.h"
%include "../../../CryEngine/CryAction/IMovementController.h"
%include "../../../CryEngine/CryAction/IUIDraw.h"
%include "../../../CryEngine/CryAction/IWeapon.h"
%include "../../../CryEngine/CryAction/IWorldQuery.h"

SMART_PTR_TEMPLATE(IAction)
SMART_PTR_TEMPLATE(IAttributeEnumerator)
%template(IEquipmentPackIteratorPtr) _smart_ptr<IEquipmentManager::IEquipmentPackIterator>;
SMART_PTR_TEMPLATE(ISaveGameEnumerator)
SMART_PTR_TEMPLATE(ISaveGameThumbnail)
SMART_PTR_TEMPLATE(IVehicleIterator)

%extend IGameObject {
	void RegisterCollisionEvent(IGameObjectExtension* piExtention)
	{
		const int eventToRegister[] = { eGFE_OnCollision };
		$self->RegisterExtForEvents(piExtention, eventToRegister, sizeof(eventToRegister)/sizeof(int));
	}
	void UnregisterCollisionEvent(IGameObjectExtension* piExtention)
	{
		const int eventToRegister[] = { eGFE_OnCollision };
		$self->UnRegisterExtForEvents(piExtention, eventToRegister, sizeof(eventToRegister)/sizeof(int));
	}
}