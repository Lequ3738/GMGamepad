#include "pch.h"
#include "SDL.h"
#include <vector>
#include <stdexcept>

std::vector<SDL_Gamepad*> sticks;
SDL_Event my_event;
int stick_capacity = 16;
int button_capacity = SDL_GAMEPAD_BUTTON_COUNT;
int stick_count = 0;
int button_events[32][SDL_GAMEPAD_BUTTON_COUNT];

typedef double GMReal;
typedef const char* GMString;

#define expReal extern "C" __declspec(dllexport) GMReal _cdecl
#define expString extern "C" __declspec(dllexport) GMString _cdecl

bool gp_updated = false;
std::vector<double> gp_deadzone;
std::vector<double> gp_threshold;

inline double lerp(double fromA, double fromB, double toA, double toB, double value)
{
    return ((value - fromA) / (fromB - fromA)) * (toB - toA) + toA;
}

#define sign(x) ((x > 0) - (x < 0))

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    {
        SDL_InitSubSystem(SDL_INIT_GAMEPAD);
        SDL_SetGamepadEventsEnabled(true);
		sticks.resize(stick_capacity);
		gp_deadzone.resize(stick_capacity, 0.05); // default deadzone of 5%
        gp_threshold.resize(stick_capacity, 0.05);

        break;
    }

    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

expReal gamepad_get_device_count() { return stick_count; }

expString gamepad_get_description(GMReal id)
{
    if (id >= 0 && id < stick_count)
        return SDL_GetGamepadName(sticks[(int)id]);
    
    return "No Gamepad";
}

expReal gamepad_get_type(GMReal id)
{
    if (id >= 0 && id < stick_count)
        return SDL_GetGamepadType(sticks[(int)id]);

    return -1;
}

