#include "pch.h"
#include "SDL.h"
#include <vector>
#include <array>
#include <list>

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

	SDL_GamepadBinding** bindings = nullptr;
	int binding_count = 0;

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
		return sticks.at((uint)id).gamepad != nullptr;
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
		return SDL_GetJoystickName(sticks.at((uint)id).joystick);
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
		if (sticks.at((uint)id).gamepad == nullptr)
			return SDL_GAMEPAD_TYPE_UNKNOWN;

		return SDL_GetGamepadType(sticks.at((uint)id).gamepad);
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

	SDL_GUID guid = SDL_GetJoystickGUID(sticks[(uint)id].joystick);
	bool error = true;
	for (uint i = 0; i < 16; ++i)
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

	return SDL_GetJoystickID(sticks[(uint)id].joystick);
}

expReal gamepad_get_axis_deadzone(GMReal id)
{
	if (id < 0 || id >= sticks.size())
		return 0;

	return sticks[(uint)id].deadzone;
}

expReal gamepad_set_axis_deadzone(GMReal id, GMReal deadzone)
{
	if (id < 0 || id >= sticks.size())
		return 0;

	sticks[(uint)id].deadzone = SDL_clamp(deadzone, 0.0, 1.0);
	return 1;
}

expReal gamepad_axis_value(GMReal id, GMReal axis)
{
	if (id < 0 || id >= sticks.size() || (int)axis < JoystickAxisOffset)
		return 0;

	double value;
	if ((int)axis < DefinedAxisOffset)
	{
		value = (double)SDL_GetJoystickAxis(sticks[(uint)id].joystick, (int)axis - JoystickAxisOffset) / 32767;
		value = SDL_clamp(value, -1.0, 1.0);
	}
	else
	{
		SDL_GamepadAxis axis_enum = (SDL_GamepadAxis)((int)axis - DefinedAxisOffset);

		value = (double)SDL_GetGamepadAxis(sticks[(uint)id].gamepad, axis_enum) / 32767;
		value = SDL_clamp(value, -1.0, 1.0);
	}

	if (fabs(value) < sticks[(uint)id].deadzone)
		return 0;

	value = lerp(sticks[(uint)id].deadzone, 1, 0, 1, fabs(value)) * sign(value);
	return value;
}

expReal gamepad_button_check_direct(GMReal id, GMReal button)
{
	if (id < 0 || id >= sticks.size() || button < 0)
		return 0;

	int input = (int)button;

	if (input < DefinedButtonOffset)
	{
		if (input < JoystickAxisOffset)
			return SDL_GetJoystickButton(sticks[(uint)id].joystick, input);
		else if (input < JoystickHatOffset)
		{
			GMReal value = gamepad_axis_value(id, input);
			return fabs(sign(value));
		}
		else
		{
			int mask = SDL_GetJoystickHat(sticks[(uint)id].joystick, (input - JoystickHatOffset) / 4);
			auto list = GamepadGetHat(mask, 0, 1, 2, 3);
			for (auto inp : list)
			{
				if (inp == (input - JoystickHatOffset) % 4)
					return 1;  // 按下
			}
		}
		return SDL_GetJoystickButton(sticks[(uint)id].joystick, input);
	}
	else if (input < DefinedAxisOffset)
		return SDL_GetGamepadButton(sticks[(uint)id].gamepad, (SDL_GamepadButton)(input - DefinedButtonOffset));
	else if (input < DefinedAxisOffset + SDL_GAMEPAD_AXIS_COUNT)
	{
		GMReal value = gamepad_axis_value(id, input);
		return fabs(sign(value));
	}

	return 0;
}

expReal gamepad_button_check(GMReal id, GMReal button)
{
	try
	{
		return (sticks.at((uint)id).button_events.at((uint)button) & 0b100) != 0;
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
		return (sticks.at((uint)id).button_events.at((uint)button) & 0b001) != 0;
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
		return (sticks.at((uint)id).button_events.at((uint)button) & 0b010) != 0;
	}
	catch (...)
	{
		return 0;
	}
}

