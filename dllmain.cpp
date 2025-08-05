#include "pch.h"
#include "SDL.h"
#include <vector>
#include <array>

typedef double GMReal;
typedef const char* GMString;
typedef unsigned int uint;

#define expReal extern "C" __declspec(dllexport) GMReal _cdecl
#define expString extern "C" __declspec(dllexport) GMString _cdecl

// 0 - 99: 手柄的原始按钮值
// 100 - 125: 已定义的手柄按钮常量
// 126 - 131: 已定义的手柄摇杆常量
constexpr int DefinedButtonOffset = 100;
constexpr int DefinedAxisOffset = DefinedButtonOffset + SDL_GAMEPAD_BUTTON_COUNT;
constexpr int ButtonCount = DefinedAxisOffset + SDL_GAMEPAD_AXIS_COUNT + 3;

// 0 - 59: 手柄的原始按钮值
// 60 - 79: 手柄的原始摇杆值
// 80 - 99: 手柄的原始方向键值
constexpr int JoystickAxisOffset = 60;
constexpr int JoystickHatOffset = 80;

enum ExtraSDL
{
	SDL_GAMEPAD_BUTTON_ANY = 132,
	SDL_GAMEPAD_AXIS_ANY,
	SDL_GAMEPAD_ANY
};

struct GMGamepad
{
	// 当接入 SDL3 支持的手柄时，gamepad 和 joystick 都不为 nullptr；
	// 当接入 SDL3 不支持的手柄时，gamepad 为 nullptr，joystick 不为 nullptr。
	// 永远不会出现 gamepad 不为 nullptr，joystick 为 nullptr 的情况。
	SDL_Gamepad* gamepad;
	SDL_Joystick* joystick;

	double deadzone = 0.05;
	std::array<char, ButtonCount> button_events;
};

std::vector<GMGamepad> sticks;
SDL_Event my_event;

bool gp_updated = false;

inline double lerp(double fromA, double fromB, double toA, double toB, double value)
{
	return ((value - fromA) / (fromB - fromA)) * (toB - toA) + toA;
}

#define sign(x) ((x > 0) - (x < 0))

expReal gamepad_init(GMString gamepadDB)
{
	if (*gamepadDB != '\0')
		SDL_AddGamepadMappingsFromFile(gamepadDB);

	bool result = SDL_Init(SDL_INIT_GAMEPAD);
	SDL_SetGamepadEventsEnabled(true);
	return result;
}

expReal gamepad_is_supported(GMReal id)
{
	try
	{
		return sticks.at((int)id).gamepad != nullptr;
	}
	catch (...)
	{
		return 0;
	}
}

expReal gamepad_get_device_count() { return sticks.size(); }

expString gamepad_get_description(GMReal id)
{
	try
	{
		return SDL_GetJoystickName(sticks.at((int)id).joystick);
	}
	catch (...)
	{
		return "no gamepad";
	}
}

expReal gamepad_get_type(GMReal id)
{
	try
	{
		if (sticks.at((int)id).gamepad == nullptr)
			return SDL_GAMEPAD_TYPE_UNKNOWN;

		return SDL_GetGamepadType(sticks.at((int)id).gamepad);
	}
	catch (...)
	{
		return -1;
	}
}

expString gamepad_get_guid(GMReal id)
{
	if (id < 0 || id >= sticks.size())
		return "device index out of range";

	SDL_GUID guid = SDL_GetJoystickGUID(sticks[(int)id].joystick);
	bool error = true;
	for (int i = 0; i < 16; ++i)
	{
		if (guid.data[i] != 0)
		{
			error = false;
			break;
		}
	}
	if (error)
		return "none";

	char* guid_str = new char[33];
	SDL_GUIDToString(guid, guid_str, 33);

	return guid_str;
}

expReal gamepad_get_id(GMReal id)
{
	if (id < 0 || id >= sticks.size())
		return -1;

	return SDL_GetJoystickID(sticks[(int)id].joystick);
}

expReal gamepad_get_axis_deadzone(GMReal id)
{
	if (id < 0 || id >= sticks.size())
		return 0;

	return sticks[(int)id].deadzone;
}

expReal gamepad_set_axis_deadzone(GMReal id, GMReal deadzone)
{
	if (id < 0 || id >= sticks.size())
		return 0;

	sticks[(int)id].deadzone = SDL_clamp(deadzone, 0.0, 1.0);
	return 1;
}

