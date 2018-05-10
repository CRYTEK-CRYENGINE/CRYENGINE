// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   -------------------------------------------------------------------------
   File name:   ScriptBind_AI.h
   Description: Goal Op Factory interface and management classes

   -------------------------------------------------------------------------
   History:
   -24:02:2008   - Created by Matthew

 *********************************************************************/

#ifndef __GoalOpFactory_H__
#define __GoalOpFactory_H__

#pragma once

#include <CryAISystem/IGoalPipe.h>

/** Interface for classes than can create and return a goalop instance */
struct IGoalOpFactory
{
	virtual ~IGoalOpFactory(){}
	/**
	 * Create a goalop instance
	 * Attempts to create a goalop instance from the goalop name and script arguments.
	 * Should return NULL if not recognised.
	 * @return A new goalop instance or NULL, and correct parameters
	 */
	virtual IGoalOp* GetGoalOp(const char* sGoalOpName, IFunctionHandler* pH, int nFirstParam, GoalParameters& params) const = 0;

	/**
	 * Create a goalop instance
	 * Attempts to create a goalop instance from the goalop id and parameters.
	 * Should return NULL if not recognised.
	 * @return A new goalop instance or NULL
	 */
	virtual IGoalOp* GetGoalOp(EGoalOperations name, GoalParameters& params) const = 0;
};

/**
 * An ordering of goalop factories.
 * Attempts to delegate to a series of factories in the order they were added and returns the first result.
 * Should return NULL if none of them recognize the goalop.
 * An empty ordering will always return NULL;
 * @return A new goalop instance or NULL
 */
class CGoalOpFactoryOrdering : public IGoalOpFactory
{
public:
	IGoalOp* GetGoalOp(const char* sGoalOpName, IFunctionHandler* pH, int nFirstParam, GoalParameters& params) const;

	IGoalOp* GetGoalOp(EGoalOperations op, GoalParameters& params) const;

	/** Add a factory to the ordering. */
	void AddFactory(IGoalOpFactory* pFactory);

	/** Reserve space for factories. */
	void PrepareForFactories(size_t count);

	/**
	 * Destroy all factories in the ordering.
	 * Result is is an empty ordering.
	 */
	void DestroyAll(void);

protected:
	typedef std::vector<IGoalOpFactory*> TFactoryVector;
	TFactoryVector m_Factories;
};

#endif  // __GoalOpFactory_H__
