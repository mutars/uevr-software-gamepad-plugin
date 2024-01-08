// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"

#include <memory>

#include "uevr/Plugin.hpp"
#include <ViGEm/Client.h>
#include <iostream>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "ViGEmClient.lib")

using namespace uevr;

#define PLUGIN_LOG_ONCE(...) {\
    static bool _logged_ = false; \
    if (!_logged_) { \
        _logged_ = true; \
        API::get()->log_info(__VA_ARGS__); \
    }}

class ExamplePlugin : public uevr::Plugin {
public:
    ExamplePlugin() : Plugin(), connected(false), client(vigem_alloc()), pad(nullptr), virtual_touch(0){
        if (client == nullptr)
        {
            API::get()->log_error("Uh, not enough memory to do that?!");
            return;
        }
        connected = true;

    }

    ~ExamplePlugin() override {
        if (connected) {
			vigem_target_remove(client, pad);
			vigem_target_free(pad);
		}
	}

    void on_dllmain() override {

    }

    void on_initialize() override {
        if (!connected) {
            return;
        }
        const auto retval = vigem_connect(client);

        if (!VIGEM_SUCCESS(retval))
        {
            API::get()->log_error("ViGEm Bus connection failed with error code: 0x %d", retval);
            return;
        }
        //
        // Add client to the bus, this equals a plug-in event
        //
        pad = vigem_target_x360_alloc();

        const auto pir = vigem_target_add(client, pad);

        if (!VIGEM_SUCCESS(pir))
        {
            API::get()->log_error("Target plugin failed with error code: 0x %d", pir);
        }
        API::get() -> log_info("ViGEm Bus connection successful!");
        connected = true;
    }

    void on_pre_engine_tick(UEVR_UGameEngineHandle engine, float delta) override {
        PLUGIN_LOG_ONCE("Pre Engine Tick: %f", delta);
    }

    void on_post_engine_tick(UEVR_UGameEngineHandle engine, float delta) override {
        PLUGIN_LOG_ONCE("Post Engine Tick: %f", delta);
    }

    void on_pre_slate_draw_window(UEVR_FSlateRHIRendererHandle renderer, UEVR_FViewportInfoHandle viewport_info) override {
        PLUGIN_LOG_ONCE("Pre Slate Draw Window");
    }

    void on_post_slate_draw_window(UEVR_FSlateRHIRendererHandle renderer, UEVR_FViewportInfoHandle viewport_info) override {
        PLUGIN_LOG_ONCE("Post Slate Draw Window");
    }

    void on_xinput_get_state(uint32_t* retval, uint32_t user_index, XINPUT_STATE* state) override {
        if (!connected) {
			return;
		}
        if (user_index == virtual_touch && *retval == ERROR_SUCCESS) {
            vigem_target_x360_update(client, pad, *reinterpret_cast<XUSB_REPORT*>(&state->Gamepad));
            XINPUT_STATE new_state{};
            state = &new_state;
		}
    }
private:
    bool connected;
    uint32_t virtual_touch;
    PVIGEM_CLIENT client;
    PVIGEM_TARGET pad;

};

// Actually creates the plugin. Very important that this global is created.
// The fact that it's using std::unique_ptr is not important, as long as the constructor is called in some way.
std::unique_ptr<ExamplePlugin> g_plugin{ new ExamplePlugin() };