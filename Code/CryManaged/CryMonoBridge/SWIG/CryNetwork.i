%include "CryEngine.swig"

%import "CryCommon.i"

%{
#include <CryNetwork/INetwork.h>
#include <CryNetwork/ICryOnlineStorage.h>
#include <CryNetwork/ISimpleHttpServer.h>
%}
%csconstvalue("0xFFFFFFFFu") eEA_All;
%ignore INetworkEngineModule;
%ignore ISerializableInfo;
%ignore IBreakDescriptionInfo::SerialiseSimpleBreakage;
%ignore INetSender::ser;
%ignore IGameContext::SynchObject;
%ignore IGameContext::HandleRMI;
%ignore IGameContext::ReceiveSimpleBreakage;
%ignore SRMIBenchmarkParams::SerializeWith;
%ignore INetMessage::WritePayload;
%ignore IRMICppLogger::SerializeParams;
%ignore IRMIMessageBody::SerializeWith;
%ignore CreateNetwork;
%ignore operator<(const SMessageTag& left, const SMessageTag& right);
%ignore operator==(const SDefenseFileInfo& lhs, const SDefenseFileInfo& rhs);
%include "../../../../CryEngine/CryCommon/CryNetwork/INetwork.h"
%ignore g_szMCMString;
%ignore g_szColliderModeString;
%ignore g_szColliderModeLayerString;
%ignore ICrysis3AuthenticationHandler::SPersona::SPersona;
