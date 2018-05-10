// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __PUSHBUTTON_H__
#define __PUSHBUTTON_H__

#include "IUIComponent.h"
#include <string>

class PushButton : public IUIComponent
{
public:
	template <typename T> PushButton(const TCHAR* text, T* object, void (T::*method)());
	~PushButton();

	void Enable(bool enabled);

	// IUIComponent
	virtual void CreateUI(void* window, int left, int top, int width, int height);
	virtual void Resize(void* window, int left, int top, int width, int height);
	virtual void DestroyUI(void* window);
	virtual void GetExtremeDimensions(void* window, int& minWidth, int& maxWidth, int& minHeight, int& maxHeight);

private:
	PushButton(const PushButton&);
	PushButton& operator=(const PushButton&);

	struct ICallback
	{
		virtual void Release() = 0;
		virtual void Call() = 0;
	};

	template <typename T> struct Callback : public ICallback
	{
		Callback(T* object, void (T::*method)()): object(object), method(method) {}
		virtual void Release() {delete this;}
		virtual void Call() {(object->*method)();}
		T* object;
		void (T::*method)();
	};

	void OnPushed();

	std::basic_string<TCHAR> m_text;
	void* m_button;
	void* m_font;
	ICallback* m_callback;
	bool m_enabled;
};

template <typename T> PushButton::PushButton(const TCHAR* text, T* object, void (T::*method)())
	: m_text(text)
	, m_button(0)
	, m_font(0)
	, m_callback(new Callback<T>(object, method))
	, m_enabled(true)
{
}

#endif //__PUSHBUTTON_H__