expReal gamepad_axis_value(GMReal id, GMReal axis)
{
	if (id < 0 || id >= sticks.size() || (int)axis < JoystickAxisOffset)
		return 0;

	double value;
	if ((int)axis < DefinedAxisOffset)
	{
		value = (double)SDL_GetJoystickAxis(sticks[(int)id].joystick, (int)axis - JoystickAxisOffset) / 32767;
		value = SDL_clamp(value, -1.0, 1.0);
	}
	else
	{
		SDL_GamepadAxis axis_enum = (SDL_GamepadAxis)((int)axis - DefinedAxisOffset);

		value = (double)SDL_GetGamepadAxis(sticks[(int)id].gamepad, axis_enum) / 32767;
		value = SDL_clamp(value, -1.0, 1.0);
	}

	value = lerp(sticks[(int)id].deadzone, 1, 0, 1, fabs(value)) * sign(value);
	return value;
}

expReal gamepad_button_check_direct(GMReal id, GMReal button)
{
	if (id < 0 || id >= sticks.size() || button < 0)
		return 0;

	int but = (int)button;

	if (but < DefinedButtonOffset)
		return SDL_GetJoystickButton(sticks[(int)id].joystick, but);
	else if (but < DefinedAxisOffset)
		return SDL_GetGamepadButton(sticks[(int)id].gamepad, (SDL_GamepadButton)(but - DefinedButtonOffset));
	else if (but < DefinedAxisOffset + SDL_GAMEPAD_AXIS_COUNT)
	{
		GMReal value = gamepad_axis_value(id, but);
		return fabs(sign(value));
	}

	return 0;
}

expReal gamepad_button_check(GMReal id, GMReal button)
{
	try
	{
		return (sticks.at((int)id).button_events.at((int)button) & 0b100) != 0;
	}
	catch (...)
	{
		return 0;
	}
}

expReal gamepad_button_check_pressed(GMReal id, GMReal button)
{
	try
	{
		return (sticks.at((int)id).button_events.at((int)button) & 0b001) != 0;
	}
	catch (...)
	{
		return 0;
	}
}

expReal gamepad_button_check_released(GMReal id, GMReal button)
{
	try
	{
		return (sticks.at((int)id).button_events.at((int)button) & 0b010) != 0;
	}
	catch (...)
	{
		return 0;
	}
}

template<typename T>
std::vector<T> GamepadGetHat(int hatMask, T up, T down, T left, T right)
{
	switch (hatMask)
	{
	case SDL_HAT_UP:  return { up };
	case SDL_HAT_DOWN: return { down };
	case SDL_HAT_LEFT: return { left };
	case SDL_HAT_RIGHT: return { right };
	case SDL_HAT_LEFTUP: return { up, left };
	case SDL_HAT_LEFTDOWN: return { down, left };
	case SDL_HAT_RIGHTUP: return { up, right };
	case SDL_HAT_RIGHTDOWN: return { down, right };
	default: return {};
	}
}

int GamepadGetOriginalIndex(SDL_Gamepad* device, int button, int* any)
{
	if (device == nullptr || button < DefinedButtonOffset || button >= DefinedAxisOffset + SDL_GAMEPAD_AXIS_COUNT)
		return -1;

	int type;
	if (button < DefinedAxisOffset)
	{
		type = SDL_GAMEPAD_BINDTYPE_BUTTON;
		button -= DefinedButtonOffset;
	}
	else
	{
		type = SDL_GAMEPAD_BINDTYPE_AXIS;
		button -= DefinedAxisOffset;
	}

	int count;
	SDL_GamepadBinding** bindings = SDL_GetGamepadBindings(device, &count);
	for (int i = 0; i < count; i++)
	{
		SDL_GamepadBinding* bind = bindings[i];
		if (bind->output_type != type)
			continue;

		if (type == SDL_GAMEPAD_BINDTYPE_BUTTON)
		{
			if (bind->output.button != button)
				continue;
		}
		else
		{
			if (bind->output.axis.axis != button)
				continue;
		}

		switch (bind->input_type)
		{
		case SDL_GAMEPAD_BINDTYPE_NONE:
		{
			SDL_free(bindings);

			if (any != nullptr)
				*any = SDL_GAMEPAD_BUTTON_INVALID;

			return -1;
		}
		case SDL_GAMEPAD_BINDTYPE_BUTTON:
		{
			SDL_free(bindings);

			if (any != nullptr)
				*any = SDL_GAMEPAD_BUTTON_ANY;

			return bind->input.button;
		}
		case SDL_GAMEPAD_BINDTYPE_AXIS:
		{
			SDL_free(bindings);

			if (any != nullptr)
				*any = SDL_GAMEPAD_AXIS_ANY;

			return JoystickAxisOffset + bind->input.axis.axis;
		}
		case SDL_GAMEPAD_BINDTYPE_HAT:	
		{
			SDL_free(bindings);

			if (any != nullptr)
				*any = SDL_GAMEPAD_BUTTON_ANY;

			auto indexList = GamepadGetHat(bind->input.hat.hat_mask, 0, 1, 2, 3);
			return JoystickHatOffset + bind->input.hat.hat * 4 + indexList.at(0);
		}
		}
	}

	SDL_free(bindings);
	return -1;
}

