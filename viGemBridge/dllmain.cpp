// dllmain.cpp : Defines the entry point for the DLL application.
#include <memory>

#include "uevr/Plugin.hpp"
#include <ViGEm/Client.h>
#include <iostream>
#include <Windows.h>
#include "Math.hpp"
#include "imgui/imgui_impl_dx11.h"
#include "imgui/imgui_impl_dx12.h"
#include "imgui/imgui_impl_win32.h"
#include "rendering/d3d11.hpp"
#include "rendering/d3d12.hpp"

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


    void on_present() {
        if (!m_initialized) {
            if (!initialize_imgui()) {
                API::get()->log_info("Failed to initialize imgui");
                return;
            }
            else {
                API::get()->log_info("Initialized imgui");
            }
        }

        const auto renderer_data = API::get()->param()->renderer;

        if (renderer_data->renderer_type == UEVR_RENDERER_D3D11) {
            ImGui_ImplDX11_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();

            internal_frame();

            ImGui::EndFrame();
            ImGui::Render();

            g_d3d11.render_imgui();
        }
        else if (renderer_data->renderer_type == UEVR_RENDERER_D3D12) {
            auto command_queue = (ID3D12CommandQueue*)renderer_data->command_queue;

            if (command_queue == nullptr) {
                return;
            }

            ImGui_ImplDX12_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();

            internal_frame();

            ImGui::EndFrame();
            ImGui::Render();

            g_d3d12.render_imgui();
        }
    }

    void on_device_reset() {
        PLUGIN_LOG_ONCE("Device Reset");

        const auto renderer_data = API::get()->param()->renderer;

        if (renderer_data->renderer_type == UEVR_RENDERER_D3D11) {
            ImGui_ImplDX11_Shutdown();
            g_d3d11 = {};
        }

        if (renderer_data->renderer_type == UEVR_RENDERER_D3D12) {
            ImGui_ImplDX12_Shutdown();
            g_d3d12 = {};
        }

        m_initialized = false;
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
        if (*retval != ERROR_SUCCESS || user_index != virtual_touch) {
			return;
		}

        if (is_hand_behind_head(0)) {
            if (state->Gamepad.wButtons & XINPUT_GAMEPAD_BACK || state->Gamepad.wButtons & XINPUT_GAMEPAD_START) {
                state->Gamepad.wButtons ^= XINPUT_GAMEPAD_START;
                state->Gamepad.wButtons |= XINPUT_GAMEPAD_BACK;
            }
            if (state->Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) {
    				state->Gamepad.wButtons ^= XINPUT_GAMEPAD_LEFT_SHOULDER;
                send_vm_tab(1);
            }
            else {
                send_vm_tab(0);
            }
        }
        else {
            if (state->Gamepad.wButtons & XINPUT_GAMEPAD_BACK) {
                state->Gamepad.wButtons |= XINPUT_GAMEPAD_START;
                state->Gamepad.wButtons ^= XINPUT_GAMEPAD_BACK;
            }
            send_vm_tab(0);
        }

        auto tilt = get_tilt_direction();
        if (tilt != 0) {
            if (tilt == -1) {
				send_q(1);
				send_e(0);
			}
            else {
				send_q(0);
				send_e(1);
			}
        }
        else {
            send_q(0);
            send_e(0);
        }


        // Sync state to virtual controller
        if (connected) {
            vigem_target_x360_update(client, pad, *reinterpret_cast<XUSB_REPORT*>(&state->Gamepad));
        }
        /*XINPUT_STATE new_state{};
        state = &new_state;*/
    }
