#include "StdAfx.h"
#include "InputComponent.h"

static void ReflectType(Schematyc::CTypeDesc<EActionInputDevice>& desc)
{
	desc.SetGUID("{91C5E76B-8DB1-409B-8326-DA99E2CFF6E9}"_cry_guid);
	desc.SetLabel("Input Device");
	desc.SetDescription("Represents a device that can be used for input");
	desc.SetDefaultValue(eAID_KeyboardMouse);
	desc.AddConstant(eAID_KeyboardMouse, "KBM", "Keyboard and Mouse");
	desc.AddConstant(eAID_PS4Pad, "PS4", "PS4 Controller");
	desc.AddConstant(eAID_XboxPad, "Xbox", "Xbox Controller");
}

static void ReflectType(Schematyc::CTypeDesc<EActionActivationMode>& desc)
{
	desc.SetGUID("{C16D3CD2-F2D0-4BB8-BE16-FBDF09A308E7}"_cry_guid);
	desc.SetLabel("Input Activation Mode");
	desc.SetDescription("Represents the type of input event");
	desc.SetDefaultValue(eAAM_OnPress);
	desc.AddConstant(eAAM_OnPress, "Press", "Pressed");
	desc.AddConstant(eAAM_OnRelease, "Release", "Released");
	desc.AddConstant(eAAM_OnHold, "Hold", "Held");
}

