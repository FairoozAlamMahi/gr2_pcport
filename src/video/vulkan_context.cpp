#include "video/vulkan_context.h"
#include "gpu/gnm_driver.h"
#include "common/logger.h"
#include <iostream>
#include <algorithm>

namespace Video {

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        (void)hdc;
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1; // Prevent GDI flicker, Vulkan renders entire client rect
    case WM_SETFOCUS:
        VulkanContext::Instance().SetFocus(true);
        return 0;
    case WM_KILLFOCUS:
        VulkanContext::Instance().SetFocus(false);
        return 0;
    case WM_ACTIVATE:
        VulkanContext::Instance().SetFocus(LOWORD(wParam) != WA_INACTIVE);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xFFF0) == SC_SCREENSAVE || (wParam & 0xFFF0) == SC_MONITORPOWER) {
            return 0; // Prevent monitor sleep during gameplay
        }
        break;
    case WM_CLOSE:
        PostQuitMessage(0);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            PostQuitMessage(0);
            return 0;
        }
        break;
    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

VulkanContext& VulkanContext::Instance() {
    static VulkanContext s_instance;
    return s_instance;
}

bool VulkanContext::Initialize(u32 width, u32 height, bool fullscreen, bool headless) {
    m_width = width;
    m_height = height;
    m_fullscreen = fullscreen;
    m_headless = headless;

    LOG_INFO("VULKAN", "Initializing Vulkan Graphics & Presentation Context (" + 
             std::to_string(width) + "x" + std::to_string(height) + 
             (fullscreen ? " Fullscreen" : " Windowed") + 
             (headless ? " [Headless Mode]" : "") + ")...");

    if (!LoadVulkanLibrary()) {
        LOG_WARN("VULKAN", "Failed to load vulkan-1.dll; running in headless software mode.");
        return false;
    }

    if (!m_headless) {
        if (!CreateAppWindow()) {
            LOG_ERROR("VULKAN", "Failed to create Win32 application window.");
            return false;
        }
    }

    if (!CreateInstance()) {
        LOG_ERROR("VULKAN", "Failed to create Vulkan instance.");
        return false;
    }

    if (!m_headless && m_hwnd) {
        VkWin32SurfaceCreateInfoKHR sci{};
        sci.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        sci.hinstance = m_hinstance;
        sci.hwnd = m_hwnd;

        if (fp_vkCreateWin32SurfaceKHR(m_vk_instance, &sci, nullptr, &m_vk_surface) != VK_SUCCESS) {
            LOG_ERROR("VULKAN", "Failed to create Win32 Vulkan surface.");
            return false;
        }
        LOG_INFO("VULKAN", "Vulkan Win32 presentation surface created successfully.");
    }

    if (!SelectPhysicalDevice()) {
        LOG_ERROR("VULKAN", "Failed to select suitable physical GPU.");
        return false;
    }

    if (!CreateLogicalDevice()) {
        LOG_ERROR("VULKAN", "Failed to create Vulkan logical device.");
        return false;
    }

    if (m_vk_device && fp_vkCreateSemaphore) {
        VkSemaphoreCreateInfo sci{};
        sci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        if (fp_vkCreateSemaphore(m_vk_device, &sci, nullptr, &m_image_available_semaphore) != VK_SUCCESS) {
            LOG_ERROR("VULKAN", "Failed to create image-available semaphore.");
            return false;
        }
        if (fp_vkCreateSemaphore(m_vk_device, &sci, nullptr, &m_render_finished_semaphore) != VK_SUCCESS) {
            LOG_ERROR("VULKAN", "Failed to create render-finished semaphore.");
            return false;
        }
        LOG_INFO("VULKAN", "Vulkan presentation synchronization semaphores created successfully.");
    }

    if (!m_headless && m_vk_surface) {
        if (!CreateSwapchain()) {
            LOG_ERROR("VULKAN", "Failed to create Vulkan presentation swapchain.");
            return false;
        }
    }

    m_initialized = true;
    LOG_INFO("VULKAN", "Vulkan Swapchain Presentation Pipeline fully initialized.");
    return true;
}

