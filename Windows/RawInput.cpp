// Copyright (c) 2014- PPSSPP Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0 or later versions.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official git repository and contact information can be found at
// https://github.com/hrydgard/ppsspp and http://www.ppsspp.org/.

#include <set>
#include <algorithm>
#include "base/NativeApp.h"
#include "input/input_state.h"
#include "Windows/RawInput.h"
#include "Windows/KeyboardDevice.h"
#include "Windows/WindowsHost.h"
#include "Core/Config.h"

#ifndef HID_USAGE_PAGE_GENERIC
#define HID_USAGE_PAGE_GENERIC         ((USHORT) 0x01)
#endif
#ifndef HID_USAGE_GENERIC_POINTER
#define HID_USAGE_GENERIC_POINTER      ((USHORT) 0x01)
#endif
#ifndef HID_USAGE_GENERIC_MOUSE
#define HID_USAGE_GENERIC_MOUSE        ((USHORT) 0x02)
#endif
#ifndef HID_USAGE_GENERIC_JOYSTICK
#define HID_USAGE_GENERIC_JOYSTICK     ((USHORT) 0x04)
#endif
#ifndef HID_USAGE_GENERIC_GAMEPAD
#define HID_USAGE_GENERIC_GAMEPAD      ((USHORT) 0x05)
#endif
#ifndef HID_USAGE_GENERIC_KEYBOARD
#define HID_USAGE_GENERIC_KEYBOARD     ((USHORT) 0x06)
#endif
#ifndef HID_USAGE_GENERIC_KEYPAD
#define HID_USAGE_GENERIC_KEYPAD       ((USHORT) 0x07)
#endif
#ifndef HID_USAGE_GENERIC_MULTIAXIS
#define HID_USAGE_GENERIC_MULTIAXIS    ((USHORT) 0x07)
#endif

namespace WindowsRawInput {
	static std::set<int> keyboardKeysDown;
	static void *rawInputBuffer;
	static size_t rawInputBufferSize;

	void Init() {
		RAWINPUTDEVICE dev[2];
		memset(dev, 0, sizeof(dev));

		dev[0].usUsagePage = HID_USAGE_PAGE_GENERIC;
		dev[0].usUsage = HID_USAGE_GENERIC_KEYBOARD;
		dev[0].dwFlags = g_Config.bIgnoreWindowsKey ? RIDEV_NOHOTKEYS : 0;

		dev[1].usUsagePage = HID_USAGE_PAGE_GENERIC;
		dev[1].usUsage = HID_USAGE_GENERIC_MOUSE;
		dev[1].dwFlags = 0;
		RegisterRawInputDevices(dev, 2, sizeof(RAWINPUTDEVICE));
	}

	static int GetTrueVKey(const RAWKEYBOARD &kb) {
		switch (kb.VKey) {
		case VK_SHIFT:
			return MapVirtualKey(kb.MakeCode, MAPVK_VSC_TO_VK_EX);

		case VK_CONTROL:
			if (kb.Flags & RI_KEY_E0)
				return VK_RCONTROL;
			else
				return VK_LCONTROL;

		case VK_MENU:
			if (kb.Flags & RI_KEY_E0)
				return VK_RMENU;  // Right Alt / AltGr
			else
				return VK_LMENU;  // Left Alt

		default:
			return kb.VKey;
		}
	}

	LRESULT Process(WPARAM wParam, LPARAM lParam) {
		UINT dwSize;
		GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &dwSize, sizeof(RAWINPUTHEADER));
		if (!rawInputBuffer) {
			rawInputBuffer = malloc(dwSize);
			rawInputBufferSize = dwSize;
		}
		if (dwSize > rawInputBufferSize) {
			rawInputBuffer = realloc(rawInputBuffer, dwSize);
		}
		GetRawInputData((HRAWINPUT)lParam, RID_INPUT, rawInputBuffer, &dwSize, sizeof(RAWINPUTHEADER));
		RAWINPUT* raw = (RAWINPUT*)rawInputBuffer;
		if (raw->header.dwType == RIM_TYPEKEYBOARD) {
			KeyInput key;
			key.deviceId = DEVICE_ID_KEYBOARD;
			if (raw->data.keyboard.Message == WM_KEYDOWN || raw->data.keyboard.Message == WM_SYSKEYDOWN) {
				key.flags = KEY_DOWN;
				key.keyCode = windowsTransTable[GetTrueVKey(raw->data.keyboard)];

				if (key.keyCode) {
					NativeKey(key);
					keyboardKeysDown.insert(key.keyCode);
				}
			} else if (raw->data.keyboard.Message == WM_KEYUP) {
				key.flags = KEY_UP;
				key.keyCode = windowsTransTable[GetTrueVKey(raw->data.keyboard)];

				if (key.keyCode) {
					NativeKey(key);

					auto keyDown = std::find(keyboardKeysDown.begin(), keyboardKeysDown.end(), key.keyCode);
					if (keyDown != keyboardKeysDown.end())
						keyboardKeysDown.erase(keyDown);
				}
			}
		} else if (raw->header.dwType == RIM_TYPEMOUSE) {
			mouseDeltaX += raw->data.mouse.lLastX;
			mouseDeltaY += raw->data.mouse.lLastY;

			KeyInput key;
			key.deviceId = DEVICE_ID_MOUSE;

			int mouseRightBtnPressed = raw->data.mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_DOWN;
			int mouseRightBtnReleased = raw->data.mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_UP;

			if(mouseRightBtnPressed) {
				key.flags = KEY_DOWN;
				key.keyCode = windowsTransTable[VK_RBUTTON];
				NativeKey(key);
			}
			else if(mouseRightBtnReleased) {
				key.flags = KEY_UP;
				key.keyCode = windowsTransTable[VK_RBUTTON];
				NativeKey(key);
			}

			// TODO : Smooth and translate to an axis every frame.
			// NativeAxis()
		}

		return 0;
	}

	void LoseFocus() {
		// Force-release all held keys on the keyboard to prevent annoying stray inputs.
		KeyInput key;
		key.deviceId = DEVICE_ID_KEYBOARD;
		key.flags = KEY_UP;
		for (auto i = keyboardKeysDown.begin(); i != keyboardKeysDown.end(); ++i) {
			key.keyCode = *i;
			NativeKey(key);
		}
	}
};