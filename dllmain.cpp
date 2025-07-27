#include "pch.h"
#include "SDL.h"
#include <vector>
#include <array>
#include <stdexcept>

typedef double GMReal;
typedef const char* GMString;
typedef unsigned int uint;

#define expReal extern "C" __declspec(dllexport) GMReal _cdecl
#define expString extern "C" __declspec(dllexport) GMString _cdecl

struct GMGamepad
{
    SDL_Gamepad* gamepad;
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

expReal gamepad_init()
{
    SDL_Init(SDL_INIT_GAMEPAD | SDL_INIT_EVENTS);
    SDL_SetGamepadEventsEnabled(true);
    return 1;
}

expReal gamepad_get_device_count() { return sticks.size(); }

expString gamepad_get_description(GMReal id)
{
    try
    {
        return SDL_GetGamepadName(sticks.at((int)id).gamepad);
    }
    catch (...)
    {
        return "No Gamepad";
    }
}

expReal gamepad_get_type(GMReal id)
{
    try
    {
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
    
    SDL_Joystick* joy = SDL_GetGamepadJoystick(sticks[(int)id].gamepad);
    SDL_GUID guid = SDL_GetJoystickGUID(joy);
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

    return SDL_GetGamepadID(sticks[(int)id].gamepad);
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

    return SDL_RumbleGamepad(sticks[(int)id].gamepad, low_strength, high_strength, len_ms);
}

expReal gamepad_set_color(GMReal id, GMReal col)
{
    if (id < 0 || id >= sticks.size())
		return 0;
    
	int color = (int)col;
    Uint8 b = (Uint8)((color >> 16) & 0xFF);
    Uint8 g = (Uint8)((color >> 8) & 0xFF);
    Uint8 r = (Uint8)(color & 0xFF);

	return SDL_SetGamepadLED(sticks[(int)id].gamepad, r, g, b);
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

    SDL_Joystick* joy = SDL_GetGamepadJoystick(sticks[(int)id].gamepad);
    return SDL_GetNumJoystickAxes(joy);
}

expReal gamepad_button_count(GMReal id)
{
    if (id < 0 || id >= sticks.size())
        return 0;

    SDL_Joystick* joy = SDL_GetGamepadJoystick(sticks[(int)id].gamepad);
    return SDL_GetNumJoystickButtons(joy);
}

expReal gamepad_hat_count(GMReal id)
{
    if (id < 0 || id >= sticks.size())
        return 0;

    SDL_Joystick* joy = SDL_GetGamepadJoystick(sticks[(int)id].gamepad);
    return SDL_GetNumJoystickHats(joy);
}

expString gamepad_get_mapping(GMReal id)
{
    if (id < 0 || id >= sticks.size())
        return 0;

    return SDL_GetGamepadMapping(sticks[(int)id].gamepad);
}

expReal gamepad_test_mapping(GMReal id, GMString mapping)
{
    if (id < 0 || id >= sticks.size())
        return 0;

	SDL_JoystickID joy_id = SDL_GetGamepadID(sticks[(int)id].gamepad);
	return SDL_SetGamepadMapping(joy_id, mapping);
}

expReal gamepad_remove_mapping(GMReal id)
{
    if (id < 0 || id >= sticks.size())
        return 0;

    SDL_JoystickID joy_id = SDL_GetGamepadID(sticks[(int)id].gamepad);
    return SDL_SetGamepadMapping(joy_id, nullptr);
}

expReal gamepad_update()
{
    bool change = false;
    SDL_UpdateGamepads();

    // count currently active gamepads and free any that are detached
    for (int i = sticks.size() - 1; i >= 0; --i)
    {
        if (!SDL_GamepadConnected(sticks[i].gamepad))
        {
            SDL_CloseGamepad(sticks[i].gamepad);
			sticks.erase(sticks.begin() + i);
            change = true;
        }
    }

    int count;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);
    if (ids == nullptr)
    {
		MessageBox(nullptr, L"Failed to get gamepad IDs", L"GMGamepad Error", MB_OK | MB_ICONERROR);
		return 0;
    }
    else
        SDL_free(ids);

    if ((int)sticks.size() < count)  // a new stick was attached, find it and give it a slot
    {
        for (int i = 0; i < count; i++)
        {
            bool found = false;
            SDL_Gamepad* new_joy = SDL_OpenGamepad(i);
            if (new_joy == nullptr)
                continue;

            // do we already have this one?
            SDL_JoystickID id = SDL_GetGamepadID(new_joy);
            for (uint j = 0; j < sticks.size(); j++)
            {
                if (id == SDL_GetGamepadID(sticks[j].gamepad))
                {
                    found = true;
                    break;
                }
            }

            if (found)  // decrement refcount and continue
            {
                SDL_CloseGamepad(new_joy);
                continue;
            }
            else  // we got a new stick add it to the set
            {
                sticks.push_back({ new_joy });
                change = true;
            }
        }
    }

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