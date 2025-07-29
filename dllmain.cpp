#include "pch.h"
#include "SDL.h"
#include <vector>
#include <array>

typedef double GMReal;
typedef const char* GMString;
typedef unsigned int uint;

#define expReal extern "C" __declspec(dllexport) GMReal _cdecl
#define expString extern "C" __declspec(dllexport) GMString _cdecl

struct GMGamepad
{
    // 当接入 SDL3 支持的手柄时，gamepad 和 joystick 都不为 nullptr；
    // 当接入 SDL3 不支持的手柄时，gamepad 为 nullptr，joystick 不为 nullptr。
    // 永远不会出现 gamepad 不为 nullptr，joystick 为 nullptr 的情况。
    SDL_Gamepad* gamepad;
    SDL_Joystick* joystick;

	double deadzone = 0.05;
	double threshold = 0.05;
    std::array<int, SDL_GAMEPAD_BUTTON_COUNT> button_events;
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

expReal gamepad_get_button_threshold(GMReal id)
{
    if (id < 0 || id >= sticks.size())
        return 0;

	return sticks[(int)id].threshold;
}

expReal gamepad_set_button_threshold(GMReal id, GMReal threshold)
{
    if (id < 0 || id >= sticks.size())
        return 0;

    sticks[(int)id].threshold = SDL_clamp(threshold, 0.0, 1.0);
    return 1;
}

expReal gamepad_get_axis_deadzone(GMReal id)
{
    if (id < 0 || id >= sticks.size())
        return 0;

    return sticks[(int)id].deadzone;
}

expReal gamepad_button_check(GMReal id, GMReal button)
{
    if (id >= 0 && id < sticks.size())
        return SDL_GetGamepadButton(sticks[(int)id].gamepad, (SDL_GamepadButton)button);
    
    return 0;
}

expReal gamepad_button_check_pressed(GMReal id, GMReal button)
{
    try
    {
        return (sticks.at((int)id).button_events.at((int)button) & 1) != 0;
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
        return (sticks.at((int)id).button_events.at((int)button) & 2) != 0;
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

expReal gamepad_set_axis_deadzone(GMReal id, GMReal deadzone)
{
    if (id < 0 || id >= sticks.size())
        return 0;

    sticks[(int)id].deadzone = SDL_clamp(deadzone, 0.0, 1.0);
	return 1;
}

expReal gamepad_axis_value(GMReal id, GMReal axis)
{
    if (id < 0 || id >= sticks.size())
        return 0;

	SDL_GamepadAxis axis_enum = (SDL_GamepadAxis)axis;

    double value = (double)SDL_GetGamepadAxis(sticks[(int)id].gamepad, axis_enum) / 32767;
	value = SDL_clamp(value, -1.0, 1.0);

    switch (axis_enum)
    {
	case SDL_GAMEPAD_AXIS_LEFTX:
	case SDL_GAMEPAD_AXIS_LEFTY:
	case SDL_GAMEPAD_AXIS_RIGHTX:
    case SDL_GAMEPAD_AXIS_RIGHTY:
    {
        if (fabs(value) < sticks[(int)id].deadzone)
            return 0;

		value = lerp(sticks[(int)id].deadzone, 1, 0, 1, fabs(value)) * sign(value);
        break;
    }
	case SDL_GAMEPAD_AXIS_LEFT_TRIGGER:
    case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER:
    {
        if (fabs(value) < sticks[(int)id].threshold)
            return 0;

        value = lerp(sticks[(int)id].threshold, 1, 0, 1, fabs(value)) * sign(value);
        break;
    }
    }

	return value;
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

expReal gamepad_add_mapping(GMString mapping)
{
    return SDL_AddGamepadMapping(mapping);
}

expReal gamepad_remove_mapping(GMReal id)
{
    if (id < 0 || id >= sticks.size())
        return 0;

    SDL_JoystickID joy_id = SDL_GetJoystickID(sticks[(int)id].joystick);
    return SDL_SetGamepadMapping(joy_id, nullptr);
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

    //gather press and release button events    
    for (uint i = 0; i < sticks.size(); i++)
    {
        for (uint j = 0; j < SDL_GAMEPAD_BUTTON_COUNT; j++)
            sticks[i].button_events[j] = 0;
    }

    while (SDL_PollEvent(&my_event))
    {
        switch (my_event.type)
        {
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        {
            SDL_Gamepad* joy = SDL_GetGamepadFromID(my_event.gbutton.which);
            int joyid = 0;
            for (uint i = 0; i < sticks.size(); i++)
            {
                if (sticks[i].gamepad == joy)
                {
                    joyid = i;
                    break;
                }
            }

            sticks[joyid].button_events[my_event.gbutton.button] |= 1;
            break;
        }
        case SDL_EVENT_GAMEPAD_BUTTON_UP:
        {
            SDL_Gamepad* joy = SDL_GetGamepadFromID(my_event.gbutton.which);
            int joyid = 0;
            for (uint i = 0; i < sticks.size(); i++)
            {
                if (sticks[i].gamepad == joy)
                {
                    joyid = i;
                    break;
                }
            }

            sticks[joyid].button_events[my_event.gbutton.button] |= 2;
            break;
        }
        }
    }

    return change;
}