void VulkanContext::Shutdown() {
    if (!m_initialized) return;

    if (fp_vkDeviceWaitIdle && m_vk_device) {
        fp_vkDeviceWaitIdle(m_vk_device);
    }

    if (m_render_finished_semaphore && fp_vkDestroySemaphore) {
        fp_vkDestroySemaphore(m_vk_device, m_render_finished_semaphore, nullptr);
        m_render_finished_semaphore = VK_NULL_HANDLE;
    }

    if (m_image_available_semaphore && fp_vkDestroySemaphore) {
        fp_vkDestroySemaphore(m_vk_device, m_image_available_semaphore, nullptr);
        m_image_available_semaphore = VK_NULL_HANDLE;
    }

    for (auto iv : m_swapchain_image_views) {
        if (iv && fp_vkDestroyImageView) fp_vkDestroyImageView(m_vk_device, iv, nullptr);
    }
    m_swapchain_image_views.clear();

    if (m_vk_swapchain && fp_vkDestroySwapchainKHR) {
        fp_vkDestroySwapchainKHR(m_vk_device, m_vk_swapchain, nullptr);
        m_vk_swapchain = VK_NULL_HANDLE;
    }

    if (m_vk_surface && fp_vkDestroySurfaceKHR) {
        fp_vkDestroySurfaceKHR(m_vk_instance, m_vk_surface, nullptr);
        m_vk_surface = VK_NULL_HANDLE;
    }

    if (m_vk_device && fp_vkDestroyDevice) {
        fp_vkDestroyDevice(m_vk_device, nullptr);
        m_vk_device = VK_NULL_HANDLE;
    }

    if (m_vk_instance && fp_vkDestroyInstance) {
        fp_vkDestroyInstance(m_vk_instance, nullptr);
        m_vk_instance = VK_NULL_HANDLE;
    }

    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = NULL;
    }

    if (m_vulkan_dll) {
        FreeLibrary(m_vulkan_dll);
        m_vulkan_dll = NULL;
    }

    m_initialized = false;
    LOG_INFO("VULKAN", "Vulkan presentation context shut down.");
}

bool VulkanContext::LoadVulkanLibrary() {
    m_vulkan_dll = LoadLibraryA("vulkan-1.dll");
    if (!m_vulkan_dll) {
        return false;
    }

    fp_vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
        GetProcAddress(m_vulkan_dll, "vkGetInstanceProcAddr"));
    if (!fp_vkGetInstanceProcAddr) {
        return false;
    }

    fp_vkCreateInstance = reinterpret_cast<PFN_vkCreateInstance>(
        fp_vkGetInstanceProcAddr(nullptr, "vkCreateInstance"));
    return fp_vkCreateInstance != nullptr;
}

bool VulkanContext::CreateAppWindow() {
    m_hinstance = GetModuleHandleW(nullptr);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = m_hinstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"GravityRush2_WindowClass";

    RegisterClassExW(&wc);

    RECT r = {0, 0, static_cast<LONG>(m_width), static_cast<LONG>(m_height)};
    DWORD style = WS_OVERLAPPEDWINDOW;
    if (m_fullscreen) {
        style = WS_POPUP;
    }
    AdjustWindowRect(&r, style, FALSE);

    m_hwnd = CreateWindowExW(
        0,
        wc.lpszClassName,
        L"Gravity Rush 2 (PC Port) — 60 FPS [Argon Engine]",
        style,
        CW_USEDEFAULT, CW_USEDEFAULT,
        r.right - r.left, r.bottom - r.top,
        nullptr, nullptr, m_hinstance, nullptr
    );

    if (!m_hwnd) return false;

    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);

    LOG_INFO("VULKAN", "Win32 Game Window created (Handle: 0x" + 
             std::to_string(reinterpret_cast<uintptr_t>(m_hwnd)) + ")");
    return true;
}

bool VulkanContext::CreateInstance() {
    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "Gravity Rush 2 PC";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 11, 0);
    app_info.pEngineName = "Argon Port Core";
    app_info.engineVersion = VK_MAKE_VERSION(2, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_2;

    std::vector<const char*> extensions;
    if (!m_headless) {
        extensions.push_back("VK_KHR_surface");
        extensions.push_back("VK_KHR_win32_surface");
    }

    VkInstanceCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ci.pApplicationInfo = &app_info;
    ci.enabledExtensionCount = static_cast<u32>(extensions.size());
    ci.ppEnabledExtensionNames = extensions.data();

    if (fp_vkCreateInstance(&ci, nullptr, &m_vk_instance) != VK_SUCCESS) {
        return false;
    }

    // Load instance-level functions
    #define LOAD_INST(fn) fp_##fn = reinterpret_cast<PFN_##fn>(fp_vkGetInstanceProcAddr(m_vk_instance, #fn))
    LOAD_INST(vkDestroyInstance);
    LOAD_INST(vkEnumeratePhysicalDevices);
    LOAD_INST(vkGetPhysicalDeviceProperties);
    LOAD_INST(vkGetPhysicalDeviceQueueFamilyProperties);
    LOAD_INST(vkCreateDevice);
    LOAD_INST(vkDestroyDevice);
    LOAD_INST(vkGetDeviceQueue);
    LOAD_INST(vkDeviceWaitIdle);

    LOAD_INST(vkCreateWin32SurfaceKHR);
    LOAD_INST(vkDestroySurfaceKHR);
    LOAD_INST(vkGetPhysicalDeviceSurfaceSupportKHR);
    LOAD_INST(vkGetPhysicalDeviceSurfaceCapabilitiesKHR);
    LOAD_INST(vkGetPhysicalDeviceSurfaceFormatsKHR);
    LOAD_INST(vkGetPhysicalDeviceSurfacePresentModesKHR);
    #undef LOAD_INST

    LOG_INFO("VULKAN", "Vulkan 1.2 Instance created successfully.");
    return true;
}

