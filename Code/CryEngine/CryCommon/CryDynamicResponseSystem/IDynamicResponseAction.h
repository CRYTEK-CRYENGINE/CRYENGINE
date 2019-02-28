// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/************************************************************************

   An Interface for actions that can be executed by Response Segments

   /************************************************************************/

#ifndef _DYNAMICRESPONSEACTION_H_
#define _DYNAMICRESPONSEACTION_H_

#include <CrySystem/XML/IXml.h>
#include <CrySerialization/Forward.h>
#include "IDynamicResponseSystem.h"

namespace DRS
{
struct IResponseInstance;
struct IResponseActionInstance;

typedef std::unique_ptr<IResponseActionInstance> IResponseActionInstanceUniquePtr;

struct IResponseAction : public IEditorObject
{
	virtual ~IResponseAction() = default;

	/**
	 * Will execute the action. If the action is not an instantaneous action, it can return an ActionInstance, which is then updated as long as needed by the DRS to finish the execution of the action
	 *
	 * Note: its called by the DRS system, when a response segment is executed which contains a action of this type
	 * @param pResponseInstance - the ResponseInstance that requested the execution of the action. From the ResponseInstance the active actor and the context variables of signal can be obtained, if needed for the execution
	 * @return if needed an actionInstance is returned. If the action was already completed during the function 0 will be returned. (For instant-effect-actions like setVariable)
	 * @see IResponseInstance
	 */
	virtual IResponseActionInstanceUniquePtr Execute(IResponseInstance* pResponseInstance) = 0;

	/**
	 * Serializes all the member variables of this class
	 *
	 * Note: Called by the serialization library to serialize to disk or to the UI
	 */
	virtual void Serialize(Serialization::IArchive& ar) = 0;

};

//! One concrete running instance of the actions. (There might be actions that dont need instances, because their action is instantaneously.
struct IResponseActionInstance
{
	//these states are returned by the update method and will influence the further execution of the corresponding response instance
	enum eCurrentState
	{
		CS_RUNNING, //the action is not finished and will be updated furthermore. The response instance will NOT continue with the next follow-up response. Therefore processing is blocked, until all currently RUNNING action instances are done.
		CS_FINISHED, //tells the response-instance that the action has finished successfully. The response instance is allowed to continue with the next follow-up response.
		CS_CANCELED,  //tells the response-instance that the action was canceled. The response instance is allowed to continue with the next follow-up response.
		CS_ERROR, //tells the response-instance that the action could not finish. The response instance is allowed to continue with the next follow-up response.
		CS_RUNNING_NON_BLOCKING,  //the action is not done, but the response-instance should not wait for it to finish. The response instance is allowed to continue with the next follow-up response. Can be useful for actions that should be updated the whole time while the response instance is running, remark: that the actionInstance will still be canceled when the response instance is done.
	};

	virtual ~IResponseActionInstance() = default;

	/**
	 * This method continues the execution of an started action that takes some time to finish. Its called from the DRS System until it stops returning CS_RUNNING.
	 *
	 * Note: its called by the DRS system, as long as the action instance is running
	 * @return returns the current state of the execution of the action. CS_RUNNING means still needs time to finish, CS_FINISHED means finished successful,
	 * @return CS_CANCELED means the action was canceled from the outside before it was finished, CS_ERROR means an error happened during execution, CS_RUNNING_NON_BLOCKING means not finished, but the response instance is nevertheless allowed to continue with the next follow-up response
	 */
	virtual eCurrentState Update() = 0;

	/**
	 * Will be called when someone requested a cancellation of a running response (segment). Its up to the action instance how to handle this request.
	 * There might be cases for example, where the action decides to first finish the current sentence before actually stop. So the DRS will continue updating the ActionInstance,
	 * until the actionInstance decides to stop (by returning CS_CANCELED or CS_FINISHED in the Update method).
	 *
	 * Note: its called by the DRS system, when someone requested a cancellation of a running response (segment)
	 * @see Update()
	 */
	virtual void Cancel() = 0;
};
}

#endif  //_DYNAMICRESPONSEACTION_H_