SERIALIZATION_ENUM_BEGIN(EKeyId, "KeyId")
SERIALIZATION_ENUM(EKeyId::eKI_Escape, "escape", "Keyboard_Escape")
SERIALIZATION_ENUM(EKeyId::eKI_1, "1", "Keyboard_1")
SERIALIZATION_ENUM(EKeyId::eKI_2, "2", "Keyboard_2")
SERIALIZATION_ENUM(EKeyId::eKI_3, "3", "Keyboard_3")
SERIALIZATION_ENUM(EKeyId::eKI_4, "4", "Keyboard_4")
SERIALIZATION_ENUM(EKeyId::eKI_5, "5", "Keyboard_5")
SERIALIZATION_ENUM(EKeyId::eKI_6, "6", "Keyboard_6")
SERIALIZATION_ENUM(EKeyId::eKI_7, "7", "Keyboard_7")
SERIALIZATION_ENUM(EKeyId::eKI_8, "8", "Keyboard_8")
SERIALIZATION_ENUM(EKeyId::eKI_9, "9", "Keyboard_9")
SERIALIZATION_ENUM(EKeyId::eKI_0, "0", "Keyboard_0")
SERIALIZATION_ENUM(EKeyId::eKI_Minus, "minus", "Keyboard_Minus")
SERIALIZATION_ENUM(EKeyId::eKI_Equals, "equals", "Keyboard_Equals")
SERIALIZATION_ENUM(EKeyId::eKI_Backspace, "backspace", "Keyboard_Backspace")
SERIALIZATION_ENUM(EKeyId::eKI_Tab, "tab", "Keyboard_Tab")
SERIALIZATION_ENUM(EKeyId::eKI_Q, "q", "Keyboard_Q")
SERIALIZATION_ENUM(EKeyId::eKI_W, "w", "Keyboard_W")
SERIALIZATION_ENUM(EKeyId::eKI_E, "e", "Keyboard_E")
SERIALIZATION_ENUM(EKeyId::eKI_R, "r", "Keyboard_R")
SERIALIZATION_ENUM(EKeyId::eKI_T, "t", "Keyboard_T")
SERIALIZATION_ENUM(EKeyId::eKI_Y, "y", "Keyboard_Y")
SERIALIZATION_ENUM(EKeyId::eKI_U, "u", "Keyboard_U")
SERIALIZATION_ENUM(EKeyId::eKI_I, "i", "Keyboard_I")
SERIALIZATION_ENUM(EKeyId::eKI_O, "o", "Keyboard_O")
SERIALIZATION_ENUM(EKeyId::eKI_P, "p", "Keyboard_P")
SERIALIZATION_ENUM(EKeyId::eKI_LBracket, "lbracket", "Keyboard_LBracket")
SERIALIZATION_ENUM(EKeyId::eKI_RBracket, "rbracket", "Keyboard_RBracket")
SERIALIZATION_ENUM(EKeyId::eKI_Enter, "enter", "Keyboard_Enter")
SERIALIZATION_ENUM(EKeyId::eKI_LCtrl, "lctrl", "Keyboard_LCtrl")
SERIALIZATION_ENUM(EKeyId::eKI_A, "a", "Keyboard_A")
SERIALIZATION_ENUM(EKeyId::eKI_S, "s", "Keyboard_S")
SERIALIZATION_ENUM(EKeyId::eKI_D, "d", "Keyboard_D")
SERIALIZATION_ENUM(EKeyId::eKI_F, "f", "Keyboard_F")
SERIALIZATION_ENUM(EKeyId::eKI_G, "g", "Keyboard_G")
SERIALIZATION_ENUM(EKeyId::eKI_H, "h", "Keyboard_H")
SERIALIZATION_ENUM(EKeyId::eKI_J, "j", "Keyboard_J")
SERIALIZATION_ENUM(EKeyId::eKI_K, "k", "Keyboard_K")
SERIALIZATION_ENUM(EKeyId::eKI_L, "l", "Keyboard_L")
SERIALIZATION_ENUM(EKeyId::eKI_Semicolon, "semicolon", "Keyboard_Semicolon")
SERIALIZATION_ENUM(EKeyId::eKI_Apostrophe, "apostrophe", "Keyboard_Apostrophe")
SERIALIZATION_ENUM(EKeyId::eKI_Tilde, "tilde", "Keyboard_Tilde")
SERIALIZATION_ENUM(EKeyId::eKI_LShift, "lshift", "Keyboard_LShift")
SERIALIZATION_ENUM(EKeyId::eKI_Backslash, "backslash", "Keyboard_Backslash")
SERIALIZATION_ENUM(EKeyId::eKI_Z, "z", "Keyboard_Z")
SERIALIZATION_ENUM(EKeyId::eKI_X, "x", "Keyboard_X")
SERIALIZATION_ENUM(EKeyId::eKI_C, "c", "Keyboard_C")
SERIALIZATION_ENUM(EKeyId::eKI_V, "v", "Keyboard_V")
SERIALIZATION_ENUM(EKeyId::eKI_B, "b", "Keyboard_B")
SERIALIZATION_ENUM(EKeyId::eKI_N, "n", "Keyboard_N")
SERIALIZATION_ENUM(EKeyId::eKI_M, "m", "Keyboard_M")
SERIALIZATION_ENUM(EKeyId::eKI_Comma, "comma", "Keyboard_Comma")
SERIALIZATION_ENUM(EKeyId::eKI_Period, "period", "Keyboard_Period")
SERIALIZATION_ENUM(EKeyId::eKI_Slash, "slash", "Keyboard_Slash")
SERIALIZATION_ENUM(EKeyId::eKI_RShift, "rshift", "Keyboard_RShift")
SERIALIZATION_ENUM(EKeyId::eKI_NP_Multiply, "np_multiply", "Keyboard_NP_Multiply")
SERIALIZATION_ENUM(EKeyId::eKI_LAlt, "lalt", "Keyboard_LAlt")
SERIALIZATION_ENUM(EKeyId::eKI_Space, "space", "Keyboard_Space")
SERIALIZATION_ENUM(EKeyId::eKI_CapsLock, "capslock", "Keyboard_CapsLock")
SERIALIZATION_ENUM(EKeyId::eKI_F1, "f1", "Keyboard_F1")
SERIALIZATION_ENUM(EKeyId::eKI_F2, "f2", "Keyboard_F2")
SERIALIZATION_ENUM(EKeyId::eKI_F3, "f3", "Keyboard_F3")
SERIALIZATION_ENUM(EKeyId::eKI_F4, "f4", "Keyboard_F4")
SERIALIZATION_ENUM(EKeyId::eKI_F5, "f5", "Keyboard_F5")
SERIALIZATION_ENUM(EKeyId::eKI_F6, "f6", "Keyboard_F6")
SERIALIZATION_ENUM(EKeyId::eKI_F7, "f7", "Keyboard_F7")
SERIALIZATION_ENUM(EKeyId::eKI_F8, "f8", "Keyboard_F8")
SERIALIZATION_ENUM(EKeyId::eKI_F9, "f9", "Keyboard_F9")
SERIALIZATION_ENUM(EKeyId::eKI_F10, "f10", "Keyboard_F10")
SERIALIZATION_ENUM(EKeyId::eKI_NumLock, "numlock", "Keyboard_NumLock")
SERIALIZATION_ENUM(EKeyId::eKI_ScrollLock, "scrollock", "Keyboard_ScrollLock")
SERIALIZATION_ENUM(EKeyId::eKI_NP_7, "np_7", "Keyboard_NP_7")
SERIALIZATION_ENUM(EKeyId::eKI_NP_8, "np_8", "Keyboard_NP_8")
SERIALIZATION_ENUM(EKeyId::eKI_NP_9, "np_9", "Keyboard_NP_9")
SERIALIZATION_ENUM(EKeyId::eKI_NP_Substract, "np_subtract", "Keyboard_NP_Substract")
SERIALIZATION_ENUM(EKeyId::eKI_NP_4, "np_4", "Keyboard_NP_4")
SERIALIZATION_ENUM(EKeyId::eKI_NP_5, "np_5", "Keyboard_NP_5")
SERIALIZATION_ENUM(EKeyId::eKI_NP_6, "np_6", "Keyboard_NP_6")
SERIALIZATION_ENUM(EKeyId::eKI_NP_Add, "np_add", "Keyboard_NP_Add")
SERIALIZATION_ENUM(EKeyId::eKI_NP_1, "np_1", "Keyboard_NP_1")
SERIALIZATION_ENUM(EKeyId::eKI_NP_2, "np_2", "Keyboard_NP_2")
SERIALIZATION_ENUM(EKeyId::eKI_NP_3, "np_3", "Keyboard_NP_3")
SERIALIZATION_ENUM(EKeyId::eKI_NP_0, "np_0", "Keyboard_NP_0")
SERIALIZATION_ENUM(EKeyId::eKI_F11, "f11", "Keyboard_F11")
SERIALIZATION_ENUM(EKeyId::eKI_F12, "f12", "Keyboard_F12")
SERIALIZATION_ENUM(EKeyId::eKI_F13, "f13", "Keyboard_F13")
SERIALIZATION_ENUM(EKeyId::eKI_F14, "f14", "Keyboard_F14")
SERIALIZATION_ENUM(EKeyId::eKI_F15, "f15", "Keyboard_F15")
SERIALIZATION_ENUM(EKeyId::eKI_Colon, "colon", "Keyboard_Colon")
SERIALIZATION_ENUM(EKeyId::eKI_Underline, "underline", "Keyboard_Underline")
SERIALIZATION_ENUM(EKeyId::eKI_NP_Enter, "np_enter", "Keyboard_NP_Enter")
SERIALIZATION_ENUM(EKeyId::eKI_RCtrl, "rctrl", "Keyboard_RCtrl")
SERIALIZATION_ENUM(EKeyId::eKI_NP_Period, "np_period", "Keyboard_NP_Period")
SERIALIZATION_ENUM(EKeyId::eKI_NP_Divide, "np_divide", "Keyboard_NP_Divide")
SERIALIZATION_ENUM(EKeyId::eKI_Print, "print", "Keyboard_Print")
SERIALIZATION_ENUM(EKeyId::eKI_RAlt, "ralt", "Keyboard_RAlt")
SERIALIZATION_ENUM(EKeyId::eKI_Pause, "pause", "Keyboard_Pause")
SERIALIZATION_ENUM(EKeyId::eKI_Home, "home", "Keyboard_Home")
SERIALIZATION_ENUM(EKeyId::eKI_Up, "up", "Keyboard_Up")
SERIALIZATION_ENUM(EKeyId::eKI_PgUp, "pgup", "Keyboard_PgUp")
SERIALIZATION_ENUM(EKeyId::eKI_Left, "left", "Keyboard_Left")
SERIALIZATION_ENUM(EKeyId::eKI_Right, "right", "Keyboard_Right")
SERIALIZATION_ENUM(EKeyId::eKI_End, "end", "Keyboard_End")
SERIALIZATION_ENUM(EKeyId::eKI_Down, "down", "Keyboard_Down")
SERIALIZATION_ENUM(EKeyId::eKI_PgDn, "pgdn", "Keyboard_PgDn")
SERIALIZATION_ENUM(EKeyId::eKI_Insert, "insert", "Keyboard_Insert")
SERIALIZATION_ENUM(EKeyId::eKI_Delete, "delete", "Keyboard_Delete")
SERIALIZATION_ENUM(EKeyId::eKI_LWin, "lwin", "Keyboard_LWin")
SERIALIZATION_ENUM(EKeyId::eKI_RWin, "rwin", "Keyboard_RWin")
SERIALIZATION_ENUM(EKeyId::eKI_Apps, "apps", "Keyboard_Apps")