bool VulkanContext::SelectPhysicalDevice() {
    u32 gpu_count = 0;
    fp_vkEnumeratePhysicalDevices(m_vk_instance, &gpu_count, nullptr);
    if (gpu_count == 0) {
        LOG_FATAL("VULKAN", "No Vulkan-compatible physical devices found!");
        return false;
    }

    std::vector<VkPhysicalDevice> gpus(gpu_count);
    fp_vkEnumeratePhysicalDevices(m_vk_instance, &gpu_count, gpus.data());

    // Prefer Discrete GPU matching our probed vendor profile
    const auto& caps = GPU::GetCapabilities();
    VkPhysicalDevice selected_gpu = VK_NULL_HANDLE;

    for (const auto& gpu : gpus) {
        VkPhysicalDeviceProperties props;
        fp_vkGetPhysicalDeviceProperties(gpu, &props);

        LOG_INFO("VULKAN", "Discovered Physical GPU: " + std::string(props.deviceName) + 
                 " (Vendor ID: 0x" + std::to_string(props.vendorID) + ")");

        if (caps.vendor_id != 0 && props.vendorID == caps.vendor_id) {
            selected_gpu = gpu;
            break;
        }
    }

    if (!selected_gpu) {
        selected_gpu = gpus[0]; // Fallback to first device
    }

    m_vk_physical_device = selected_gpu;

    VkPhysicalDeviceProperties sel_props;
    fp_vkGetPhysicalDeviceProperties(m_vk_physical_device, &sel_props);
    LOG_INFO("VULKAN", "Selected Physical GPU for Rendering: " + std::string(sel_props.deviceName));

    // Find queue family supporting graphics and presentation
    u32 qf_count = 0;
    fp_vkGetPhysicalDeviceQueueFamilyProperties(m_vk_physical_device, &qf_count, nullptr);
    std::vector<VkQueueFamilyProperties> q_props(qf_count);
    fp_vkGetPhysicalDeviceQueueFamilyProperties(m_vk_physical_device, &qf_count, q_props.data());

    for (u32 i = 0; i < qf_count; ++i) {
        if (q_props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            m_graphics_queue_family = i;
            if (m_vk_surface && fp_vkGetPhysicalDeviceSurfaceSupportKHR) {
                VkBool32 present_supported = VK_FALSE;
                fp_vkGetPhysicalDeviceSurfaceSupportKHR(m_vk_physical_device, i, m_vk_surface, &present_supported);
                if (present_supported) {
                    m_present_queue_family = i;
                    break;
                }
            } else {
                m_present_queue_family = i;
                break;
            }
        }
    }

    return true;
}

bool VulkanContext::CreateLogicalDevice() {
    float queue_priority = 1.0f;
    VkDeviceQueueCreateInfo qci{};
    qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    qci.queueFamilyIndex = m_graphics_queue_family;
    qci.queueCount = 1;
    qci.pQueuePriorities = &queue_priority;

    std::vector<const char*> dev_extensions;
    if (!m_headless && m_vk_surface) {
        dev_extensions.push_back("VK_KHR_swapchain");
    }

    VkDeviceCreateInfo dci{};
    dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dci.queueCreateInfoCount = 1;
    dci.pQueueCreateInfos = &qci;
    dci.enabledExtensionCount = static_cast<u32>(dev_extensions.size());
    dci.ppEnabledExtensionNames = dev_extensions.data();

    if (fp_vkCreateDevice(m_vk_physical_device, &dci, nullptr, &m_vk_device) != VK_SUCCESS) {
        return false;
    }

    fp_vkGetDeviceQueue(m_vk_device, m_graphics_queue_family, 0, &m_graphics_queue);
    fp_vkGetDeviceQueue(m_vk_device, m_present_queue_family, 0, &m_present_queue);

    // Load device-level swapchain and synchronization functions
    #define LOAD_DEV(fn) fp_##fn = reinterpret_cast<PFN_##fn>(fp_vkGetInstanceProcAddr(m_vk_instance, #fn))
    LOAD_DEV(vkCreateSwapchainKHR);
    LOAD_DEV(vkDestroySwapchainKHR);
    LOAD_DEV(vkGetSwapchainImagesKHR);
    LOAD_DEV(vkAcquireNextImageKHR);
    LOAD_DEV(vkQueuePresentKHR);
    LOAD_DEV(vkCreateImageView);
    LOAD_DEV(vkDestroyImageView);
    LOAD_DEV(vkQueueSubmit);
    LOAD_DEV(vkCreateSemaphore);
    LOAD_DEV(vkDestroySemaphore);
    #undef LOAD_DEV

    LOG_INFO("VULKAN", "Vulkan Logical Device and Queues created successfully.");
    return true;
}