expReal gamepad_button_press(GMReal id, GMReal button)
{
	if (button < 0 || (int)button >= DefinedAxisOffset + SDL_GAMEPAD_AXIS_COUNT)
		return 0;

	try
	{
		sticks.at((int)id).button_events[(int)button] |= 0b101;
		sticks.at((int)id).button_events[SDL_GAMEPAD_ANY] |= 0b101;

		// 如果是受支持的手柄且传入游戏手柄按钮常量，则原始索引也会打开事件
		int anyindex;
		int index = GamepadGetOriginalIndex(sticks[(int)id].gamepad, (int)button, &anyindex);
		if (index >= 0)
			sticks.at((int)id).button_events[index] |= 0b101;

		if (anyindex != SDL_GAMEPAD_BUTTON_INVALID)
			sticks.at((int)id).button_events[anyindex] |= 0b101;
		
		return 1;
	}
	catch (...)
	{
		return 0;
	}
}

expReal gamepad_button_release(GMReal id, GMReal button)
{
	if (button < 0 || (int)button >= DefinedAxisOffset + SDL_GAMEPAD_AXIS_COUNT)
		return 0;

	try
	{
		auto event = &sticks.at((int)id).button_events[(int)button];
		*event &= 0b011;  // 关闭按钮事件
		*event |= 0b010;  // 打开按钮放开事件

		event = &sticks.at((int)id).button_events[SDL_GAMEPAD_ANY];
		*event &= 0b011;
		*event |= 0b010;

		// 如果是受支持的手柄且传入游戏手柄按钮常量，则原始索引也会打开事件
		int anyindex;
		int index = GamepadGetOriginalIndex(sticks[(int)id].gamepad, (int)button, &anyindex);
		if (index >= 0)
		{
			event = &sticks.at((int)id).button_events[index];
			*event &= 0b011;
			*event |= 0b010;
		}

		if (anyindex != SDL_GAMEPAD_BUTTON_INVALID)
		{
			event = &sticks.at((int)id).button_events[anyindex];
			*event &= 0b011;
			*event |= 0b010;
		}

		return 1;
	}
	catch (...)
	{
		return 0;
	}
}

expReal gamepad_set_vibration(GMReal id, GMReal low, GMReal high, GMReal len)
{
	if (id < 0 || id >= sticks.size())
		return 0;

	Uint16 low_strength = (Uint16)(SDL_clamp(low * 65535, 0, 65535));
	Uint16 high_strength = (Uint16)(SDL_clamp(high * 65535, 0, 65535));
	Uint32 len_ms = (Uint32)(max(0, len * 1000));

	return SDL_RumbleJoystick(sticks[(int)id].joystick, low_strength, high_strength, len_ms);
}

expReal gamepad_set_color(GMReal id, GMReal col)
{
	if (id < 0 || id >= sticks.size())
		return 0;

	int color = (int)col;
	Uint8 b = (Uint8)((color >> 16) & 0xFF);
	Uint8 g = (Uint8)((color >> 8) & 0xFF);
	Uint8 r = (Uint8)(color & 0xFF);

	return SDL_SetJoystickLED(sticks[(int)id].joystick, r, g, b);
}