SERIALIZATION_ENUM(EKeyId::eKI_Mouse1, "mouse1", "Mouse_Button_1")
SERIALIZATION_ENUM(EKeyId::eKI_Mouse2, "mouse2", "Mouse_Button_2")
SERIALIZATION_ENUM(EKeyId::eKI_Mouse3, "mouse3", "Mouse_Button_3")
SERIALIZATION_ENUM(EKeyId::eKI_Mouse4, "mouse4", "Mouse_Button_4")
SERIALIZATION_ENUM(EKeyId::eKI_Mouse5, "mouse5", "Mouse_Button_5")
SERIALIZATION_ENUM(EKeyId::eKI_Mouse6, "mouse6", "Mouse_Button_6")
SERIALIZATION_ENUM(EKeyId::eKI_Mouse7, "mouse7", "Mouse_Button_7")
SERIALIZATION_ENUM(EKeyId::eKI_Mouse8, "mouse8", "Mouse_Button_8")
SERIALIZATION_ENUM(EKeyId::eKI_MouseWheelUp, "mwheel_up", "Mouse_WheelUp")
SERIALIZATION_ENUM(EKeyId::eKI_MouseWheelDown, "mwheel_down", "Mouse_WheelDown")
SERIALIZATION_ENUM(EKeyId::eKI_MouseX, "maxis_x", "Mouse_X")
SERIALIZATION_ENUM(EKeyId::eKI_MouseY, "maxis_y", "Mouse_Y")
SERIALIZATION_ENUM(EKeyId::eKI_MouseZ, "maxis_z", "Mouse_Z")

SERIALIZATION_ENUM(EKeyId::eKI_XI_DPadUp, "xi_dpad_up", "Pad_DPad_Up")
SERIALIZATION_ENUM(EKeyId::eKI_XI_DPadDown, "xi_dpad_down", "Pad_DPad_Down")
SERIALIZATION_ENUM(EKeyId::eKI_XI_DPadLeft, "xi_dpad_left", "Pad_DPad_Left")
SERIALIZATION_ENUM(EKeyId::eKI_XI_DPadRight, "xi_dpad_right", "Pad_DPad_Right")
SERIALIZATION_ENUM(EKeyId::eKI_XI_Start, "xi_start", "Pad_Start")
SERIALIZATION_ENUM(EKeyId::eKI_XI_Back, "xi_back", "Pad_Back")
SERIALIZATION_ENUM(EKeyId::eKI_XI_ThumbL, "xi_thumbl", "Pad_LeftThumbPress")
SERIALIZATION_ENUM(EKeyId::eKI_XI_ThumbR, "xi_thumbr", "Pad_RightThumbPress")
SERIALIZATION_ENUM(EKeyId::eKI_XI_ShoulderL, "xi_shoulderl", "Pad_LeftShoulder")
SERIALIZATION_ENUM(EKeyId::eKI_XI_ShoulderR, "xi_shoulder", "Pad_RightShoulder")
SERIALIZATION_ENUM(EKeyId::eKI_XI_A, "xi_a", "Pad_Button_A")
SERIALIZATION_ENUM(EKeyId::eKI_XI_B, "xi_b", "Pad_Button_B")
SERIALIZATION_ENUM(EKeyId::eKI_XI_X, "xi_x", "Pad_Button_X")
SERIALIZATION_ENUM(EKeyId::eKI_XI_Y, "xi_y", "Pad_Button_Y")
SERIALIZATION_ENUM(EKeyId::eKI_XI_TriggerL, "xi_triggerl", "Pad_LeftTrigger")
SERIALIZATION_ENUM(EKeyId::eKI_XI_TriggerR, "xi_triggerr", "Pad_RightTrigger")
SERIALIZATION_ENUM(EKeyId::eKI_XI_ThumbLX, "xi_thumblx", "Pad_LeftThumb_X-Axis")
SERIALIZATION_ENUM(EKeyId::eKI_XI_ThumbLY, "xi_thumbly", "Pad_LeftThumb_Y-Axis")
//SERIALIZATION_ENUM(EKeyId::eKI_XI_ThumbLUp, "Pad_ThumbLUp", "Pad_ThumbLUp")
//SERIALIZATION_ENUM(EKeyId::eKI_XI_ThumbLDown, "Pad_ThumbLDown", "Pad_ThumbLDown")
//SERIALIZATION_ENUM(EKeyId::eKI_XI_ThumbLLeft, "Pad_ThumbLLeft", "Pad_ThumbLLeft")
//SERIALIZATION_ENUM(EKeyId::eKI_XI_ThumbLRight, "Pad_ThumbLRight", "Pad_ThumbLRight")
SERIALIZATION_ENUM(EKeyId::eKI_XI_ThumbRX, "xi_thumbrx", "Pad_RightThumb_X-Axis")
SERIALIZATION_ENUM(EKeyId::eKI_XI_ThumbRY, "xi_thumbry", "Pad_RightThumb_Y-Axis")
//SERIALIZATION_ENUM(EKeyId::eKI_XI_ThumbRUp, "Pad_ThumbRUp", "Pad_ThumbRUp")
//SERIALIZATION_ENUM(EKeyId::eKI_XI_ThumbRDown, "Pad_ThumbRDown", "Pad_ThumbRDown")
//SERIALIZATION_ENUM(EKeyId::eKI_XI_ThumbRLeft, "Pad_ThumbRLeft", "Pad_ThumbRLeft")
//SERIALIZATION_ENUM(EKeyId::eKI_XI_ThumbRRight, "Pad_ThumbRRight", "Pad_ThumbRRight")

