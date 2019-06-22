%include "CryEngine.swig"

%import "CryCommon.i"
%import "CryEntitySystem.i"

%{
#include <CryEntitySystem/IEntity.h>
#include <CryAISystem/IAISystemComponent.h>
#include <CryAISystem/IAISystem.h>
#include <CryAISystem/IAgent.h>
#include <CryAISystem/IAIAction.h>
#include <CryAISystem/IAIActorProxy.h>
#include <CryAISystem/IAIDebugRenderer.h>
#include <CryAISystem/IAIGroupProxy.h>
#include <CryAISystem/IAIObject.h>
#include <CryAISystem/IAIObjectManager.h>
#include <CryAISystem/IPathfinder.h>
#include <CryAISystem/IAIActor.h>
#include <CryAISystem/IAIRateOfDeathHandler.h>
#include <CryAISystem/IAIRecorder.h>
#include <CryAISystem/IBlackBoard.h>
#include <CryAISystem/IInterestSystem.h>
#include <CryAISystem/IClusterDetector.h>
#include <CryAISystem/ICoverSystem.h>
#include <CryAISystem/ICommunicationManager.h>
#include <CryAISystem/IVisionMap.h>
#include <CryAISystem/IFactionMap.h>
#include <CryAISystem/IGoalPipe.h>
#include <CryAISystem/ITacticalPointSystem.h>
#include <CryAISystem/INavigation.h>
#include <CryAISystem/INavigationSystem.h>
#include <CryAISystem/IMovementSystem.h>
#include <CryAISystem/IOffMeshNavigationManager.h>
#include <CryAISystem/MovementRequest.h>
#include <CryAISystem/MovementRequestID.h>

#include <CryAISystem/BehaviorTree/IBehaviorTree.h>
#include <CryAISystem/BehaviorTree/Node.h>

using namespace BehaviorTree;
%}

%typemap(csbase) EEntityAspects "ushort"
%typemap(csbase) EAILoadDataFlag "ushort"
%typemap(csbase) IMNMCustomPathCostComputer::EComputationType "uint"
%typemap(csbase) MNM::Constants::Edges "uint"
%typemap(csbase) EMNMDangers "uint"
%typemap(csbase) ICoverUser::EStateFlags "byte"
%typemap(csbase) INavigationSystem::EMeshFlag "uint"

%ignore IAIEngineModule;

