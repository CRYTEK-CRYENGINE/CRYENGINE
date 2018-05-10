// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __TOGGLEBUTTON_H__
#define __TOGGLEBUTTON_H__

#include "IUIComponent.h"
#include <string>

class ToggleButton : public IUIComponent
{
public:
	template <typename T> ToggleButton(const TCHAR* text, T* object, void (T::*method)(bool value));
	~ToggleButton();

	void SetState(bool value);

	// IUIComponent
	virtual void CreateUI(void* window, int left, int top, int width, int height);
	virtual void Resize(void* window, int left, int top, int width, int height);
	virtual void DestroyUI(void* window);
	virtual void GetExtremeDimensions(void* window, int& minWidth, int& maxWidth, int& minHeight, int& maxHeight);

private:
	ToggleButton(const ToggleButton&);
	ToggleButton& operator=(const ToggleButton);

	struct ICallback
	{
		virtual void Release() = 0;
		virtual void Call(bool value) = 0;
	};

	template <typename T> struct Callback : public ICallback
	{
		Callback(T* object, void (T::*method)(bool value)): object(object), method(method) {}
		virtual void Release() {delete this;}
		virtual void Call(bool value) {(object->*method)(value);}
		T* object;
		void (T::*method)(bool value);
	};

	void OnChecked(bool checked);

	tstring m_text;
	void* m_button;
	void* m_font;
	bool m_state;
	ICallback* m_callback;
};

template <typename T> ToggleButton::ToggleButton(const TCHAR* text, T* object, void (T::*method)(bool value))
	: m_text(text)
	, m_button(0)
	, m_font(0)
	, m_state(false)
	, m_callback(new Callback<T>(object, method))
{
}

#endif //__TOGGLEBUTTON_H__