SERIALIZATION_ENUM(EKeyId::eKI_Orbis_Options, "pad_start", "Orbis_Options")
SERIALIZATION_ENUM(EKeyId::eKI_Orbis_L3, "pad_l3", "Orbis_L3")
SERIALIZATION_ENUM(EKeyId::eKI_Orbis_R3, "pad_r3", "Orbis_R3")
SERIALIZATION_ENUM(EKeyId::eKI_Orbis_Up, "pad_up", "Orbis_Up")
SERIALIZATION_ENUM(EKeyId::eKI_Orbis_Right, "pad_right", "Orbis_Right")
SERIALIZATION_ENUM(EKeyId::eKI_Orbis_Down, "pad_down", "Orbis_Down")
SERIALIZATION_ENUM(EKeyId::eKI_Orbis_Left, "pad_left", "Orbis_Left")
SERIALIZATION_ENUM(EKeyId::eKI_Orbis_LeftTrigger, "pad_ltrigger", "Orbis_LeftTrigger")
SERIALIZATION_ENUM(EKeyId::eKI_Orbis_RightTrigger, "pad_rtrigger", "Orbis_RightTrigger")
SERIALIZATION_ENUM(EKeyId::eKI_Orbis_L1, "pad_l1", "Orbis_LeftShoulderButton")
SERIALIZATION_ENUM(EKeyId::eKI_Orbis_R1, "pad_r1", "Orbis_RightShoulderButton")
SERIALIZATION_ENUM(EKeyId::eKI_Orbis_Triangle, "pad_triangle", "Orbis_Triangle")
SERIALIZATION_ENUM(EKeyId::eKI_Orbis_Circle, "pad_circle", "Orbis_Circle")
SERIALIZATION_ENUM(EKeyId::eKI_Orbis_Cross, "pad_cross", "Orbis_Cross")
SERIALIZATION_ENUM(EKeyId::eKI_Orbis_Square, "pad_square", "Orbis_Square")
SERIALIZATION_ENUM(EKeyId::eKI_Orbis_StickLX, "pad_sticklx", "Orbis_StickLX")
SERIALIZATION_ENUM(EKeyId::eKI_Orbis_StickLY, "pad_stickly", "Orbis_StickLY")
SERIALIZATION_ENUM(EKeyId::eKI_Orbis_StickRX, "pad_stickrx", "Orbis_StickRX")
SERIALIZATION_ENUM(EKeyId::eKI_Orbis_StickRY, "pad_stickry", "Orbis_StickRY")
//SERIALIZATION_ENUM(EKeyId::eKI_Orbis_RotX, "Orbis_RotX", "Orbis_RotX")
//SERIALIZATION_ENUM(EKeyId::eKI_Orbis_RotY, "Orbis_RotY", "Orbis_RotY")
//SERIALIZATION_ENUM(EKeyId::eKI_Orbis_RotZ, "Orbis_RotZ", "Orbis_RotZ")
//SERIALIZATION_ENUM(EKeyId::eKI_Orbis_RotX_KeyL, "Orbis_RotX_KeyL", "Orbis_RotX_KeyL")
//SERIALIZATION_ENUM(EKeyId::eKI_Orbis_RotX_KeyR, "Orbis_RotX_KeyR", "Orbis_RotX_KeyR")
//SERIALIZATION_ENUM(EKeyId::eKI_Orbis_RotZ_KeyD, "Orbis_RotZ_KeyD", "Orbis_RotZ_KeyD")
//SERIALIZATION_ENUM(EKeyId::eKI_Orbis_RotZ_KeyU, "Orbis_RotZ_KeyU", "Orbis_RotZ_KeyU")
SERIALIZATION_ENUM_END()

namespace Cry
{
namespace DefaultComponents
{
void ReflectType(Schematyc::CTypeDesc<CInputComponent::EKeyboardInputId>& desc)
{
	desc.SetGUID("{2FAB7843-AAF8-4573-ABC0-6190C5C5900A}"_cry_guid);
	desc.SetLabel("Key Id");
	desc.SetDescription("Input Key Identifier");
	desc.SetFlags(Schematyc::ETypeFlags::Switchable);
	desc.SetDefaultValue((CInputComponent::EKeyboardInputId)EKeyId::eKI_Enter);
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_Escape, "Escape", "Escape");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_1, "1", "1");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_2, "2", "2");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_3, "3", "3");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_4, "4", "4");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_5, "5", "5");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_6, "6", "6");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_7, "7", "7");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_8, "8", "8");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_9, "9", "9");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_0, "0", "0");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_Minus, "Minus", "Minus");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_Equals, "Equals", "Equals");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_Backspace, "Backspace", "Backspace");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_Tab, "Tab", "Tab");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_Q, "Q", "Q");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_W, "W", "W");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_E, "E", "E");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_R, "R", "R");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_T, "T", "T");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_Y, "Y", "Y");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_U, "U", "U");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_I, "I", "I");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_O, "O", "O");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_P, "P", "P");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_LBracket, "LBracket", "Left Bracket");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_RBracket, "RBracket", "Right Bracket");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_Enter, "Enter", "Enter");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_LCtrl, "LCtrl", "Left Ctrl");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_A, "A", "A");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_S, "S", "S");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_D, "D", "D");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_F, "F", "F");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_G, "G", "G");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_H, "H", "H");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_J, "J", "J");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_K, "K", "K");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_L, "L", "L");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_Semicolon, "Semicolon", "Semicolon");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_Apostrophe, "Apostrophe", "Apostrophe");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_Tilde, "Tilde", "Tilde");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_LShift, "LShift", "Left Shift");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_Backslash, "Backslash", "Backslash");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_Z, "Z", "Z");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_X, "X", "X");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_C, "C", "C");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_V, "V", "V");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_B, "B", "B");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_N, "N", "N");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_M, "M", "M");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_Comma, "Comma", "Comma");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_Period, "Period", "Period");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_Slash, "Slash", "Slash");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_RShift, "RShift", "RShift");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_LAlt, "LAlt", "LAlt");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_Space, "Space", "Space");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_CapsLock, "CapsLock", "CapsLock");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_F1, "F1", "F1");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_F2, "F2", "F2");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_F3, "F3", "F3");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_F4, "F4", "F4");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_F5, "F5", "F5");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_F6, "F6", "F6");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_F7, "F7", "F7");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_F8, "F8", "F8");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_F9, "F9", "F9");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_F10, "F10", "F10");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_NumLock, "NumLock", "NumLock");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_ScrollLock, "ScrollLock", "ScrollLock");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_NP_Multiply, "NP_Multiply", "Numpad Multiply");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_NP_7, "NP_7", "Numpad 7");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_NP_8, "NP_8", "Numpad 8");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_NP_9, "NP_9", "Numpad 9");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_NP_Substract, "NP_Substract", "Numpad Substract");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_NP_4, "NP_4", "Numpad 4");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_NP_5, "NP_5", "Numpad 5");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_NP_6, "NP_6", "Numpad 6");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_NP_Add, "NP_Add", "Numpad Add");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_NP_1, "NP_1", "Numpad 1");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_NP_2, "NP_2", "Numpad 2");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_NP_3, "NP_3", "Numpad 3");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_NP_0, "NP_0", "Numpad 0");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_NP_Enter, "NP_Enter", "Numpad Enter");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_NP_Period, "NP_Period", "Numpad Period");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_NP_Divide, "NP_Divide", "Numpad Divide");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_F11, "F11", "F11");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_F12, "F12", "F12");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_F13, "F13", "F13");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_F14, "F14", "F14");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_F15, "F15", "F15");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_Colon, "Colon", "Colon");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_Underline, "Underline", "Underline");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_RCtrl, "RCtrl", "Right Ctrl");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_Print, "Print", "Print");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_RAlt, "RAlt", "Right Alt");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_Pause, "Pause", "Pause");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_Home, "Home", "Home");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_Up, "Up", "Up Arrow");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_PgUp, "PgUp", "Page Up");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_Left, "Left", "Left Arrow");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_Right, "Right", "Right Arrow");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_End, "End", "End");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_Down, "Down", "Down Arrow");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_PgDn, "PgDn", "Page Down");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_Insert, "Insert", "Insert");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_Delete, "Delete", "Delete");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_LWin, "LWin", "Left Windows Key");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_RWin, "RWin", "Right Windows Key");
	desc.AddConstant((CInputComponent::EKeyboardInputId)EKeyId::eKI_Apps, "Apps", "Apps");
}