%include "../../../../CryEngine/CryCommon/CryAISystem/NavigationSystem/NavigationIdTypes.h"
%template(NavigationMeshID) TNavigationID<MeshIDTag>;
%template(NavigationAgentTypeID) TNavigationID<AgentTypeIDTag>;
%template(NavigationVolumeID) TNavigationID<VolumeIDTag>;
%template(TileGeneratorExtensionID) TNavigationID<TileGeneratorExtensionIDTag>;
%template(TileIDTag) TNavigationID<TileIDTag>;
%template(TileTriangleIDTag) TNavigationID<TileTriangleIDTag>;
%template(OffMeshLinkIDTag) TNavigationID<OffMeshLinkIDTag>;
%feature("director") IAISystemComponent;
%include "../../../../CryEngine/CryCommon/CryAISystem/IAISystemComponent.h"
%feature("director") IAIEventListener;
%feature("director") IAIGlobalPerceptionListener;
%ignore EAICollisionEntities;
%ignore IAISystem::SerializeObjectIDs;
%ignore IAISystem::Serialize;
%ignore IAISystem::NavCapMask::Serialize;
%ignore IAISystem::ESubsystemUpdateFlag;
%include "../../../../CryEngine/CryCommon/CryAISystem/IAISystem.h"
%feature("director") IAICommunicationHandler::IEventListener;
%include "../../../../CryEngine/CryCommon/CryAISystem/IAgent.h"
%feature("director") IAIActionListener;
%include "../../../../CryEngine/CryCommon/CryAISystem/IAIAction.h"
%include "../../../../CryEngine/CryCommon/CryAISystem/IAIActorProxy.h"
%include "../../../../CryEngine/CryCommon/CryAISystem/IAIDebugRenderer.h"
%include "../../../../CryEngine/CryCommon/CryAISystem/IAIGroupProxy.h"
%include "../../../../CryEngine/CryCommon/CryAISystem/IAIObject.h"
%include "../../../../CryEngine/CryCommon/CryAISystem/IAIObjectManager.h"
%ignore IAIPathFinder::RegisterPathFinderListener;
%ignore IAIPathFinder::UnregisterPathFinderListener;
%include "../../../../CryEngine/CryCommon/CryAISystem/IPathfinder.h"
%typemap(ctype) IAISystem::ENavigationType* "IAISystem::ENavigationType *"
%typemap(imtype) IAISystem::ENavigationType* "ref IAISystem.ENavigationType"
%typemap(cstype) IAISystem::ENavigationType* "ref IAISystem.ENavigationType"
%typemap(csin) IAISystem::ENavigationType* "$csinput" 
%feature("director") IActorBehaviorListener;
%include "../../../../CryEngine/CryCommon/CryAISystem/IAIActor.h"
%include "../../../../CryEngine/CryCommon/CryAISystem/IAIRateOfDeathHandler.h"
%feature("director") IAIRecorderListener;
%include "../../../../CryEngine/CryCommon/CryAISystem/IAIRecorder.h"
%include "../../../../CryEngine/CryCommon/CryAISystem/IBlackBoard.h"
%include "../../../../CryEngine/CryCommon/CryAISystem/IInterestSystem.h"
%include "../../../../CryEngine/CryCommon/CryAISystem/IClusterDetector.h"
%include "../../../../CryEngine/CryCommon/CryAISystem/ICoverSystem.h"
%include "../../../../CryEngine/CryCommon/CryAISystem/ICommunicationManager.h"
%typemap(csbase) EChangeHint "uint"
%include "../../../../CryEngine/CryCommon/CryAISystem/IVisionMap.h"
%include "../../../../CryEngine/CryCommon/CryAISystem/IFactionMap.h"
%include "../../../../CryEngine/CryCommon/CryAISystem/IGoalPipe.h"
%include "../../../../CryEngine/CryCommon/CryAISystem/ITacticalPointSystem.h"
%template(TPSQueryID) STicket<1>;
%template(TPSQueryTicket) STicket<2>;
%include "../../../../CryEngine/CryCommon/CryAISystem/INavigation.h"
%include "../../../../CryEngine/CryCommon/CryAISystem/INavigationSystem.h"
%include "../../../../CryEngine/CryCommon/CryAISystem/MovementRequest.h"
%feature("director") CLastingMovementRequest;
class CLastingMovementRequest : public MovementRequest
{
public:
	CLastingMovementRequest()
	{
		callback = functor(*this, &CLastingMovementRequest::OnMovementRequestCallback);
	}
	virtual void OnMovementRequestCallback(const MovementRequestResult& requestResult) { }
};
%{
class CLastingMovementRequest : public MovementRequest
{
public:
	CLastingMovementRequest()
	{
		callback = functor(*this, &CLastingMovementRequest::OnMovementRequestCallback);
	}
	virtual void OnMovementRequestCallback(const MovementRequestResult& requestResult) { }
};
%}
%include "../../../../CryEngine/CryCommon/CryAISystem/MovementRequestID.h"
%include "../../../../CryEngine/CryCommon/CryAISystem/MovementBlock.h"
%ignore MovementStyle::ConstructDictionariesIfNotAlreadyDone;
%include "../../../../CryEngine/CryCommon/CryAISystem/MovementStyle.h"
%include "../../../../CryEngine/CryCommon/CryAISystem/IMovementSystem.h"
%include "../../../../CryEngine/CryCommon/CryAISystem/IOffMeshNavigationManager.h"

%ignore BehaviorTree::BehaviorVariablesContext;
%ignore BehaviorTree::UpdateContext::variables;
%feature("director") INode;
%include "../../../../CryEngine/CryCommon/CryAISystem/BehaviorTree/BehaviorTreeDefines.h"
%include "../../../../CryEngine/CryCommon/CryAISystem/BehaviorTree/IBehaviorTree.h"
%include <typemaps.i>
%apply stack_string *OUTPUT { stack_string& debugText };
%feature("director") NodeProxy;
%include "../../../../CryEngine/CryCommon/CryAISystem/BehaviorTree/Node.h"
namespace BehaviorTree
{
class NodeProxy : public Node
{
	struct RuntimeData {};
public:
	NodeProxy() {}
	virtual void HandleEvent(const EventContext& context, const Event& event) {}
};
}
%{
namespace BehaviorTree
{
class NodeProxy : public Node
{
	struct RuntimeData {};
public:
	NodeProxy() {}
	virtual void HandleEvent(const EventContext& context, const Event& event) {}
};
}
%}