// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
////////////////////////////////////////////////////////////////////////////
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2011.
// -------------------------------------------------------------------------
//  File name:   LensFlareUndo.h
//  Created:     12/Dec/2012 by Jaesik.
////////////////////////////////////////////////////////////////////////////

class CLensFlareItem;

class CUndoLensFlareItem : public IUndoObject
{
public:
	CUndoLensFlareItem(CLensFlareItem* pGroupItem, const char* undoDescription = "Undo Lens Flare Tree");
	~CUndoLensFlareItem();

protected:
	const char* GetDescription() { return m_undoDescription; };
	void        Undo(bool bUndo);
	void        Redo();

private:

	string m_undoDescription;

	struct SData
	{
		SData()
		{
			m_pOptics = NULL;
		}

		string               m_selectedFlareItemName;
		bool                  m_bRestoreSelectInfo;
		IOpticsElementBasePtr m_pOptics;
	};

	string m_flarePathName;
	SData   m_Undo;
	SData   m_Redo;

	void Restore(const SData& data);
};

class CUndoRenameLensFlareItem : public IUndoObject
{
public:

	CUndoRenameLensFlareItem(const char* oldFullName, const char* newFullName, bool bRefreshItemTreeWhenUndo = false, bool bRefreshItemTreeWhenRedo = false);

protected:
	const char* GetDescription() { return m_undoDescription; };
	void        Undo(bool bUndo);
	void        Redo();

private:

	string m_undoDescription;

	struct SUndoDataStruct
	{
		string m_oldFullItemName;
		string m_newFullItemName;
		bool    m_bRefreshItemTreeWhenUndo;
		bool    m_bRefreshItemTreeWhenRedo;
	};

	void Rename(const SUndoDataStruct& data, bool bRefreshItemTree);

	SUndoDataStruct m_undo;
	SUndoDataStruct m_redo;
};

class CUndoLensFlareElementSelection : public IUndoObject
{
public:
	CUndoLensFlareElementSelection(CLensFlareItem* pLensFlareItem, const char* flareTreeItemFullName, const char* undoDescription = "Undo Lens Flare Element Tree");
	~CUndoLensFlareElementSelection(){}

protected:
	const char* GetDescription() { return m_undoDescription; };
	void        Undo(bool bUndo);
	void        Redo();

private:

	string m_undoDescription;

	string m_flarePathNameForUndo;
	string m_flareTreeItemFullNameForUndo;

	string m_flarePathNameForRedo;
	string m_flareTreeItemFullNameForRedo;
};

class CUndoLensFlareItemSelectionChange : public IUndoObject
{
public:
	CUndoLensFlareItemSelectionChange(const char* fullLensFlareItemName, const char* undoDescription = "Undo Lens Flare element selection");
	~CUndoLensFlareItemSelectionChange(){}

protected:
	const char* GetDescription() { return m_undoDescription; };

	void        Undo(bool bUndo);
	void        Redo();

private:
	string m_undoDescription;
	string m_FullLensFlareItemNameForUndo;
	string m_FullLensFlareItemNameForRedo;
};