void ReflectType(Schematyc::CTypeDesc<CInputComponent::EMouseInputId>& desc)
{
	desc.SetGUID("{B09F6A7D-6AFD-48F6-B589-A25AD326CD0E}"_cry_guid);
	desc.SetLabel("Key Id");
	desc.SetDescription("Input Key Identifier");
	desc.SetFlags(Schematyc::ETypeFlags::Switchable);
	desc.SetDefaultValue((CInputComponent::EMouseInputId)EKeyId::eKI_Mouse1);
	desc.AddConstant((CInputComponent::EMouseInputId)EKeyId::eKI_Mouse1, "mouse1", "Left Button");
	desc.AddConstant((CInputComponent::EMouseInputId)EKeyId::eKI_Mouse2, "mouse2", "Right Button");
	desc.AddConstant((CInputComponent::EMouseInputId)EKeyId::eKI_Mouse3, "mouse3", "Middle Button");
	desc.AddConstant((CInputComponent::EMouseInputId)EKeyId::eKI_Mouse4, "mouse4", "Button_4");
	desc.AddConstant((CInputComponent::EMouseInputId)EKeyId::eKI_Mouse5, "mouse5", "Button_5");
	desc.AddConstant((CInputComponent::EMouseInputId)EKeyId::eKI_Mouse6, "mouse6", "Button_6");
	desc.AddConstant((CInputComponent::EMouseInputId)EKeyId::eKI_Mouse7, "mouse7", "Button_7");
	desc.AddConstant((CInputComponent::EMouseInputId)EKeyId::eKI_Mouse8, "mouse8", "Button_8");
	desc.AddConstant((CInputComponent::EMouseInputId)EKeyId::eKI_MouseWheelUp, "mwheel_up", "WheelUp");
	desc.AddConstant((CInputComponent::EMouseInputId)EKeyId::eKI_MouseWheelDown, "mwheel_down", "WheelDown");
	desc.AddConstant((CInputComponent::EMouseInputId)EKeyId::eKI_MouseX, "maxis_x", "X");
	desc.AddConstant((CInputComponent::EMouseInputId)EKeyId::eKI_MouseY, "maxis_y", "Y");
	desc.AddConstant((CInputComponent::EMouseInputId)EKeyId::eKI_MouseZ, "maxis_z", "Z");
}

