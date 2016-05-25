#pragma once
#define VULKAN
#include <Api/Vkapi.h>
#include <AMD_TressFX.h>

struct sample
{
    AMD::TressFX_Desc tressfx_helper;
    std::unique_ptr<device_t> dev;
    std::unique_ptr<command_queue_t> queue;
    std::unique_ptr<swap_chain_t> chain;

    std::vector<std::unique_ptr<image_t>> back_buffer;

    std::unique_ptr<image_t> depth_texture;
    std::unique_ptr<image_view_t> depth_texture_view;
    std::unique_ptr<buffer_t> constant_buffer;

    std::unique_ptr<command_list_storage_t> command_storage;
    std::unique_ptr<command_list_t> draw_command_buffer;
    std::array<std::unique_ptr<command_list_t>, 2> blit_command_buffer;

    sample(HINSTANCE hinstance, HWND hwnd);
    void draw();
};

