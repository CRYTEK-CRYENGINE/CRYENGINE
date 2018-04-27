// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// Nodes for coordinate space transformations

#include "StdAfx.h"

#include <CryFlowGraph/IFlowBaseNode.h>


class CFlowGeometryTransformInSpaceNode	: public CFlowBaseNode<eNCT_Singleton>
{
	enum INPUTS
	{
		eIP_Calc = 0,
		eIP_Position,
		eIP_Orientation,
		eIP_PivotPosition,
		eIP_PivotOrientation,
		eIP_Translation,
		eIP_Rotation,
	};

	enum OUTPUTS
	{
		eOP_Done = 0,
		eOP_Position,
		eOP_Orientation,
	};

public:
	CFlowGeometryTransformInSpaceNode(SActivationInfo* pActInfo)
	{
	}

	virtual ~CFlowGeometryTransformInSpaceNode()
	{
	}

private:
	virtual void GetMemoryUsage(ICrySizer * s) const override
	{
		s->Add(*this);
	}

	virtual void GetConfiguration(SFlowNodeConfig &config) override
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void ("Calc",                         _HELP("Perform the specified transformation in pivot space.")),
			InputPortConfig<Vec3>("Position",         Vec3(ZERO), _HELP("Position that should be transformed in the pivot space.")),
			InputPortConfig<Vec3>("Orientation",      Vec3(ZERO), _HELP("Orientation in degrees that should be transformed in the pivot space.")),
			InputPortConfig<Vec3>("PivotPosition",    Vec3(ZERO), _HELP("Position of the pivot.")),
			InputPortConfig<Vec3>("PivotOrientation", Vec3(ZERO), _HELP("Orientation of the pivot.")),
			InputPortConfig<Vec3>("Translation",      Vec3(ZERO), _HELP("Translation to apply in the pivot space.")),
			InputPortConfig<Vec3>("Rotation",         Vec3(ZERO), _HELP("Rotation (in degrees) to apply in the pivot space.")),
			{0}
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_Void ("Done",        _HELP("Triggered when the calculations are done.")),
			OutputPortConfig<Vec3>("Position",    _HELP("Result of rotating the input Position around the Pivot with the angles specified in Rotation.")),
			OutputPortConfig<Vec3>("Orientation", _HELP("Result of rotating the input Orientation around the Pivot with the angles specified in Rotation.")),
			{0}
		};
		config.sDescription = _HELP( "Perform a transformation inside the space of a pivot." );
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo) override
	{
		switch (event)
		{
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, eIP_Calc))
				{
					const Vec3 inputPosition         = GetPortVec3(pActInfo, eIP_Position);
					const Vec3 inputOrientation      = GetPortVec3(pActInfo, eIP_Orientation);
					const Vec3 inputPivotPosition    = GetPortVec3(pActInfo, eIP_PivotPosition);
					const Vec3 inputPivotOrientation = GetPortVec3(pActInfo, eIP_PivotOrientation);
					const Vec3 inputTranslation      = GetPortVec3(pActInfo, eIP_Translation);
					const Vec3 inputRotation         = GetPortVec3(pActInfo, eIP_Rotation);

					const QuatT pivotToWorldSpace   = QuatT(Quat::CreateRotationXYZ(Ang3(DEG2RAD(inputPivotOrientation))), inputPivotPosition);
					const QuatT worldToPivotSpace   = pivotToWorldSpace.GetInverted();
					const QuatT localTransformation = QuatT(Quat::CreateRotationXYZ(Ang3(DEG2RAD(inputRotation))), inputTranslation);
					const QuatT fullTransformation  = pivotToWorldSpace * localTransformation * worldToPivotSpace;

					const QuatT inputTransform    = QuatT(Quat(Ang3(DEG2RAD(inputOrientation))), inputPosition);
					const QuatT resultTransform   = fullTransformation * inputTransform;
					const Ang3  resultOrientation = Ang3(resultTransform.q);

					const Vec3 outputPosition    = resultTransform.t;
					const Vec3 outputOrientation = RAD2DEG(Vec3(resultOrientation.x, resultOrientation.y, resultOrientation.z));

					ActivateOutput(pActInfo, eOP_Done, true);
					ActivateOutput(pActInfo, eOP_Position, outputPosition);
					ActivateOutput(pActInfo, eOP_Orientation, outputOrientation);
				}
			}
		}
	}
};