void ReflectType(Schematyc::CTypeDesc<CInputComponent::EXboxInputId>& desc)
{
	desc.SetGUID("{8C26C065-8A61-4B89-93FA-9C102C225BBA}"_cry_guid);
	desc.SetLabel("Key Id");
	desc.SetDescription("Input Key Identifier");
	desc.SetFlags(Schematyc::ETypeFlags::Switchable);
	desc.SetDefaultValue((CInputComponent::EXboxInputId)EKeyId::eKI_XI_X);
	desc.AddConstant((CInputComponent::EXboxInputId)EKeyId::eKI_XI_DPadUp, "xi_dpad_up", "D-Pad Up");
	desc.AddConstant((CInputComponent::EXboxInputId)EKeyId::eKI_XI_DPadDown, "xi_dpad_down", "D-Pad Down");
	desc.AddConstant((CInputComponent::EXboxInputId)EKeyId::eKI_XI_DPadLeft, "xi_dpad_left", "D-Pad Left");
	desc.AddConstant((CInputComponent::EXboxInputId)EKeyId::eKI_XI_DPadRight, "xi_dpad_right", "D-Pad Right");
	desc.AddConstant((CInputComponent::EXboxInputId)EKeyId::eKI_XI_Start, "xi_start", "Start");
	desc.AddConstant((CInputComponent::EXboxInputId)EKeyId::eKI_XI_Back, "xi_back", "Back");
	desc.AddConstant((CInputComponent::EXboxInputId)EKeyId::eKI_XI_ThumbL, "xi_thumbl", "Left Thumb Press");
	desc.AddConstant((CInputComponent::EXboxInputId)EKeyId::eKI_XI_ThumbR, "xi_thumbr", "Right Thumb Press");
	desc.AddConstant((CInputComponent::EXboxInputId)EKeyId::eKI_XI_ShoulderL, "xi_shoulderl", "Left Shoulder");
	desc.AddConstant((CInputComponent::EXboxInputId)EKeyId::eKI_XI_ShoulderR, "xi_shoulderr", "Right Shoulder");
	desc.AddConstant((CInputComponent::EXboxInputId)EKeyId::eKI_XI_A, "xi_a", "A");
	desc.AddConstant((CInputComponent::EXboxInputId)EKeyId::eKI_XI_B, "xi_b", "B");
	desc.AddConstant((CInputComponent::EXboxInputId)EKeyId::eKI_XI_X, "xi_x", "X");
	desc.AddConstant((CInputComponent::EXboxInputId)EKeyId::eKI_XI_Y, "xi_y", "Y");
	desc.AddConstant((CInputComponent::EXboxInputId)EKeyId::eKI_XI_TriggerL, "xi_triggerl", "Left Trigger");
	desc.AddConstant((CInputComponent::EXboxInputId)EKeyId::eKI_XI_TriggerR, "xi_triggerr", "Right Trigger");
	desc.AddConstant((CInputComponent::EXboxInputId)EKeyId::eKI_XI_ThumbLX, "xi_thumblx", "Left Thumb X Axis");
	desc.AddConstant((CInputComponent::EXboxInputId)EKeyId::eKI_XI_ThumbLY, "xi_thumbly", "Left Thumb Y Axis");
	//desc.AddConstant((CInputComponent::EXboxInputId)EKeyId::eKI_XI_ThumbLUp, "ThumbLUp", "ThumbLUp");
	//desc.AddConstant((CInputComponent::EXboxInputId)EKeyId::eKI_XI_ThumbLDown, "ThumbLDown", "ThumbLDown");
	//desc.AddConstant((CInputComponent::EXboxInputId)EKeyId::eKI_XI_ThumbLLeft, "ThumbLLeft", "ThumbLLeft");
	//desc.AddConstant((CInputComponent::EXboxInputId)EKeyId::eKI_XI_ThumbLRight, "ThumbLRight", "ThumbLRight");
	desc.AddConstant((CInputComponent::EXboxInputId)EKeyId::eKI_XI_ThumbRX, "xi_thumbrx", "Right Thumb X Axis");
	desc.AddConstant((CInputComponent::EXboxInputId)EKeyId::eKI_XI_ThumbRY, "xi_thumbry", "Right Thumb Y Axis");
	//desc.AddConstant((CInputComponent::EXboxInputId)EKeyId::eKI_XI_ThumbRUp, "ThumbRUp", "ThumbRUp");
	//desc.AddConstant((CInputComponent::EXboxInputId)EKeyId::eKI_XI_ThumbRDown, "ThumbRDown", "ThumbRDown");
	//desc.AddConstant((CInputComponent::EXboxInputId)EKeyId::eKI_XI_ThumbRLeft, "ThumbRLeft", "ThumbRLeft");
	//desc.AddConstant((CInputComponent::EXboxInputId)EKeyId::eKI_XI_ThumbRRight, "ThumbRRight", "ThumbRRight");
}

void ReflectType(Schematyc::CTypeDesc<CInputComponent::EPS4InputId>& desc)
{
	desc.SetGUID("{9AC5093B-737C-493E-8866-D116996EA6E4}"_cry_guid);
	desc.SetLabel("Key Id");
	desc.SetDescription("Input Key Identifier");
	desc.SetFlags(Schematyc::ETypeFlags::Switchable);
	desc.SetDefaultValue((CInputComponent::EPS4InputId)EKeyId::eKI_XI_X);
	desc.AddConstant((CInputComponent::EPS4InputId)EKeyId::eKI_Orbis_Options, "pad_start", "Options");
	desc.AddConstant((CInputComponent::EPS4InputId)EKeyId::eKI_Orbis_L3, "pad_l3", "Left Thumb Press");
	desc.AddConstant((CInputComponent::EPS4InputId)EKeyId::eKI_Orbis_R3, "pad_r3", "Right Thumb Press");
	desc.AddConstant((CInputComponent::EPS4InputId)EKeyId::eKI_Orbis_Up, "pad_up", "D-Pad Up");
	desc.AddConstant((CInputComponent::EPS4InputId)EKeyId::eKI_Orbis_Right, "pad_right", "D-Pad Right");
	desc.AddConstant((CInputComponent::EPS4InputId)EKeyId::eKI_Orbis_Down, "pad_down", "D-Pad Down");
	desc.AddConstant((CInputComponent::EPS4InputId)EKeyId::eKI_Orbis_Left, "pad_left", "D-Pad Left");
	desc.AddConstant((CInputComponent::EPS4InputId)EKeyId::eKI_Orbis_LeftTrigger, "pad_ltrigger", "Left Trigger");
	desc.AddConstant((CInputComponent::EPS4InputId)EKeyId::eKI_Orbis_RightTrigger, "pad_rtrigger", "Right Trigger");
	desc.AddConstant((CInputComponent::EPS4InputId)EKeyId::eKI_Orbis_L1, "pad_l1", "Left Shoulder");
	desc.AddConstant((CInputComponent::EPS4InputId)EKeyId::eKI_Orbis_R1, "pad_r1", "Right Shoulder");
	desc.AddConstant((CInputComponent::EPS4InputId)EKeyId::eKI_Orbis_Triangle, "pad_triangle", "Triangle");
	desc.AddConstant((CInputComponent::EPS4InputId)EKeyId::eKI_Orbis_Circle, "pad_circle", "Circle");
	desc.AddConstant((CInputComponent::EPS4InputId)EKeyId::eKI_Orbis_Cross, "pad_cross", "Cross");
	desc.AddConstant((CInputComponent::EPS4InputId)EKeyId::eKI_Orbis_Square, "pad_square", "Square");
	desc.AddConstant((CInputComponent::EPS4InputId)EKeyId::eKI_Orbis_StickLX, "pad_sticklx", "Left Stick X");
	desc.AddConstant((CInputComponent::EPS4InputId)EKeyId::eKI_Orbis_StickLY, "pad_stickly", "Left Stick Y");
	desc.AddConstant((CInputComponent::EPS4InputId)EKeyId::eKI_Orbis_StickRX, "pad_stickrx", "Right Stick X");
	desc.AddConstant((CInputComponent::EPS4InputId)EKeyId::eKI_Orbis_StickRY, "pad_stickry", "Right Stick Y");
	//desc.AddConstant((CInputComponent::EPS4InputId)EKeyId::eKI_Orbis_RotX, "RotX", "RotX");
	//desc.AddConstant((CInputComponent::EPS4InputId)EKeyId::eKI_Orbis_RotY, "RotY", "RotY");
	//desc.AddConstant((CInputComponent::EPS4InputId)EKeyId::eKI_Orbis_RotZ, "RotZ", "RotZ");
	//desc.AddConstant((CInputComponent::EPS4InputId)EKeyId::eKI_Orbis_RotX_KeyL, "RotX_KeyL", "RotX_KeyL");
	//desc.AddConstant((CInputComponent::EPS4InputId)EKeyId::eKI_Orbis_RotX_KeyR, "RotX_KeyR", "RotX_KeyR");
	//desc.AddConstant((CInputComponent::EPS4InputId)EKeyId::eKI_Orbis_RotZ_KeyD, "RotZ_KeyD", "RotZ_KeyD");
	//desc.AddConstant((CInputComponent::EPS4InputId)EKeyId::eKI_Orbis_RotZ_KeyU, "RotZ_KeyU", "RotZ_KeyU");
}

