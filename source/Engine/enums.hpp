#pragma once
#include "core.hpp"
#include <glfw/glfw3.h>

namespace GR
{
	namespace Enums
	{
		enum class EMouse
		{
			Left = GLFW_MOUSE_BUTTON_LEFT,
			Right = GLFW_MOUSE_BUTTON_RIGHT,
			Middle = GLFW_MOUSE_BUTTON_MIDDLE,
			Button_4 = GLFW_MOUSE_BUTTON_4,
			Button_5 = GLFW_MOUSE_BUTTON_5,
			Button_6 = GLFW_MOUSE_BUTTON_6,
			Button_7 = GLFW_MOUSE_BUTTON_7,
			Button_8 = GLFW_MOUSE_BUTTON_8
		};

		enum class EKey
		{
			Key_0 = GLFW_KEY_0,
			Key_1 = GLFW_KEY_1,
			Key_2 = GLFW_KEY_2,
			Key_3 = GLFW_KEY_3,
			Key_4 = GLFW_KEY_4,
			Key_5 = GLFW_KEY_5,
			Key_6 = GLFW_KEY_6,
			Key_7 = GLFW_KEY_7,
			Key_8 = GLFW_KEY_8,
			Key_9 = GLFW_KEY_9,
			Semicolon = GLFW_KEY_SEMICOLON,
			Equal = GLFW_KEY_EQUAL,
			A = GLFW_KEY_A,
			B = GLFW_KEY_B,
			C = GLFW_KEY_C,
			D = GLFW_KEY_D,
			E = GLFW_KEY_E,
			F = GLFW_KEY_F,
			G = GLFW_KEY_G,
			H = GLFW_KEY_H,
			I = GLFW_KEY_I,
			J = GLFW_KEY_J,
			K = GLFW_KEY_K,
			L = GLFW_KEY_L,
			M = GLFW_KEY_M,
			N = GLFW_KEY_N,
			O = GLFW_KEY_O,
			P = GLFW_KEY_P,
			Q = GLFW_KEY_Q,
			R = GLFW_KEY_R,
			S = GLFW_KEY_S,
			T = GLFW_KEY_T,
			U = GLFW_KEY_U,
			V = GLFW_KEY_V,
			W = GLFW_KEY_W,
			X = GLFW_KEY_X,
			Y = GLFW_KEY_Y,
			Z = GLFW_KEY_Z,
			LeftBracket = GLFW_KEY_LEFT_BRACKET,
			BackSlash = GLFW_KEY_BACKSLASH,
			RightBracket = GLFW_KEY_RIGHT_BRACKET,
			GraveAccent = GLFW_KEY_GRAVE_ACCENT,
			Escape = GLFW_KEY_ESCAPE,
			Enter = GLFW_KEY_ENTER,
			Tab = GLFW_KEY_TAB,
			Backspace = GLFW_KEY_BACKSPACE,
			Insert = GLFW_KEY_INSERT,
			Delete = GLFW_KEY_DELETE,
			ArrowRight = GLFW_KEY_RIGHT,
			ArrowLeft = GLFW_KEY_LEFT,
			ArrowDown = GLFW_KEY_DOWN,
			ArrowUp = GLFW_KEY_UP,
			PageUp = GLFW_KEY_PAGE_UP,
			PageDown = GLFW_KEY_PAGE_DOWN,
			Home = GLFW_KEY_HOME,
			End = GLFW_KEY_END,
			CapsLock = GLFW_KEY_CAPS_LOCK,
			ScrollLock = GLFW_KEY_SCROLL_LOCK,
			NumLock = GLFW_KEY_NUM_LOCK,
			PrintScreen = GLFW_KEY_PRINT_SCREEN,
			Pause = GLFW_KEY_PAUSE,
			F1 = GLFW_KEY_F1,
			F2 = GLFW_KEY_F2,
			F3 = GLFW_KEY_F3,
			F4 = GLFW_KEY_F4,
			F5 = GLFW_KEY_F5,
			F6 = GLFW_KEY_F6,
			F7 = GLFW_KEY_F7,
			F8 = GLFW_KEY_F8,
			F9 = GLFW_KEY_F9,
			F10 = GLFW_KEY_F10,
			F11 = GLFW_KEY_F11,
			F12 = GLFW_KEY_F12,
			LeftShift = GLFW_KEY_LEFT_SHIFT,
			LeftControl = GLFW_KEY_LEFT_CONTROL,
			LeftAlt = GLFW_KEY_LEFT_ALT,
			RightShift = GLFW_KEY_RIGHT_SHIFT,
			RightControl = GLFW_KEY_RIGHT_CONTROL,
			RightAlt = GLFW_KEY_RIGHT_ALT
		};

		enum class EAction
		{
			Release = GLFW_RELEASE,
			Press = GLFW_PRESS,
			Repeat = GLFW_REPEAT
		};

		enum class EImageType
		{
			RGBA_SRGB,
			RGBA_UNORM,
			RGBA_FLOAT
		};
	};
};