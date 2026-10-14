#pragma once

#include "common/types.h"
#include <windows.h>
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#include <string>
#include <vector>

namespace Video {

class VulkanContext {
public:
    static VulkanContext& Instance();

    bool Initialize(u32 width = 1920, u32 height = 1080, bool fullscreen = false, bool headless = false);
    void Shutdown();

    void PresentFrame();
    bool ProcessWindowMessages();

    HWND GetHwnd() const { return m_hwnd; }
    u32 GetWidth() const { return m_width; }
    u32 GetHeight() const { return m_height; }
    bool IsInitialized() const { return m_initialized; }
    bool HasFocus() const { return m_has_focus; }
    void SetFocus(bool focus) { m_has_focus = focus; }
    VkSemaphore GetImageAvailableSemaphore() const { return m_image_available_semaphore; }
    VkSemaphore GetRenderFinishedSemaphore() const { return m_render_finished_semaphore; }

private:
    VulkanContext() = default;
    ~VulkanContext() = default;

    bool LoadVulkanLibrary();
    bool CreateAppWindow();
    bool CreateInstance();
    bool SelectPhysicalDevice();
    bool CreateLogicalDevice();
    bool CreateSwapchain();

    bool m_initialized{false};
    bool m_headless{false};
    u32 m_width{1920};
    u32 m_height{1080};
    bool m_fullscreen{false};

    HMODULE m_vulkan_dll{NULL};
    HWND m_hwnd{NULL};
    HINSTANCE m_hinstance{NULL};

    VkInstance m_vk_instance{VK_NULL_HANDLE};
    VkSurfaceKHR m_vk_surface{VK_NULL_HANDLE};
    VkPhysicalDevice m_vk_physical_device{VK_NULL_HANDLE};
    VkDevice m_vk_device{VK_NULL_HANDLE};
    VkQueue m_graphics_queue{VK_NULL_HANDLE};
    VkQueue m_present_queue{VK_NULL_HANDLE};
    u32 m_graphics_queue_family{0};
    u32 m_present_queue_family{0};

    VkSwapchainKHR m_vk_swapchain{VK_NULL_HANDLE};
    VkFormat m_swapchain_format{VK_FORMAT_B8G8R8A8_UNORM};
    std::vector<VkImage> m_swapchain_images;
    std::vector<VkImageView> m_swapchain_image_views;
    u32 m_current_image_index{0};

    bool m_has_focus{true};
    VkSemaphore m_image_available_semaphore{VK_NULL_HANDLE};
    VkSemaphore m_render_finished_semaphore{VK_NULL_HANDLE};

    // Dynamically loaded function pointers
    PFN_vkGetInstanceProcAddr fp_vkGetInstanceProcAddr{nullptr};
    PFN_vkCreateInstance fp_vkCreateInstance{nullptr};
    PFN_vkDestroyInstance fp_vkDestroyInstance{nullptr};
    PFN_vkEnumeratePhysicalDevices fp_vkEnumeratePhysicalDevices{nullptr};
    PFN_vkGetPhysicalDeviceProperties fp_vkGetPhysicalDeviceProperties{nullptr};
    PFN_vkGetPhysicalDeviceQueueFamilyProperties fp_vkGetPhysicalDeviceQueueFamilyProperties{nullptr};
    PFN_vkCreateDevice fp_vkCreateDevice{nullptr};
    PFN_vkDestroyDevice fp_vkDestroyDevice{nullptr};
    PFN_vkGetDeviceQueue fp_vkGetDeviceQueue{nullptr};
    PFN_vkDeviceWaitIdle fp_vkDeviceWaitIdle{nullptr};
    PFN_vkQueueSubmit fp_vkQueueSubmit{nullptr};

    // Semaphores
    PFN_vkCreateSemaphore fp_vkCreateSemaphore{nullptr};
    PFN_vkDestroySemaphore fp_vkDestroySemaphore{nullptr};

    // Surface & Win32 KHR
    PFN_vkCreateWin32SurfaceKHR fp_vkCreateWin32SurfaceKHR{nullptr};
    PFN_vkDestroySurfaceKHR fp_vkDestroySurfaceKHR{nullptr};
    PFN_vkGetPhysicalDeviceSurfaceSupportKHR fp_vkGetPhysicalDeviceSurfaceSupportKHR{nullptr};
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR fp_vkGetPhysicalDeviceSurfaceCapabilitiesKHR{nullptr};
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR fp_vkGetPhysicalDeviceSurfaceFormatsKHR{nullptr};
    PFN_vkGetPhysicalDeviceSurfacePresentModesKHR fp_vkGetPhysicalDeviceSurfacePresentModesKHR{nullptr};

    // Swapchain KHR
    PFN_vkCreateSwapchainKHR fp_vkCreateSwapchainKHR{nullptr};
    PFN_vkDestroySwapchainKHR fp_vkDestroySwapchainKHR{nullptr};
    PFN_vkGetSwapchainImagesKHR fp_vkGetSwapchainImagesKHR{nullptr};
    PFN_vkAcquireNextImageKHR fp_vkAcquireNextImageKHR{nullptr};
    PFN_vkQueuePresentKHR fp_vkQueuePresentKHR{nullptr};
    PFN_vkCreateImageView fp_vkCreateImageView{nullptr};
    PFN_vkDestroyImageView fp_vkDestroyImageView{nullptr};
};

} // namespace Video