struct SActionPressedSignal
{
	SActionPressedSignal() {}
	SActionPressedSignal(Schematyc::CSharedString actionName, float value)
		: m_actionName(actionName)
		, m_value(value) {}

	Schematyc::CSharedString m_actionName;
	float                    m_value;
};

struct SActionReleasedSignal
{
	SActionReleasedSignal() {}
	SActionReleasedSignal(Schematyc::CSharedString actionName, float value)
		: m_actionName(actionName)
		, m_value(value) {}

	Schematyc::CSharedString m_actionName;
	float                    m_value;
};

struct SActionChangedSignal
{
	SActionChangedSignal() {}
	SActionChangedSignal(Schematyc::CSharedString actionName, float value)
		: m_actionName(actionName)
		, m_value(value) {}

	Schematyc::CSharedString m_actionName;
	float                    m_value;
};

void CInputComponent::Register(Schematyc::CEnvRegistrationScope& componentScope)
{
	// Functions
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CInputComponent::BindKeyboardAction, "{0F22767B-6AA6-4EDF-92F0-7BCD10CAB00E}"_cry_guid, "RegisterKeyboardAction");
		pFunction->SetDescription("Registers the action and binds the keyboard input to it");
		pFunction->SetFlags({ Schematyc::EEnvFunctionFlags::Member, Schematyc::EEnvFunctionFlags::Construction });
		pFunction->BindInput(1, 'grou', "Group Name");
		pFunction->BindInput(2, 'name', "Name");
		pFunction->BindInput(3, 'keyn', "Input Id");
		pFunction->BindInput(4, 'pres', "Receive Press Events");
		pFunction->BindInput(5, 'rele', "Receive Release Events");
		pFunction->BindInput(6, 'hold', "Receive Hold Events");
		componentScope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CInputComponent::BindMouseAction, "{64199BB3-88A5-4404-BAEF-212BC0D0ACCC}"_cry_guid, "RegisterMouseAction");
		pFunction->SetDescription("Registers the action and binds the mouse input to it");
		pFunction->SetFlags({ Schematyc::EEnvFunctionFlags::Member, Schematyc::EEnvFunctionFlags::Construction });
		pFunction->BindInput(1, 'grou', "Group Name");
		pFunction->BindInput(2, 'name', "Name");
		pFunction->BindInput(3, 'keyn', "Input Id");
		pFunction->BindInput(4, 'pres', "Receive Press Events");
		pFunction->BindInput(5, 'rele', "Receive Release Events");
		pFunction->BindInput(6, 'hold', "Receive Hold Events");
		componentScope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CInputComponent::BindXboxAction, "{5D8DDAD6-AEFD-4A1A-8BA0-F27F7836AC05}"_cry_guid, "RegisterXboxAction");
		pFunction->SetDescription("Registers the action and binds the Xbox input to it");
		pFunction->SetFlags({ Schematyc::EEnvFunctionFlags::Member, Schematyc::EEnvFunctionFlags::Construction });
		pFunction->BindInput(1, 'grou', "Group Name");
		pFunction->BindInput(2, 'name', "Name");
		pFunction->BindInput(3, 'keyn', "Input Id");
		pFunction->BindInput(4, 'pres', "Receive Press Events");
		pFunction->BindInput(5, 'rele', "Receive Release Events");
		pFunction->BindInput(6, 'hold', "Receive Hold Events");
		componentScope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CInputComponent::BindPS4Action, "{D7CE1BF1-F192-4E0D-B3D6-305B31E8C693}"_cry_guid, "RegisterPS4Action");
		pFunction->SetDescription("Registers the action and binds the PS4 input to it");
		pFunction->SetFlags({ Schematyc::EEnvFunctionFlags::Member, Schematyc::EEnvFunctionFlags::Construction });
		pFunction->BindInput(1, 'grou', "Group Name");
		pFunction->BindInput(2, 'name', "Name");
		pFunction->BindInput(3, 'keyn', "Input Id");
		pFunction->BindInput(4, 'pres', "Receive Press Events");
		pFunction->BindInput(5, 'rele', "Receive Release Events");
		pFunction->BindInput(6, 'hold', "Receive Hold Events");
		componentScope.Register(pFunction);
	}

	// Signals
	{
		componentScope.Register(SCHEMATYC_MAKE_ENV_SIGNAL(SActionPressedSignal));
	}
	{
		componentScope.Register(SCHEMATYC_MAKE_ENV_SIGNAL(SActionReleasedSignal));
	}
	{
		componentScope.Register(SCHEMATYC_MAKE_ENV_SIGNAL(SActionChangedSignal));
	}
}

static void ReflectType(Schematyc::CTypeDesc<SActionPressedSignal>& desc)
{
	desc.SetGUID("{603771B5-6321-4113-82FB-B839F1BE7F64}"_cry_guid);
	desc.SetLabel("On Action Pressed");
	desc.AddMember(&SActionPressedSignal::m_actionName, 'actn', "Action", "Action Name", "Name of the triggered action", "");
	desc.AddMember(&SActionPressedSignal::m_value, 'valu', "value", "Value", "Value, if any, for example the joystick analog value", 0.0f);
}

