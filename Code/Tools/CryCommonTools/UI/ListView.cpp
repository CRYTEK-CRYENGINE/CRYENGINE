// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ListView.h"
#include "Win32GUI.h"
#include "resource.h"
#include "ModuleHelpers.h"
#include <Windows.h>
#include <CommCtrl.h>
#include <cstring>

ListView::ListView()
: m_list(0)
{
}

void ListView::Add(int imageIndex, const TCHAR* message)
{
	int itemCount = int(SendMessage((HWND)m_list, LVM_GETITEMCOUNT, 0, 0));

	LVITEM item;
	std::memset(&item, 0, sizeof(item));
	item.mask = LVIF_TEXT | LVIF_IMAGE;
	item.iItem = itemCount;
	item.iSubItem = 0;
	item.pszText = (TCHAR*)message;
	item.iImage = imageIndex;

	SendMessage((HWND)m_list, LVM_INSERTITEM, 0, (LPARAM)&item);
}

void ListView::Clear()
{
	SendMessage((HWND)m_list, LVM_DELETEALLITEMS, 0, 0);
}

void ListView::CreateUI(void* window, int left, int top, int width, int height)
{
	m_list = Win32GUI::CreateControl(WC_LISTVIEW, LVS_REPORT | LVS_NOCOLUMNHEADER, (HWND)window, left, top, width, height);

	LVCOLUMN column;
	std::memset(&column, 0, sizeof(column));
	column.mask = LVCF_TEXT | LVCF_WIDTH;
	column.pszText = _T("Message");
	column.cx = width;
	SendMessage((HWND)m_list, LVM_INSERTCOLUMN, 0, (LPARAM)&column);

	HIMAGELIST imageList = (HIMAGELIST)CreateImageList();
	SendMessage((HWND)m_list, LVM_SETIMAGELIST, LVSIL_SMALL, (LPARAM)imageList);
}

void ListView::Resize(void* window, int left, int top, int width, int height)
{
	MoveWindow((HWND)m_list, left, top, width, height, true);
	SendMessage((HWND)m_list, LVM_SETCOLUMNWIDTH, 0, width);
}

void ListView::DestroyUI(void* window)
{
	DestroyWindow((HWND)m_list);
	m_list = 0;
}

void ListView::GetExtremeDimensions(void* window, int& minWidth, int& maxWidth, int& minHeight, int& maxHeight)
{
	minWidth = 20;
	maxWidth = 2000;
	minHeight = 20;
	maxHeight = 2000;
}

void* ListView::CreateImageList()
{
	HINSTANCE instance = ModuleHelpers::GetCurrentModule(ModuleHelpers::CurrentModuleSpecifier_Library);

	HBITMAP image = (HBITMAP)LoadImage(instance, MAKEINTRESOURCE(IDB_LOG_ICONS), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);
	DIBSECTION dibSection;
	GetObject(image, sizeof(dibSection), &dibSection);
	int height = dibSection.dsBmih.biHeight;
	int width = height;
	int count = dibSection.dsBmih.biWidth / width;
	HIMAGELIST imageList = ImageList_Create(16, 16, ILC_COLOR32, count, 0);
	ImageList_Add(imageList, image, 0);
	return imageList;
}