expString gamepad_get_guid(GMReal id)
{
    if (id < 0 || id >= stick_count)
        return "device index out of range";
    
    SDL_Joystick* joy = SDL_GetGamepadJoystick(sticks[(int)id]);
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

expReal gamepad_get_button_threshold(GMReal id)
{
    if (id < 0 || id >= stick_count)
        return 0;

	return gp_threshold[(int)id];
}

expReal gamepad_set_button_threshold(GMReal id, GMReal threshold)
{
    if (id < 0 || id >= stick_count)
        return 0;

	gp_threshold[(int)id] = SDL_clamp(threshold, 0.0, 1.0);
    return 1;
}

expReal gamepad_get_axis_deadzone(GMReal id)
{
    if (id < 0 || id >= stick_count)
        return 0;

    return gp_deadzone[(int)id];
}

expReal gamepad_button_check(GMReal id, GMReal button)
{
    if (id >= 0 && id < stick_count)
        return SDL_GetGamepadButton(sticks[(int)id], (SDL_GamepadButton)button);
    
    return 0;
}

expReal gamepad_button_check_pressed(GMReal id, GMReal button)
{
    if (id < stick_count && button < button_capacity)
        return (button_events[(int)id][(int)button] & 1) != 0;
    
    return 0;
}

expReal gamepad_button_check_released(GMReal id, GMReal button)
{
    if (id < stick_count && button < button_capacity)
        return (button_events[(int)id][(int)button] & 2) != 0;
    
    return 0;
}

expReal gamepad_set_vibration(GMReal id, GMReal low, GMReal high, GMReal len)
{
    if (id < 0 || id >= stick_count)
        return 0;

	Uint16 low_strength = (Uint16)(SDL_clamp(low * 65535, 0, 65535));
	Uint16 high_strength = (Uint16)(SDL_clamp(high * 65535, 0, 65535));
	Uint32 len_ms = (Uint32)(max(0, len * 1000));

    return SDL_RumbleGamepad(sticks[(int)id], low_strength, high_strength, len_ms);
}

expReal gamepad_set_color(GMReal id, GMReal col)
{
    if (id < 0 || id >= stick_count)
		return 0;
    
	int color = (int)col;
    Uint8 b = (Uint8)((color >> 16) & 0xFF);
    Uint8 g = (Uint8)((color >> 8) & 0xFF);
    Uint8 r = (Uint8)(color & 0xFF);

	return SDL_SetGamepadLED(sticks[(int)id], r, g, b);
}

expReal gamepad_set_axis_deadzone(GMReal id, GMReal deadzone)
{
    if (id < 0 || id >= stick_count)
        return 0;

	gp_deadzone[(int)id] = SDL_clamp(deadzone, 0.0, 1.0);
	return 1;
}

expReal gamepad_axis_value(GMReal id, GMReal axis)
{
    if (id < 0 || id >= stick_count)
        return 0;

	SDL_GamepadAxis axis_enum = (SDL_GamepadAxis)axis;

    double value = (double)SDL_GetGamepadAxis(sticks[(int)id], axis_enum) / 32767;
	value = SDL_clamp(value, -1.0, 1.0);

    switch (axis_enum)
    {
	case SDL_GAMEPAD_AXIS_LEFTX:
	case SDL_GAMEPAD_AXIS_LEFTY:
	case SDL_GAMEPAD_AXIS_RIGHTX:
    case SDL_GAMEPAD_AXIS_RIGHTY:
    {
        if (fabs(value) < gp_deadzone[(int)id])
            return 0;

		value = lerp(gp_deadzone[(int)id], 1, 0, 1, fabs(value)) * sign(value);
        break;
    }
	case SDL_GAMEPAD_AXIS_LEFT_TRIGGER:
    case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER:
    {
        if (fabs(value) < gp_threshold[(int)id])
            return 0;

        value = lerp(gp_threshold[(int)id], 1, 0, 1, fabs(value)) * sign(value);
        break;
    }
    }

	return value;
}

expReal gamepad_axis_count(GMReal id)
{
    if (id < 0 || id >= stick_count)
        return 0;

    SDL_Joystick* joy = SDL_GetGamepadJoystick(sticks[(int)id]);
    return SDL_GetNumJoystickAxes(joy);
}

expReal gamepad_button_count(GMReal id)
{
    if (id < 0 || id >= stick_count)
        return 0;

    SDL_Joystick* joy = SDL_GetGamepadJoystick(sticks[(int)id]);
    return SDL_GetNumJoystickButtons(joy);
}

expReal gamepad_hat_count(GMReal id)
{
    if (id < 0 || id >= stick_count)
        return 0;

    SDL_Joystick* joy = SDL_GetGamepadJoystick(sticks[(int)id]);
    return SDL_GetNumJoystickHats(joy);
}

expString gamepad_get_mapping(GMReal id)
{
    if (id < 0 || id >= stick_count)
        return 0;

    return SDL_GetGamepadMapping(sticks[(int)id]);
}

expReal gamepad_test_mapping(GMReal id, GMString mapping)
{
    if (id < 0 || id >= stick_count)
        return 0;

	SDL_JoystickID joy_id = SDL_GetGamepadID(sticks[(int)id]);
	return SDL_SetGamepadMapping(joy_id, mapping);
}

expReal gamepad_remove_mapping(GMReal id)
{
    if (id < 0 || id >= stick_count)
        return 0;

    SDL_JoystickID joy_id = SDL_GetGamepadID(sticks[(int)id]);
    return SDL_SetGamepadMapping(joy_id, nullptr);
}

expReal gamepad_update()
{
    bool change = false;
    SDL_UpdateGamepads();

    // count currently active gamepads and free any that are detached
    int current_count = 0;
    int shift_amount = 0;
    for (int i = 0; i < stick_count; i++)
    {
        if (!SDL_GamepadConnected(sticks[i]))
        {
            SDL_CloseGamepad(sticks[i]);
            shift_amount++;
            change = true;
        }
        else
        {
            sticks[i - shift_amount] = sticks[i];
            current_count++;
        }
    }

    stick_count = current_count;
    int count;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);
    if (ids == nullptr)
        throw std::runtime_error("Failed to get gamepad IDs");
    else
        SDL_free(ids);

    if (current_count < count)  // a new stick was attached, find it and give it a slot
    {
        for (int i = 0; i < count; i++)
        {
            bool found = false;
            SDL_Gamepad* new_joy = SDL_OpenGamepad(i);
            if (new_joy == nullptr)
                continue;

            // do we already have this one?
            SDL_JoystickID id = SDL_GetGamepadID(new_joy);
            for (int j = 0; j < stick_count; j++)
            {
                if (id == SDL_GetGamepadID(sticks[j]))
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
                // holy shit more than we were ready for, add more capacity
                if (stick_count >= stick_capacity)
                {
                    stick_capacity = stick_count + 1;
                    sticks.resize(stick_capacity);
                    gp_deadzone.resize(stick_capacity, 0.05);
                    gp_threshold.resize(stick_capacity, 0.05);
                }

                sticks[stick_count++] = new_joy;
                change = true;
            }
        }
    }

    //gather press and release button events    
    for (int i = 0; i < stick_capacity; i++)
    {
        for (int j = 0; j < button_capacity; j++)
            button_events[i][j] = 0;
    }

    while (SDL_PollEvent(&my_event))
    {
        switch (my_event.type)
        {
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        {
            SDL_Gamepad* joy = SDL_GetGamepadFromID(my_event.gbutton.which);
            int joyid = 0;
            for (int i = 0; i < stick_count; i++)
            {
                if (sticks[i] == joy)
                {
                    joyid = i;
                    break;
                }
            }

            button_events[joyid][my_event.gbutton.button] |= 1;
            break;
        }
        case SDL_EVENT_GAMEPAD_BUTTON_UP:
        {
            SDL_Gamepad* joy = SDL_GetGamepadFromID(my_event.gbutton.which);
            int joyid = 0;
            for (int i = 0; i < stick_count; i++)
            {
                if (sticks[i] == joy)
                {
                    joyid = i;
                    break;
                }
            }

            button_events[joyid][my_event.gbutton.button] |= 2;
            break;
        }
        }
    }

    return change;
}