expReal gamepad_axis_count(GMReal id)
{
	if (id < 0 || id >= sticks.size())
		return 0;

	return SDL_GetNumJoystickAxes(sticks[(int)id].joystick);
}

expReal gamepad_button_count(GMReal id)
{
	if (id < 0 || id >= sticks.size())
		return 0;

	return SDL_GetNumJoystickButtons(sticks[(int)id].joystick);
}

expReal gamepad_hat_count(GMReal id)
{
	if (id < 0 || id >= sticks.size())
		return 0;

	return SDL_GetNumJoystickHats(sticks[(int)id].joystick);
}

expString gamepad_get_mapping(GMReal id)
{
	if (id < 0 || id >= sticks.size())
		return "device index out of range";

	if (sticks[(int)id].gamepad == nullptr)
		return "no mapping";

	GMString mapping = SDL_GetGamepadMapping(sticks[(int)id].gamepad);
	if (mapping == nullptr)
		return "no mapping";

	return mapping;
}

expReal gamepad_test_mapping(GMReal id, GMString mapping)
{
	if (id < 0 || id >= sticks.size())
		return 0;

	SDL_JoystickID joy_id = SDL_GetJoystickID(sticks[(int)id].joystick);
	bool result = SDL_SetGamepadMapping(joy_id, mapping);
	if (!result)
		return 0;

	if (sticks[(int)id].gamepad == nullptr)
	{
		// 尝试打开手柄
		SDL_Gamepad* gamepad = SDL_OpenGamepad(joy_id);
		if (gamepad == nullptr)
			return 0;

		sticks[(int)id].gamepad = gamepad;
	}

	return 1;
}

expReal gamepad_remove_mapping(GMReal id)
{
	if (id < 0 || id >= sticks.size())
		return 0;

	SDL_JoystickID joy_id = SDL_GetJoystickID(sticks[(int)id].joystick);
	return SDL_SetGamepadMapping(joy_id, nullptr);
}

int GetGamepadID(SDL_JoystickID id)
{
	SDL_Gamepad* joy = SDL_GetGamepadFromID(id);
	for (uint i = 0; i < sticks.size(); i++)
	{
		if (sticks[i].gamepad == joy)
			return i;
	}

	return -1;
}

int GetJoystickID(SDL_JoystickID id)
{
	SDL_Joystick* joy = SDL_GetJoystickFromID(id);
	for (uint i = 0; i < sticks.size(); i++)
	{
		if (sticks[i].joystick == joy)
			return i;
	}

	return -1;
}