static void ReflectType(Schematyc::CTypeDesc<SActionReleasedSignal>& desc)
{
	desc.SetGUID("{CB22A659-40AD-4CF0-AD07-42DCCEA24F44}"_cry_guid);
	desc.SetLabel("On Action Released");
	desc.AddMember(&SActionReleasedSignal::m_actionName, 'actn', "Action", "Action Name", "Name of the triggered action", "");
	desc.AddMember(&SActionReleasedSignal::m_value, 'valu', "value", "Value", "Value, if any, for example the joystick analog value", 0.0f);
}

static void ReflectType(Schematyc::CTypeDesc<SActionChangedSignal>& desc)
{
	desc.SetGUID("{ECDF0D8F-7E4B-4FA1-AEFA-51DAFD382CE9}"_cry_guid);
	desc.SetLabel("On Action Changed");
	desc.AddMember(&SActionChangedSignal::m_actionName, 'actn', "Action", "Action Name", "Name of the triggered action", "");
	desc.AddMember(&SActionChangedSignal::m_value, 'valu', "value", "Value", "Value, if any, for example the joystick analog value", 0.0f);
}

void CInputComponent::Initialize()
{
	IActionMapManager *pActionMapManager = gEnv->pGameFramework->GetIActionMapManager();

	pActionMapManager->ClearInputDevicesMappings();

	pActionMapManager->AddInputDeviceMapping(eAID_KeyboardMouse, "keyboard");
	pActionMapManager->AddInputDeviceMapping(eAID_XboxPad, "xboxpad");
	pActionMapManager->AddInputDeviceMapping(eAID_PS4Pad, "ps4pad");
	pActionMapManager->AddInputDeviceMapping(eAID_OculusTouch, "oculustouch");

	pActionMapManager->SetVersion(0);
	pActionMapManager->Enable(true);
}

void CInputComponent::RegisterSchematycAction(Schematyc::CSharedString groupName, Schematyc::CSharedString name)
{
	CRY_ASSERT(m_pEntity->GetSchematycObject() != nullptr);

	RegisterAction(groupName.c_str(), name.c_str(), [this, name](int activationMode, float value)
			{
				if ((activationMode & eAAM_OnPress) != 0)
				{
				  m_pEntity->GetSchematycObject()->ProcessSignal(SActionPressedSignal(name, value), GetGUID());
				}
				if ((activationMode & eAAM_OnRelease) != 0)
				{
				  m_pEntity->GetSchematycObject()->ProcessSignal(SActionReleasedSignal(name, value), GetGUID());
				}
				if ((activationMode & eAAM_OnHold) != 0 || (activationMode & eAAM_Always) != 0)
				{
				  m_pEntity->GetSchematycObject()->ProcessSignal(SActionChangedSignal(name, value), GetGUID());
				}
	});
}

template <typename TEnum = EKeyId>
void InternalBindAction(EntityId id, Schematyc::CSharedString groupName, Schematyc::CSharedString name, EActionInputDevice inputDevice, TEnum keyId, bool bOnPress = true, bool bOnRelease = true, bool bOnHold = true)
{
	IActionMapManager *pActionMapManager = gEnv->pGameFramework->GetIActionMapManager();
	IActionMap* pActionMap = pActionMapManager->GetActionMap(groupName.c_str());
	if (pActionMap == nullptr)
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "Tried to bind input component action %s in group %s without registering it first!", name.c_str(), groupName.c_str());
		return;
	}

	IActionMapAction* pAction = pActionMap->GetAction(ActionId(name.c_str()));
	if (pAction == nullptr)
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "Tried to bind input component action %s in group %s without registering it first!", name.c_str(), groupName.c_str());
		return;
	}

	SActionInput input;
	input.input = input.defaultInput = Serialization::getEnumName<TEnum>(keyId);
	input.inputDevice = inputDevice;

	if (bOnPress)
	{
		input.activationMode |= eAAM_OnPress;
	}
	if (bOnRelease)
	{
		input.activationMode |= eAAM_OnRelease;
	}
	if (bOnHold)
	{
		input.activationMode |= eAAM_OnHold;
	}

	pActionMap->AddAndBindActionInput(ActionId(name.c_str()), input);

	pActionMapManager->EnableActionMap(groupName.c_str(), true);
}

void CInputComponent::BindKeyboardAction(Schematyc::CSharedString groupName, Schematyc::CSharedString name, EKeyboardInputId keyId, bool bOnPress, bool bOnRelease, bool bOnHold)
{
	RegisterSchematycAction(groupName, name);

	InternalBindAction(GetEntityId(), groupName, name, EActionInputDevice::eAID_KeyboardMouse, keyId, bOnPress, bOnRelease, bOnHold);
}

void CInputComponent::BindMouseAction(Schematyc::CSharedString groupName, Schematyc::CSharedString name, EMouseInputId keyId, bool bOnPress, bool bOnRelease, bool bOnHold)
{
	RegisterSchematycAction(groupName, name);

	InternalBindAction(GetEntityId(), groupName, name, EActionInputDevice::eAID_KeyboardMouse, keyId, bOnPress, bOnRelease, bOnHold);
}

void CInputComponent::BindXboxAction(Schematyc::CSharedString groupName, Schematyc::CSharedString name, EXboxInputId keyId, bool bOnPress, bool bOnRelease, bool bOnHold)
{
	RegisterSchematycAction(groupName, name);

	InternalBindAction(GetEntityId(), groupName, name, EActionInputDevice::eAID_XboxPad, keyId, bOnPress, bOnRelease, bOnHold);
}

void CInputComponent::BindPS4Action(Schematyc::CSharedString groupName, Schematyc::CSharedString name, EPS4InputId keyId, bool bOnPress, bool bOnRelease, bool bOnHold)
{
	RegisterSchematycAction(groupName, name);

	InternalBindAction(GetEntityId(), groupName, name, EActionInputDevice::eAID_PS4Pad, keyId, bOnPress, bOnRelease, bOnHold);
}

void CInputComponent::BindAction(Schematyc::CSharedString groupName, Schematyc::CSharedString name, EActionInputDevice device, EKeyId keyId, bool bOnPress, bool bOnRelease, bool bOnHold)
{
	InternalBindAction(GetEntityId(), groupName, name, device, keyId, bOnPress, bOnRelease, bOnHold);
}
}
}