bool VulkanContext::CreateSwapchain() {
    VkSurfaceCapabilitiesKHR caps;
    fp_vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_vk_physical_device, m_vk_surface, &caps);

    u32 image_count = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && image_count > caps.maxImageCount) {
        image_count = caps.maxImageCount;
    }

    VkSwapchainCreateInfoKHR sci{};
    sci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    sci.surface = m_vk_surface;
    sci.minImageCount = image_count;
    sci.imageFormat = m_swapchain_format;
    sci.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    sci.imageExtent = {m_width, m_height};
    sci.imageArrayLayers = 1;
    sci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    sci.preTransform = caps.currentTransform;
    sci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    sci.presentMode = VK_PRESENT_MODE_FIFO_KHR; // VSync / 60 FPS lockstep
    sci.clipped = VK_TRUE;

    if (fp_vkCreateSwapchainKHR(m_vk_device, &sci, nullptr, &m_vk_swapchain) != VK_SUCCESS) {
        return false;
    }

    u32 actual_img_count = 0;
    fp_vkGetSwapchainImagesKHR(m_vk_device, m_vk_swapchain, &actual_img_count, nullptr);
    m_swapchain_images.resize(actual_img_count);
    fp_vkGetSwapchainImagesKHR(m_vk_device, m_vk_swapchain, &actual_img_count, m_swapchain_images.data());

    m_swapchain_image_views.resize(actual_img_count);
    for (u32 i = 0; i < actual_img_count; ++i) {
        VkImageViewCreateInfo vci{};
        vci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        vci.image = m_swapchain_images[i];
        vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vci.format = m_swapchain_format;
        vci.components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                          VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
        vci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        vci.subresourceRange.levelCount = 1;
        vci.subresourceRange.layerCount = 1;

        fp_vkCreateImageView(m_vk_device, &vci, nullptr, &m_swapchain_image_views[i]);
    }

    LOG_INFO("VULKAN", "Vulkan Swapchain configured with " + 
             std::to_string(actual_img_count) + " presentation buffers (" + 
             std::to_string(m_width) + "x" + std::to_string(m_height) + ").");
    return true;
}

void VulkanContext::PresentFrame() {
    if (!m_initialized || !m_vk_swapchain || !fp_vkQueuePresentKHR || !m_vk_device) return;

    u32 image_index = 0;
    if (fp_vkAcquireNextImageKHR && m_image_available_semaphore) {
        VkResult res = fp_vkAcquireNextImageKHR(
            m_vk_device,
            m_vk_swapchain,
            UINT64_MAX,
            m_image_available_semaphore,
            VK_NULL_HANDLE,
            &image_index
        );
        if (res == VK_ERROR_OUT_OF_DATE_KHR) {
            return;
        } else if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR) {
            return;
        }
        m_current_image_index = image_index;
    }

    if (fp_vkQueueSubmit && m_render_finished_semaphore && m_image_available_semaphore) {
        VkPipelineStageFlags wait_stages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo si{};
        si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        si.waitSemaphoreCount = 1;
        si.pWaitSemaphores = &m_image_available_semaphore;
        si.pWaitDstStageMask = &wait_stages;
        si.signalSemaphoreCount = 1;
        si.pSignalSemaphores = &m_render_finished_semaphore;
        fp_vkQueueSubmit(m_graphics_queue, 1, &si, VK_NULL_HANDLE);

        VkPresentInfoKHR pi{};
        pi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        pi.waitSemaphoreCount = 1;
        pi.pWaitSemaphores = &m_render_finished_semaphore;
        pi.swapchainCount = 1;
        pi.pSwapchains = &m_vk_swapchain;
        pi.pImageIndices = &m_current_image_index;

        fp_vkQueuePresentKHR(m_present_queue, &pi);
    } else {
        VkPresentInfoKHR pi{};
        pi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        if (m_image_available_semaphore) {
            pi.waitSemaphoreCount = 1;
            pi.pWaitSemaphores = &m_image_available_semaphore;
        }
        pi.swapchainCount = 1;
        pi.pSwapchains = &m_vk_swapchain;
        pi.pImageIndices = &m_current_image_index;

        fp_vkQueuePresentKHR(m_present_queue, &pi);
    }
}

bool VulkanContext::ProcessWindowMessages() {
    if (!m_hwnd) return true;

    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) return false;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return true;
}

} // namespace Video
