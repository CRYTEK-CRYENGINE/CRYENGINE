// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __WIN32GUI_H__
#define __WIN32GUI_H__

#include <Windows.h>
#include <string>

namespace Win32GUI
{
	void Initialize();
	void RegisterFrameClass(const TCHAR* name);
	HWND CreateFrame(const TCHAR* className, unsigned style, int width, int height);
	HWND CreateControl(const TCHAR* className, unsigned style, HWND parent, int left, int top, int width, int height);
	int Run();
	std::string GetWindowString(HWND hwnd);
	void SetWindowString(HWND hwnd, const std::string& text);
	HFONT CreateFont();

	namespace EventCallbacks
	{
		class ICallback
		{
		public:
			virtual ~ICallback() {}
			virtual void Release() = 0;
		};

		struct IVoidCallback : public ICallback
		{
			virtual void Call() = 0;
		};

		struct VoidCallback
		{
			template <typename O> struct Callback : public IVoidCallback
			{
				typedef void (O::*Signature)();

				Callback(O* object, Signature method): m_object(object), m_method(method) {}
				virtual void Release() {delete this;}
				virtual void Call() {(m_object->*m_method)();}
				O* m_object;
				Signature m_method;
			};
		};

		struct IStringCallback : public ICallback
		{
			virtual void Call(const std::string& text) = 0;
		};

		struct StringCallback
		{
			template <typename O> struct Callback : public IStringCallback
			{
				typedef void (O::*Signature)(const std::string& text);

				Callback(O* object, Signature method): m_object(object), m_method(method) {}
				virtual void Release() {delete this;}
				virtual void Call(const std::string& text) {(m_object->*m_method)(text);}
				O* m_object;
				Signature m_method;
			};
		};

		struct IGetDimensionsCallback : public ICallback
		{
			virtual void Call(int& minW, int& maxW, int& minH, int& maxH) = 0;
		};

		struct GetDimensionsCallback
		{
			template <typename O> struct Callback : public IGetDimensionsCallback
			{
				typedef void (O::*Signature)(int& minW, int& maxW, int& minH, int& maxH);

				Callback(O* object, Signature method): m_object(object), m_method(method) {}
				virtual void Release() {delete this;}
				virtual void Call(int& minW, int& maxW, int& minH, int& maxH) {(m_object->*m_method)(minW, maxW, minH, maxH);}
				O* m_object;
				Signature m_method;
			};
		};

		struct ISizeCallback : public ICallback
		{
			virtual void Call(int width, int height) = 0;
		};

		struct SizeCallback
		{
			template <typename O> struct Callback : public ISizeCallback
			{
				typedef void (O::*Signature)(int width, int height);

				Callback(O* object, Signature method): m_object(object), m_method(method) {}
				virtual void Release() {delete this;}
				virtual void Call(int width, int height) {(m_object->*m_method)(width, height);}
				O* m_object;
				Signature m_method;
			};
		};

		struct IBoolCallback : public ICallback
		{
			virtual void Call(bool value) = 0;
		};

		struct BoolCallback
		{
			template <typename O> struct Callback : public IBoolCallback
			{
				typedef void (O::*Signature)(bool callback);
				Callback(O* object, Signature method): m_object(object), m_method(method) {}
				virtual void Release() {delete this;}
				virtual void Call(bool value) {(m_object->*m_method)(value);}
				O* m_object;
				Signature m_method;
			};
		};

		struct TextChanged : public StringCallback {enum {ID = 0x00005001};};
		struct GetDimensions : public GetDimensionsCallback {enum {ID = 0x00005002};};
		struct SizeChanged : public SizeCallback {enum {ID = 0x00005003};};
		struct Pushed : public VoidCallback {enum {ID = 0x00005004};};
		struct Checked : public BoolCallback {enum {ID = 0x00005005};};
	};

	void SetCallbackObject(HWND hwnd, unsigned eventID, EventCallbacks::ICallback* callback);
	template <typename T, typename O> inline void SetCallback(HWND hwnd, O* object, typename T::template Callback<O>::Signature method);
}

template <typename T, typename O> inline void Win32GUI::SetCallback(HWND hwnd, O* object, typename T::template Callback<O>::Signature method)
{
	typedef T::template Callback<O> Callback;
	Callback* callback = new Callback(object, method);
	SetCallbackObject(hwnd, T::ID, callback);
}

#endif //__WIN32GUI_H__