class CFlowGeometrySwitchCoordinateSpaceNode : public CFlowBaseNode<eNCT_Singleton>
{
	enum INPUTS
	{
		eIP_GetIntoSpace = 0,
		eIP_GetOutOfSpace,
		eIP_Position,
		eIP_Orientation,
		eIP_PivotPosition,
		eIP_PivotOrientation,
	};

	enum OUTPUTS
	{
		eOP_Done = 0,
		eOP_Position,
		eOP_Orientation,
	};

public:
	CFlowGeometrySwitchCoordinateSpaceNode(SActivationInfo* pActInfo)
	{
	}

	virtual ~CFlowGeometrySwitchCoordinateSpaceNode()
	{
	}

private:
	virtual void GetMemoryUsage(ICrySizer * s) const override
	{
		s->Add(*this);
	}

	virtual void GetConfiguration(SFlowNodeConfig &config) override
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void ("GetIntoPivotSpace",            _HELP("Transforms the specified Position and Orientation from world space to the specified space.")),
			InputPortConfig_Void ("GetOutOfPivotSpace",           _HELP("Transforms the specified Position and Orientation from the specified space to world space.")),
			InputPortConfig<Vec3>("Position",         Vec3(ZERO), _HELP("Position that should be transformed into (or out of) the Pivot space.")),
			InputPortConfig<Vec3>("Orientation",      Vec3(ZERO), _HELP("Orientation that should be transformed into (or out of) the Pivot space.")),
			InputPortConfig<Vec3>("PivotPosition",    Vec3(ZERO), _HELP("Position of the pivot (in world space).")),
			InputPortConfig<Vec3>("PivotOrientation", Vec3(ZERO), _HELP("Orientation of the pivot (in world space).")),
			{0}
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_Void ("Done",        _HELP("Triggered when the calculations are done.")),
			OutputPortConfig<Vec3>("Position",    _HELP("Result of transforming the Position into (or out of) the Pivot space.")),
			OutputPortConfig<Vec3>("Orientation", _HELP("Result of transforming the Rotation into (or out of) the Pivot space.")),
			{0}
		};
		config.sDescription = _HELP( "Conversion between spaces (coordinate systems)." );
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo) override
	{
		switch (event)
		{
		case eFE_Activate:
			{
				const bool bGetInto  = IsPortActive(pActInfo, eIP_GetIntoSpace);
				const bool bGetOutOf = IsPortActive(pActInfo, eIP_GetOutOfSpace);
				if (bGetInto || bGetOutOf)
				{
					const Vec3 inputPosition    = GetPortVec3(pActInfo, eIP_Position);
					const Vec3 inputOrientation = GetPortVec3(pActInfo, eIP_Orientation);
					const Vec3 pivotPosition    = GetPortVec3(pActInfo, eIP_PivotPosition);
					const Vec3 pivotOrientation = GetPortVec3(pActInfo, eIP_PivotOrientation);

					const QuatT pivotToWorldSpace = QuatT(Quat::CreateRotationXYZ(Ang3(DEG2RAD(pivotOrientation))), pivotPosition);
					
					const QuatT actualTransformation = bGetInto ? pivotToWorldSpace.GetInverted() : pivotToWorldSpace;

					const QuatT inputTransform    = QuatT(Quat::CreateRotationXYZ(Ang3(DEG2RAD(inputOrientation))), inputPosition);
					const QuatT resultTransform   = actualTransformation * inputTransform;
					const Ang3  resultOrientation = Ang3(resultTransform.q);

					const Vec3& outputPosition    = resultTransform.t;
					const Vec3  outputOrientation = RAD2DEG(Vec3(resultOrientation.x, resultOrientation.y, resultOrientation.z));

					ActivateOutput(pActInfo, eOP_Done, true);
					ActivateOutput(pActInfo, eOP_Position, resultTransform.t);
					ActivateOutput(pActInfo, eOP_Orientation, outputOrientation);
				}
			}
		}
	}
};


REGISTER_FLOW_NODE("Math:Geometry:TransformInSpace", CFlowGeometryTransformInSpaceNode);
REGISTER_FLOW_NODE("Math:Geometry:SwitchCoordinateSpace", CFlowGeometrySwitchCoordinateSpaceNode);