private:
    bool connected;
    uint32_t virtual_touch;
    PVIGEM_CLIENT client;
    PVIGEM_TARGET pad;

    int key_tab_down = 0;
    int key_q_down = 0;
    int key_e_down = 0;
    bool m_initialized = false;
    HWND m_wnd{};

    float distance_to_head{ 0.0f };
    float hand_dot_flat{ 0.0f };
    glm::highp_vec3 m_hmd_rotation{};

    void send_vm_tab(int down) {
        bool changed = down ^ key_tab_down;
        if (!changed) {
            return;
        }
        key_tab_down = down;
        send_key(VK_TAB, down);
	}

    void send_q(int down) {
        bool changed = down ^ key_q_down;
        if (!changed) {
            return;
        }
        key_q_down = down;
        send_key('Q', down);
    }

    void send_e(int down) {
        bool changed = down ^ key_e_down;
        if (!changed) {
            return;
        }
        key_e_down = down;
        send_key('E', down);
    }

    void send_key(WORD key, int down) {
        INPUT inputs[1] = {};
        ZeroMemory(inputs, sizeof(inputs));
        inputs[0].type = INPUT_KEYBOARD;
        inputs[0].ki.wVk = key;
        if (!down) {
            inputs[0].ki.dwFlags = KEYEVENTF_KEYUP;
        }
        UINT uSent = SendInput(ARRAYSIZE(inputs), inputs, sizeof(INPUT));
    }

    int get_tilt_direction() {
		const auto vr = API::get()->param()->vr;

        if (!vr->is_hmd_active()) {
			return 0;
		}

        auto hmd_index = vr->get_hmd_index();

        Vector3f hmd_position{};
        glm::quat hmd_rotation{};
        vr->get_grip_pose(hmd_index, (UEVR_Vector3f*)&hmd_position, (UEVR_Quaternionf*)&hmd_rotation);
        glm::quat rot_offset{};
        vr->get_rotation_offset((UEVR_Quaternionf*)&rot_offset);

        const auto current_hmd_rotation = glm::normalize(rot_offset * hmd_rotation);
        const auto angles = glm::degrees(utility::math::euler_angles_from_steamvr(current_hmd_rotation));
        m_hmd_rotation = angles;
        if (abs(angles.z) > 30.0f) {
			return 0;
		}
        if (angles.x < -15.0f) {
			return -1;
        }
        else if (angles.x > 15.0f) {
            return 1;
        }
        else {
            return 0;
        }
    }

    bool is_hand_behind_head(uint32_t hand, float sensitivity = 0.15f) {
        const auto vr = API::get()->param()->vr;

        if (!vr->is_using_controllers()) {
            return false;
        }

        auto joy_index = hand == 0 ? vr->get_left_controller_index() : vr->get_right_controller_index();

        Vector3f hand_position{};
        glm::quat hand_rotation{};
        vr->get_grip_pose(joy_index, (UEVR_Vector3f*)&hand_position, (UEVR_Quaternionf*)&hand_rotation);

        auto hmd_index = vr->get_hmd_index();

        Vector3f hmd_position{};
        glm::quat hmd_rotation{};
        vr->get_grip_pose(hmd_index, (UEVR_Vector3f*)&hmd_position, (UEVR_Quaternionf*)&hmd_rotation);
        glm::quat rot_offset{};
        vr->get_rotation_offset((UEVR_Quaternionf*)&rot_offset);

        hmd_rotation = rot_offset * hmd_rotation;
        const auto hmd_delta = Vector3f{ hand_position - hmd_position };
        const auto distance = glm::length(hmd_delta);
        distance_to_head = distance;


        const auto hmd_dir = glm::normalize(hmd_delta);
        const auto flattened_forward = glm::normalize(Vector3f{ hmd_rotation.x, 0.0f, hmd_rotation.z });

        const auto hand_dot_flat_raw = glm::dot(flattened_forward, hmd_dir);
        hand_dot_flat = hand_dot_flat_raw;

        if (distance >= 0.3f) {
            return false;
        }
        return hand_dot_flat_raw > sensitivity;
    }


    bool initialize_imgui() {
        if (m_initialized) {
            return true;
        }

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        // ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

        const auto renderer_data = API::get()->param()->renderer;

        DXGI_SWAP_CHAIN_DESC swap_desc{};
        auto swapchain = (IDXGISwapChain*)renderer_data->swapchain;
        swapchain->GetDesc(&swap_desc);

        m_wnd = swap_desc.OutputWindow;

        if (!ImGui_ImplWin32_Init(m_wnd)) {
            return false;
        }

        if (renderer_data->renderer_type == UEVR_RENDERER_D3D11) {
            if (!g_d3d11.initialize()) {
                return false;
            }
        }
        else if (renderer_data->renderer_type == UEVR_RENDERER_D3D12) {
            if (!g_d3d12.initialize()) {
                return false;
            }
        }

        m_initialized = true;
        return true;
    }

    void internal_frame() {
        if (!API::get()->param()->functions->is_drawing_ui()) {
            return;
        }

        if (ImGui::Begin("SCUM", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_AlwaysAutoResize)) {

            ImGui::Text("Left Hand to HMD distance=%f", distance_to_head);
            ImGui::Text("Hand dot flat=%f", hand_dot_flat);
            // ImGui::Text("Tilt Direction %d", hand_dot_flat.x);
            //ImGui::Text("HMD rotation Yaw=%f", m_hmd_rotation.y);
            //ImGui::Text("HMD rotation Pitch=%f", m_hmd_rotation.x); // tilt -15 to 15 , < -15 left, > 15 right
            //ImGui::Text("HMD rotation Roll=%f", m_hmd_rotation.z); // up down -30 to 30

            ImGui::End();
        }
    }

};

// Actually creates the plugin. Very important that this global is created.
// The fact that it's using std::unique_ptr is not important, as long as the constructor is called in some way.
std::unique_ptr<ExamplePlugin> g_plugin{ new ExamplePlugin() };