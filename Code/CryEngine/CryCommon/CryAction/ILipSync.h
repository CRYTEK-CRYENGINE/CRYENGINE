// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//! \cond INTERNAL

#pragma once

struct CryCharMorphParams;

//! Callback interface.
struct IDialogLoadSink
{
	// <interfuscator:shuffle>
	virtual ~IDialogLoadSink(){}
	virtual void OnDialogLoaded(struct ILipSync* pLipSync) = 0;
	virtual void OnDialogFailed(struct ILipSync* pLipSync) = 0;
	// </interfuscator:shuffle>
};

//! Interface to the LipSync system.
struct ILipSync
{
	// <interfuscator:shuffle>
	virtual ~ILipSync(){}

	//! Initializes and prepares the character for lip-synching.
	virtual bool Init(ISystem* pSystem, IEntity* pEntity) = 0;

	//! Releases all resources and deletes itself.
	virtual void Release() = 0;

	//! Load expressions from script.
	virtual bool LoadRandomExpressions(const char* pszExprScript, bool bRaiseError = true) = 0;

	//! Release expressions.
	virtual bool UnloadRandomExpressions() = 0;

	//! Loads a dialog for later playback.
	virtual bool LoadDialog(const char* pszFilename, int nSoundVolume, float fMinSoundRadius, float fMaxSoundRadius, float fClipDist, int nSoundFlags = 0, IScriptTable* pAITable = NULL) = 0;

	//! Releases all resources.
	virtual bool UnloadDialog() = 0;

	//! Plays a loaded dialog.
	virtual bool PlayDialog(bool bUnloadWhenDone = true) = 0;

	//! Stops (aborts) a dialog.
	virtual bool StopDialog() = 0;

	//! Do a specific expression.
	virtual bool DoExpression(const char* pszMorphTarget, CryCharMorphParams& MorphParams, bool bAnim = true) = 0;

	//! Stop animating the specified expression.
	virtual bool StopExpression(const char* pszMorphTarget) = 0;

	//! Updates animation & stuff.
	virtual bool Update(bool bAnimate = true) = 0;

	//! Set callback sink (see above).
	virtual void SetCallbackSink(IDialogLoadSink* pSink) = 0;
	// </interfuscator:shuffle>
};

//! \endcond