expReal gamepad_update()
{
	bool change = false;
	SDL_UpdateGamepads();

	// 检查当前已连接的游戏手柄，并释放所有已断开连接的手柄。
	for (int i = sticks.size() - 1; i >= 0; --i)
	{
		if (sticks[i].gamepad == nullptr)
		{
			// 判断不受支持的手柄的链接情况
			if (!SDL_JoystickConnected(sticks[i].joystick))
			{
				SDL_CloseJoystick(sticks[i].joystick);
				sticks.erase(sticks.begin() + i);
				change = true;
			}
		}
		else
		{
			// 判断受支持的手柄的链接情况
			if (!SDL_GamepadConnected(sticks[i].gamepad))
			{
				SDL_CloseGamepad(sticks[i].gamepad);
				sticks.erase(sticks.begin() + i);
				change = true;
			}
		}
	}

	// 获取硬件上已经连接的手柄数量
	int count;
	SDL_JoystickID* ids = SDL_GetJoysticks(&count);
	if (ids == nullptr)
	{
		MessageBox(nullptr, L"Failed to get gamepad IDs", L"GMGamepad Error", MB_OK | MB_ICONERROR);
		return 0;
	}

	if ((int)sticks.size() < count)  // a new stick was attached, find it and give it a slot
	{
		for (int i = 0; i < count; i++)
		{
			bool found = false;

			SDL_Joystick* newJoy = nullptr;
			SDL_Gamepad* newGamepad = SDL_OpenGamepad(ids[i]);
			if (newGamepad == nullptr)
			{
				newJoy = SDL_OpenJoystick(ids[i]);
				if (newJoy == nullptr)
					continue;
			}
			else
				newJoy = SDL_GetGamepadJoystick(newGamepad);

			// do we already have this one?
			SDL_JoystickID id = SDL_GetJoystickID(newJoy);
			for (uint j = 0; j < sticks.size(); j++)
			{
				if (id == SDL_GetJoystickID(sticks[j].joystick))
				{
					found = true;
					break;
				}
			}

			if (found)  // decrement refcount and continue
			{
				if (newGamepad == nullptr)
					SDL_CloseJoystick(newJoy);
				else
					SDL_CloseGamepad(newGamepad);

				continue;
			}
			else  // we got a new stick add it to the set
			{
				sticks.push_back({ newGamepad, newJoy });
				change = true;
			}
		}
	}

	SDL_free(ids);

	// 重置按钮事件
	// 位数（从右至左）代表的含义：1.按钮按下事件  2.按钮放开事件  3.按钮事件
	for (uint i = 0; i < sticks.size(); i++)
	{
		for (uint j = 0; j < ButtonCount; j++)
			sticks[i].button_events[j] &= 0b100;  // 不清除按钮事件(3)
	}

	while (SDL_PollEvent(&my_event))
	{
		switch (my_event.type)
		{
			// Gamepad
		case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
		{
			int joyid = GetGamepadID(my_event.gbutton.which);
			if (joyid < 0)
				break;

			sticks[joyid].button_events[my_event.gbutton.button + DefinedButtonOffset] |= 0b101;
			sticks[joyid].button_events[SDL_GAMEPAD_BUTTON_ANY] |= 0b101;
			sticks[joyid].button_events[SDL_GAMEPAD_ANY] |= 0b101;
			break;
		}
		case SDL_EVENT_GAMEPAD_BUTTON_UP:
		{
			int joyid = GetGamepadID(my_event.gbutton.which);
			if (joyid < 0)
				break;

			auto buttonEvent = &sticks[joyid].button_events[my_event.gbutton.button + DefinedButtonOffset];
			*buttonEvent &= 0b011;  // 关闭按钮事件
			*buttonEvent |= 0b010;  // 打开按钮放开事件

			buttonEvent = &sticks[joyid].button_events[SDL_GAMEPAD_BUTTON_ANY];
			*buttonEvent &= 0b011;
			*buttonEvent |= 0b010;

			buttonEvent = &sticks[joyid].button_events[SDL_GAMEPAD_ANY];
			*buttonEvent &= 0b011;
			*buttonEvent |= 0b010;

			break;
		}
		case SDL_EVENT_GAMEPAD_AXIS_MOTION:
		{
			int joyid = GetGamepadID(my_event.gbutton.which);
			if (joyid < 0)
				break;

			GMReal value = (GMReal)my_event.gaxis.value / 32767;
			value = lerp(sticks[joyid].deadzone, 1, 0, 1, fabs(value)) * sign(value);

			auto buttonEvent = &sticks[joyid].button_events[my_event.gaxis.axis + DefinedAxisOffset];
			auto anyAxisEvent = &sticks[joyid].button_events[SDL_GAMEPAD_AXIS_ANY];
			auto anyEvent = &sticks[joyid].button_events[SDL_GAMEPAD_ANY];

			if (fabs(value) > 0 && (*buttonEvent & 0b100) == 0)  // 摇杆刚开始运动
			{
				*buttonEvent |= 0b101;  // 打开按钮按下事件，并打开摇杆状态
				*anyAxisEvent |= 0b101;
				*anyEvent |= 0b101;
			}
			else if (value == 0 && (*buttonEvent & 0b100) != 0)  // 摇杆结束运动，回到原位
			{
				*buttonEvent &= 0b011;  // 关闭摇杆状态
				*buttonEvent |= 0b010;  // 打开按钮放开事件

				*anyAxisEvent &= 0b011;
				*anyAxisEvent |= 0b010;

				*anyEvent &= 0b011;
				*anyEvent |= 0b010;
			}

			break;
		}

		// Joystick
		case SDL_EVENT_JOYSTICK_BUTTON_DOWN:
		{
			int joyid = GetJoystickID(my_event.jbutton.which);
			if (joyid < 0)
				break;

			sticks[joyid].button_events[my_event.jbutton.button] |= 0b101;
			
			if (sticks[joyid].gamepad == nullptr)
			{
				sticks[joyid].button_events[SDL_GAMEPAD_BUTTON_ANY] |= 0b101;
				sticks[joyid].button_events[SDL_GAMEPAD_ANY] |= 0b101;
			}

			break;
		}
		case SDL_EVENT_JOYSTICK_BUTTON_UP:
		{
			int joyid = GetJoystickID(my_event.jbutton.which);
			if (joyid < 0)
				break;

			auto buttonEvent = &sticks[joyid].button_events[my_event.jbutton.button];
			*buttonEvent &= 0b011;  // 关闭按钮事件
			*buttonEvent |= 0b010;  // 打开按钮放开事件
			
			if (sticks[joyid].gamepad == nullptr)
			{
				buttonEvent = &sticks[joyid].button_events[SDL_GAMEPAD_BUTTON_ANY];
				*buttonEvent &= 0b011;
				*buttonEvent |= 0b010;

				buttonEvent = &sticks[joyid].button_events[SDL_GAMEPAD_ANY];
				*buttonEvent &= 0b011;
				*buttonEvent |= 0b010;
			}

			break;
		}
		case SDL_EVENT_JOYSTICK_AXIS_MOTION:
		{
			int joyid = GetJoystickID(my_event.jbutton.which);
			if (joyid < 0)
				break;

			if (sticks[joyid].gamepad != nullptr)
				break;

			GMReal value = (GMReal)my_event.jaxis.value / 32767;
			value = lerp(sticks[joyid].deadzone, 1, 0, 1, fabs(value)) * sign(value);

			auto buttonEvent = &sticks[joyid].button_events[JoystickAxisOffset + my_event.jaxis.axis];
			auto anyAxisEvent = &sticks[joyid].button_events[SDL_GAMEPAD_AXIS_ANY];
			auto anyEvent = &sticks[joyid].button_events[SDL_GAMEPAD_ANY];

			if (fabs(value) > 0 && (*buttonEvent & 0b100) == 0)  // 摇杆刚开始运动
			{
				*buttonEvent |= 0b101;  // 打开按钮按下事件，并打开摇杆状态
				*anyAxisEvent |= 0b101;
				*anyEvent |= 0b101;
			}
			else if (value == 0 && (*buttonEvent & 0b100) != 0)  // 摇杆结束运动，回到原位
			{
				*buttonEvent &= 0b011;  // 关闭摇杆状态
				*buttonEvent |= 0b010;  // 打开按钮放开事件

				*anyAxisEvent &= 0b011;
				*anyAxisEvent |= 0b010;

				*anyEvent &= 0b011;
				*anyEvent |= 0b010;
			}

			break;
		}
		case SDL_EVENT_JOYSTICK_HAT_MOTION:
		{
			int joyid = GetJoystickID(my_event.jbutton.which);
			if (joyid < 0)
				break;

			if (sticks[joyid].gamepad != nullptr)
				break;

			auto hatEventUp = &sticks[joyid].button_events[JoystickHatOffset + my_event.jhat.hat * 4];
			auto hatEventDown = &sticks[joyid].button_events[JoystickHatOffset + my_event.jhat.hat * 4 + 1];
			auto hatEventLeft = &sticks[joyid].button_events[JoystickHatOffset + my_event.jhat.hat * 4 + 2];
			auto hatEventRight = &sticks[joyid].button_events[JoystickHatOffset + my_event.jhat.hat * 4 + 3];
			auto anyButtonEvent = &sticks[joyid].button_events[SDL_GAMEPAD_BUTTON_ANY];
			auto anyEvent = &sticks[joyid].button_events[SDL_GAMEPAD_ANY];

			auto hatEvents = GamepadGetHat(my_event.jhat.value, hatEventUp, hatEventDown, hatEventLeft, hatEventRight);
			for (auto event : hatEvents)
			{
				if ((*event & 0b100) == 0)
				{
					*event |= 0b101;  // 打开按钮按下事件，并打开方向键状态
					*anyButtonEvent |= 0b101;
					*anyEvent |= 0b101;
				}
				else
				{
					*event &= 0b011;  // 关闭按钮事件
					*event |= 0b001;  // 打开按钮按下事件

					*anyButtonEvent &= 0b011;
					*anyButtonEvent |= 0b010;

					*anyEvent &= 0b011;
					*anyEvent |= 0b010;
				}
			}

			break;
		}
		}
	}

	return change;
}