int GamepadGetOriginalIndex(uint id, int button, int* any = nullptr)
{
	if (button < DefinedButtonOffset || button >= DefinedAxisOffset + SDL_GAMEPAD_AXIS_COUNT)
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

	for (int i = 0; i < sticks.at(id).binding_count; i++)
	{
		SDL_GamepadBinding* bind = sticks[id].bindings[i];
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
			if (any != nullptr)
				*any = SDL_GAMEPAD_BUTTON_INVALID;

			return -1;
		}
		case SDL_GAMEPAD_BINDTYPE_BUTTON:
		{
			if (any != nullptr)
				*any = SDL_GAMEPAD_BUTTON_ANY;

			return bind->input.button;
		}
		case SDL_GAMEPAD_BINDTYPE_AXIS:
		{
			if (any != nullptr)
				*any = SDL_GAMEPAD_AXIS_ANY;

			return JoystickAxisOffset + bind->input.axis.axis;
		}
		case SDL_GAMEPAD_BINDTYPE_HAT:	
		{
			if (any != nullptr)
				*any = SDL_GAMEPAD_BUTTON_ANY;

			auto indexList = GamepadGetHat(bind->input.hat.hat_mask, 0, 1, 2, 3);
			return JoystickHatOffset + bind->input.hat.hat * 4 + indexList.at(0);
		}
		}
	}

	return -1;
}

expReal gamepad_button_press(GMReal id, GMReal button)
{
	if (button < 0 || (int)button >= DefinedAxisOffset + SDL_GAMEPAD_AXIS_COUNT)
		return 0;

	try
	{
		sticks.at((uint)id).button_events[(uint)button] |= 0b101;
		sticks.at((uint)id).button_events[SDL_GAMEPAD_ANY] |= 0b101;

		// 如果是受支持的手柄且传入游戏手柄按钮常量，则原始索引也会打开事件
		int anyindex;
		int index = GamepadGetOriginalIndex((uint)id, (int)button, &anyindex);
		if (index >= 0)
			sticks.at((uint)id).button_events[index] |= 0b101;

		if (anyindex != SDL_GAMEPAD_BUTTON_INVALID)
			sticks.at((uint)id).button_events[anyindex] |= 0b101;
		
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
		auto event = &sticks.at((uint)id).button_events[(uint)button];
		*event &= 0b011;  // 关闭按钮事件
		*event |= 0b010;  // 打开按钮放开事件

		event = &sticks.at((uint)id).button_events[SDL_GAMEPAD_ANY];
		*event &= 0b011;
		*event |= 0b010;

		// 如果是受支持的手柄且传入游戏手柄按钮常量，则原始索引也会打开事件
		int anyindex;
		int index = GamepadGetOriginalIndex((uint)id, (int)button, &anyindex);
		if (index >= 0)
		{
			event = &sticks.at((uint)id).button_events[index];
			*event &= 0b011;
			*event |= 0b010;
		}

		if (anyindex != SDL_GAMEPAD_BUTTON_INVALID)
		{
			event = &sticks.at((uint)id).button_events[anyindex];
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

	return SDL_RumbleJoystick(sticks[(uint)id].joystick, low_strength, high_strength, len_ms);
}

expReal gamepad_set_color(GMReal id, GMReal col)
{
	if (id < 0 || id >= sticks.size())
		return 0;

	int color = (int)col;
	Uint8 b = (Uint8)((color >> 16) & 0xFF);
	Uint8 g = (Uint8)((color >> 8) & 0xFF);
	Uint8 r = (Uint8)(color & 0xFF);

	return SDL_SetJoystickLED(sticks[(uint)id].joystick, r, g, b);
}

expReal gamepad_axis_count(GMReal id)
{
	if (id < 0 || id >= sticks.size())
		return 0;

	return SDL_GetNumJoystickAxes(sticks[(uint)id].joystick);
}

expReal gamepad_button_count(GMReal id)
{
	if (id < 0 || id >= sticks.size())
		return 0;

	return SDL_GetNumJoystickButtons(sticks[(uint)id].joystick);
}

expReal gamepad_hat_count(GMReal id)
{
	if (id < 0 || id >= sticks.size())
		return 0;

	return SDL_GetNumJoystickHats(sticks[(uint)id].joystick);
}

expReal gamepad_get_inputs_index(GMReal id, GMReal button)
{
	try
	{
		return GamepadGetOriginalIndex((uint)id, (int)button);
	}
	catch (...)
	{
		return -1;
	}
}

expString gamepad_get_mapping(GMReal id)
{
	if (id < 0 || id >= sticks.size())
		return "device index out of range";

	if (sticks[(uint)id].gamepad == nullptr)
		return "no mapping";

	GMString mapping = SDL_GetGamepadMapping(sticks[(uint)id].gamepad);
	if (mapping == nullptr)
		return "no mapping";

	return mapping;
}

expReal gamepad_test_mapping(GMReal id, GMString mapping)
{
	if (id < 0 || id >= sticks.size())
		return 0;

	SDL_JoystickID joy_id = SDL_GetJoystickID(sticks[(uint)id].joystick);
	bool result = SDL_SetGamepadMapping(joy_id, mapping);
	if (!result)
		return 0;

	if (sticks[(uint)id].gamepad == nullptr)
	{
		// 尝试打开手柄
		SDL_Gamepad* gamepad = SDL_OpenGamepad(joy_id);
		if (gamepad == nullptr)
			return 0;

		sticks[(uint)id].gamepad = gamepad;
	}

	return 1;
}

expReal gamepad_remove_mapping(GMReal id)
{
	if (id < 0 || id >= sticks.size())
		return 0;

	SDL_JoystickID joy_id = SDL_GetJoystickID(sticks[(uint)id].joystick);
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

expReal gamepad_clear(GMReal id)
{
	try
	{
		GMGamepad& stick = sticks.at((uint)id);
		for (uint i = 0; i < ButtonCount; i++)
			stick.button_events[i] = 0;

		return 1;
	}
	catch (...)
	{
		return 0;
	}
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
				if (sticks[i].bindings != nullptr)
					SDL_free(sticks[i].bindings);

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

			// 检查新手柄是否已经存在于列表中
			SDL_JoystickID id = SDL_GetJoystickID(newJoy);
			for (uint j = 0; j < sticks.size(); j++)
			{
				if (id == SDL_GetJoystickID(sticks[j].joystick))
				{
					found = true;
					break;
				}
			}

			if (found)  // 重复添加的会被删除
			{
				if (newGamepad == nullptr)
					SDL_CloseJoystick(newJoy);
				else
					SDL_CloseGamepad(newGamepad);

				continue;
			}
			else  // 新手柄会被添加至列表中
			{
				int c = 0;
				SDL_GamepadBinding** bindings = nullptr;
				if (newGamepad != nullptr)
					bindings = SDL_GetGamepadBindings(newGamepad, &c);

				sticks.push_back({ newGamepad, newJoy, bindings, c });
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

			break;
		}
		case SDL_EVENT_GAMEPAD_AXIS_MOTION:
		{
			int joyid = GetGamepadID(my_event.gbutton.which);
			if (joyid < 0)
				break;
			
			GMReal value = (GMReal)my_event.gaxis.value / 32767;
			if (fabs(value) < sticks[joyid].deadzone)
				value = 0;
			else
				value = lerp(sticks[joyid].deadzone, 1, 0, 1, fabs(value)) * sign(value);

			// 由于 SDL3 中 SDL_EVENT_JOYSTICK_AXIS_MOTION 事件的 my_event.jaxis.value 固定为 [-32768, 32767]
			// 导致摇杆和扳机键的行为不一致，所以在 SDL_EVENT_GAMEPAD_AXIS_MOTION 事件中执行 ANY 操作。
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
		// 因为在 SDL3 中，不支持的手柄会发出 SDL_EVENT_JOYSTICK_* 事件，支持的手柄会两个类型的事件都会发出，
		// 所以 SDL_GAMEPAD_BUTTON_ANY 和 SDL_GAMEPAD_ANY 事件在此设定，保证泛用性。
		case SDL_EVENT_JOYSTICK_BUTTON_DOWN:
		{
			int joyid = GetJoystickID(my_event.jbutton.which);
			if (joyid < 0)
				break;

			sticks[joyid].button_events[my_event.jbutton.button] |= 0b101;
			sticks[joyid].button_events[SDL_GAMEPAD_BUTTON_ANY] |= 0b101;
			sticks[joyid].button_events[SDL_GAMEPAD_ANY] |= 0b101;
			
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
			
			buttonEvent = &sticks[joyid].button_events[SDL_GAMEPAD_BUTTON_ANY];
			*buttonEvent &= 0b011;
			*buttonEvent |= 0b010;

			buttonEvent = &sticks[joyid].button_events[SDL_GAMEPAD_ANY];
			*buttonEvent &= 0b011;
			*buttonEvent |= 0b010;

			break;
		}
		case SDL_EVENT_JOYSTICK_AXIS_MOTION:
		{
			int joyid = GetJoystickID(my_event.jbutton.which);
			if (joyid < 0)
				break;

			GMReal value = (GMReal)my_event.jaxis.value / 32767;
			if (fabs(value) < sticks[joyid].deadzone)
				value = 0;
			else
				value = lerp(sticks[joyid].deadzone, 1, 0, 1, fabs(value)) * sign(value);

			auto buttonEvent = &sticks[joyid].button_events[JoystickAxisOffset + my_event.jaxis.axis];

			if (fabs(value) > 0 && (*buttonEvent & 0b100) == 0)  // 摇杆刚开始运动
				*buttonEvent |= 0b101;  // 打开按钮按下事件，并打开摇杆状态
			else if (value == 0 && (*buttonEvent & 0b100) != 0)  // 摇杆结束运动，回到原位
			{
				*buttonEvent &= 0b011;  // 关闭摇杆状态
				*buttonEvent |= 0b010;  // 打开按钮放开事件
			}

			break;
		}
		case SDL_EVENT_JOYSTICK_HAT_MOTION:
		{
			int joyid = GetJoystickID(my_event.jbutton.which);
			if (joyid < 0)
				break;

			auto hatEventUp = &sticks[joyid].button_events[JoystickHatOffset + my_event.jhat.hat * 4];
			auto hatEventDown = &sticks[joyid].button_events[JoystickHatOffset + my_event.jhat.hat * 4 + 1];
			auto hatEventLeft = &sticks[joyid].button_events[JoystickHatOffset + my_event.jhat.hat * 4 + 2];
			auto hatEventRight = &sticks[joyid].button_events[JoystickHatOffset + my_event.jhat.hat * 4 + 3];
			auto anyButtonEvent = &sticks[joyid].button_events[SDL_GAMEPAD_BUTTON_ANY];
			auto anyEvent = &sticks[joyid].button_events[SDL_GAMEPAD_ANY];

			std::list<char*> noPressEvents = {
				hatEventUp, hatEventDown, hatEventLeft, hatEventRight
			};

			auto hatEvents = GamepadGetHat(my_event.jhat.value, hatEventUp, hatEventDown, hatEventLeft, hatEventRight);
			for (auto event : hatEvents)
			{
				if ((*event & 0b100) == 0)
				{
					*event |= 0b101;  // 打开按钮按下事件，并打开方向键状态
					*anyButtonEvent |= 0b101;
					*anyEvent |= 0b101;
				}

				noPressEvents.remove(event);
			}

			for (auto event : noPressEvents)
			{
				if ((*event & 0b100) != 0)
				{
					*event &= 0b011;  // 关闭按钮事件
					*event |= 0b001;  // 打开按钮按下事件
				}
			}

			if (noPressEvents.size() == 4 && (*anyButtonEvent & 0b100) != 0)
			{
				*anyButtonEvent &= 0b011;
				*anyButtonEvent |= 0b010;

				*anyEvent &= 0b011;
				*anyEvent |= 0b010;
			}

			break;
		}
		}
	}

	return change;
}