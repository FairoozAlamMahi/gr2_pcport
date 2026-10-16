#include "video/vulkan_context.h"
#include "gpu/gnm_driver.h"
#include "memory/vm.h"
#include "common/logger.h"
#include <iostream>
#include <algorithm>
#include <cstring>

namespace Video {

static inline std::string ToHex(u64 val) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%llX", static_cast<unsigned long long>(val));
    return std::string(buf);
}

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
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
        SetFocus(hwnd);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xFFF0) == SC_SCREENSAVE || (wParam & 0xFFF0) == SC_MONITORPOWER) {
            return 0; // Prevent monitor sleep during gameplay
        }
        break;
    case WM_CLOSE:
        LOG_INFO("VULKAN", "Window close requested by user.");
        ExitProcess(0);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            LOG_INFO("VULKAN", "Escape key pressed; exiting process.");
            ExitProcess(0);
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
        m_window_ready_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        m_window_running.store(true, std::memory_order_relaxed);
        std::thread window_thread(&VulkanContext::WindowThreadLoop, this);
        window_thread.detach();
        WaitForSingleObject(m_window_ready_event, INFINITE);
        CloseHandle(m_window_ready_event);
        m_window_ready_event = NULL;

        if (!m_hwnd) {
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

    if (m_vk_device && fp_vkCreateSemaphore && fp_vkCreateFence) {
        VkSemaphoreCreateInfo sci{};
        sci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fci{};
        fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        m_image_available_semaphores.resize(MAX_FRAMES_IN_FLIGHT, VK_NULL_HANDLE);
        m_render_finished_semaphores.resize(MAX_FRAMES_IN_FLIGHT, VK_NULL_HANDLE);
        m_in_flight_fences.resize(MAX_FRAMES_IN_FLIGHT, VK_NULL_HANDLE);

        for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            if (fp_vkCreateSemaphore(m_vk_device, &sci, nullptr, &m_image_available_semaphores[i]) != VK_SUCCESS ||
                fp_vkCreateSemaphore(m_vk_device, &sci, nullptr, &m_render_finished_semaphores[i]) != VK_SUCCESS ||
                fp_vkCreateFence(m_vk_device, &fci, nullptr, &m_in_flight_fences[i]) != VK_SUCCESS) {
                LOG_ERROR("VULKAN", "Failed to create presentation synchronization objects for frame " + std::to_string(i));
                return false;
            }
        }
        LOG_INFO("VULKAN", "Vulkan triple-buffered presentation semaphores and fences created successfully.");
    }

    if (!m_headless && m_vk_surface) {
        if (!CreateSwapchain()) {
            LOG_ERROR("VULKAN", "Failed to create Vulkan presentation swapchain.");
            return false;
        }
        CreateStagingResources();
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

    if (m_staging_mapped_ptr && fp_vkUnmapMemory) {
        fp_vkUnmapMemory(m_vk_device, m_staging_memory);
        m_staging_mapped_ptr = nullptr;
    }

    if (m_staging_buffer && fp_vkDestroyBuffer) {
        fp_vkDestroyBuffer(m_vk_device, m_staging_buffer, nullptr);
        m_staging_buffer = VK_NULL_HANDLE;
    }

    if (m_staging_memory && fp_vkFreeMemory) {
        fp_vkFreeMemory(m_vk_device, m_staging_memory, nullptr);
        m_staging_memory = VK_NULL_HANDLE;
    }

    if (m_cmd_pool && fp_vkDestroyCommandPool) {
        fp_vkDestroyCommandPool(m_vk_device, m_cmd_pool, nullptr);
        m_cmd_pool = VK_NULL_HANDLE;
    }

    for (auto fence : m_in_flight_fences) {
        if (fence && fp_vkDestroyFence) fp_vkDestroyFence(m_vk_device, fence, nullptr);
    }
    m_in_flight_fences.clear();

    for (auto sem : m_render_finished_semaphores) {
        if (sem && fp_vkDestroySemaphore) fp_vkDestroySemaphore(m_vk_device, sem, nullptr);
    }
    m_render_finished_semaphores.clear();

    for (auto sem : m_image_available_semaphores) {
        if (sem && fp_vkDestroySemaphore) fp_vkDestroySemaphore(m_vk_device, sem, nullptr);
    }
    m_image_available_semaphores.clear();

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

    if (m_window_running.load(std::memory_order_relaxed)) {
        m_window_running.store(false, std::memory_order_relaxed);
        if (m_hwnd) {
            PostMessageW(m_hwnd, WM_CLOSE, 0, 0);
        }
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

    // Set Per-Monitor DPI Awareness so Windows does not stretch or virtualize client rect
    typedef BOOL(WINAPI * PFN_SetProcessDpiAwarenessContext)(DPI_AWARENESS_CONTEXT);
    auto pfn_set_dpi = reinterpret_cast<PFN_SetProcessDpiAwarenessContext>(
        GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetProcessDpiAwarenessContext"));
    if (pfn_set_dpi) {
        pfn_set_dpi(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    } else {
        SetProcessDPIAware();
    }

    DisableProcessWindowsGhosting();
    RegisterClassExW(&wc);

    int screen_w = GetSystemMetrics(SM_CXSCREEN);
    int screen_h = GetSystemMetrics(SM_CYSCREEN);

    DWORD style = WS_OVERLAPPEDWINDOW;
    int win_x = CW_USEDEFAULT;
    int win_y = CW_USEDEFAULT;
    int win_w = 0;
    int win_h = 0;

    if (m_fullscreen || (m_width >= static_cast<u32>(screen_w) && m_height >= static_cast<u32>(screen_h))) {
        style = WS_POPUP;
        win_x = 0;
        win_y = 0;
        win_w = screen_w;
        win_h = screen_h;
    } else {
        RECT r = {0, 0, static_cast<LONG>(m_width), static_cast<LONG>(m_height)};
        AdjustWindowRect(&r, style, FALSE);
        win_w = r.right - r.left;
        win_h = r.bottom - r.top;
    }

    m_hwnd = CreateWindowExW(
        0,
        wc.lpszClassName,
        L"Gravity Rush 2 (PC Port) — 60 FPS [Argon Engine]",
        style,
        win_x, win_y,
        win_w, win_h,
        nullptr, nullptr, m_hinstance, nullptr
    );

    if (!m_hwnd) return false;

    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);

    LOG_INFO("VULKAN", "Win32 Game Window created (Handle: 0x" + 
             std::to_string(reinterpret_cast<uintptr_t>(m_hwnd)) + ")");
    return true;
}

void VulkanContext::WindowThreadLoop() {
    if (!CreateAppWindow()) {
        SetEvent(m_window_ready_event);
        return;
    }
    SetEvent(m_window_ready_event);

    MSG msg{};
    while (m_window_running.load(std::memory_order_relaxed)) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                m_window_running.store(false, std::memory_order_relaxed);
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        Sleep(1);
    }

    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = NULL;
    }
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
    LOAD_INST(vkGetPhysicalDeviceMemoryProperties);
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
    LOAD_DEV(vkCreateCommandPool);
    LOAD_DEV(vkDestroyCommandPool);
    LOAD_DEV(vkAllocateCommandBuffers);
    LOAD_DEV(vkFreeCommandBuffers);
    LOAD_DEV(vkBeginCommandBuffer);
    LOAD_DEV(vkEndCommandBuffer);
    LOAD_DEV(vkCmdPipelineBarrier);
    LOAD_DEV(vkCmdCopyBufferToImage);
    LOAD_DEV(vkCreateBuffer);
    LOAD_DEV(vkDestroyBuffer);
    LOAD_DEV(vkGetBufferMemoryRequirements);
    LOAD_DEV(vkAllocateMemory);
    LOAD_DEV(vkFreeMemory);
    LOAD_DEV(vkBindBufferMemory);
    LOAD_DEV(vkMapMemory);
    LOAD_DEV(vkUnmapMemory);
    LOAD_DEV(vkQueueWaitIdle);
    LOAD_DEV(vkDeviceWaitIdle);
    LOAD_DEV(vkCreateFence);
    LOAD_DEV(vkDestroyFence);
    LOAD_DEV(vkWaitForFences);
    LOAD_DEV(vkResetFences);
    #undef LOAD_DEV

    LOG_INFO("VULKAN", "Vulkan Logical Device and Queues created successfully.");
    return true;
}

bool VulkanContext::CreateSwapchain() {
    VkSurfaceCapabilitiesKHR caps{};
    if (fp_vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_vk_physical_device, m_vk_surface, &caps) != VK_SUCCESS) {
        LOG_WARN("VULKAN", "Failed to query physical device surface capabilities.");
        return false;
    }

    VkExtent2D actual_extent = {m_width, m_height};
    if (caps.currentExtent.width != UINT32_MAX && caps.currentExtent.width > 0 && caps.currentExtent.height > 0) {
        actual_extent = caps.currentExtent;
    } else {
        actual_extent.width = std::clamp(actual_extent.width, caps.minImageExtent.width, caps.maxImageExtent.width);
        actual_extent.height = std::clamp(actual_extent.height, caps.minImageExtent.height, caps.maxImageExtent.height);
    }
    m_swapchain_width = actual_extent.width;
    m_swapchain_height = actual_extent.height;

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
    sci.imageExtent = actual_extent;
    sci.imageArrayLayers = 1;
    sci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    sci.oldSwapchain = m_vk_swapchain;

    VkPresentModeKHR selected_mode = VK_PRESENT_MODE_FIFO_KHR;
    if (fp_vkGetPhysicalDeviceSurfacePresentModesKHR) {
        u32 mode_count = 0;
        fp_vkGetPhysicalDeviceSurfacePresentModesKHR(m_vk_physical_device, m_vk_surface, &mode_count, nullptr);
        std::vector<VkPresentModeKHR> modes(mode_count);
        fp_vkGetPhysicalDeviceSurfacePresentModesKHR(m_vk_physical_device, m_vk_surface, &mode_count, modes.data());
        for (auto m : modes) {
            if (m == VK_PRESENT_MODE_MAILBOX_KHR) {
                selected_mode = VK_PRESENT_MODE_MAILBOX_KHR;
                break;
            }
        }
    }
    sci.preTransform = caps.currentTransform;
    sci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    sci.presentMode = selected_mode;
    sci.clipped = VK_TRUE;

    VkSwapchainKHR new_swapchain = VK_NULL_HANDLE;
    if (fp_vkCreateSwapchainKHR(m_vk_device, &sci, nullptr, &new_swapchain) != VK_SUCCESS) {
        LOG_WARN("VULKAN", "Failed to create Vulkan swapchain.");
        return false;
    }

    // Clean up old image views and old swapchain
    for (auto iv : m_swapchain_image_views) {
        if (iv && fp_vkDestroyImageView) {
            fp_vkDestroyImageView(m_vk_device, iv, nullptr);
        }
    }
    m_swapchain_image_views.clear();

    if (m_vk_swapchain && fp_vkDestroySwapchainKHR) {
        fp_vkDestroySwapchainKHR(m_vk_device, m_vk_swapchain, nullptr);
    }
    m_vk_swapchain = new_swapchain;

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
             std::to_string(m_swapchain_width) + "x" + std::to_string(m_swapchain_height) + ").");
    return true;
}

bool VulkanContext::RecreateSwapchain() {
    if (!m_initialized || !m_vk_device || !m_vk_surface) return false;

    // Check if window is minimized
    VkSurfaceCapabilitiesKHR caps{};
    if (fp_vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_vk_physical_device, m_vk_surface, &caps) != VK_SUCCESS) {
        return false;
    }
    if (caps.currentExtent.width == 0 || caps.currentExtent.height == 0) {
        // Minimized window, skip until restored
        return false;
    }

    if (fp_vkDeviceWaitIdle) {
        fp_vkDeviceWaitIdle(m_vk_device);
    }

    return CreateSwapchain();
}

bool VulkanContext::CreateStagingResources() {
    if (m_headless || !m_vk_device) return true;

    // 1. Create Command Pool
    VkCommandPoolCreateInfo cpci{};
    cpci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cpci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cpci.queueFamilyIndex = m_graphics_queue_family;
    if (!fp_vkCreateCommandPool || fp_vkCreateCommandPool(m_vk_device, &cpci, nullptr, &m_cmd_pool) != VK_SUCCESS) {
        LOG_WARN("VULKAN", "Failed to create Vulkan command pool.");
        return false;
    }

    // 2. Allocate Primary Command Buffers (one per frame in flight)
    m_cmd_buffers.resize(MAX_FRAMES_IN_FLIGHT, VK_NULL_HANDLE);
    VkCommandBufferAllocateInfo cbai{};
    cbai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cbai.commandPool = m_cmd_pool;
    cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbai.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
    if (!fp_vkAllocateCommandBuffers || fp_vkAllocateCommandBuffers(m_vk_device, &cbai, m_cmd_buffers.data()) != VK_SUCCESS) {
        LOG_WARN("VULKAN", "Failed to allocate Vulkan command buffers.");
        return false;
    }

    // 3. Create Host-Visible Staging Buffer for Frame Blit (triple-buffered: one slot per frame in flight)
    m_staging_size = static_cast<u64>(m_width) * m_height * 4 * MAX_FRAMES_IN_FLIGHT;
    VkBufferCreateInfo bci{};
    bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size = m_staging_size;
    bci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (!fp_vkCreateBuffer || fp_vkCreateBuffer(m_vk_device, &bci, nullptr, &m_staging_buffer) != VK_SUCCESS) {
        LOG_WARN("VULKAN", "Failed to create staging buffer.");
        return false;
    }

    VkMemoryRequirements mem_reqs;
    fp_vkGetBufferMemoryRequirements(m_vk_device, m_staging_buffer, &mem_reqs);

    VkPhysicalDeviceMemoryProperties mem_props;
    fp_vkGetPhysicalDeviceMemoryProperties(m_vk_physical_device, &mem_props);

    u32 memory_type_index = UINT32_MAX;
    VkMemoryPropertyFlags required_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    for (u32 i = 0; i < mem_props.memoryTypeCount; ++i) {
        if ((mem_reqs.memoryTypeBits & (1 << i)) &&
            (mem_props.memoryTypes[i].propertyFlags & required_flags) == required_flags) {
            memory_type_index = i;
            break;
        }
    }

    if (memory_type_index == UINT32_MAX) {
        LOG_WARN("VULKAN", "No host-visible coherent memory type found for staging buffer.");
        return false;
    }

    VkMemoryAllocateInfo mai{};
    mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize = mem_reqs.size;
    mai.memoryTypeIndex = memory_type_index;

    if (!fp_vkAllocateMemory || fp_vkAllocateMemory(m_vk_device, &mai, nullptr, &m_staging_memory) != VK_SUCCESS) {
        LOG_WARN("VULKAN", "Failed to allocate staging memory.");
        return false;
    }

    fp_vkBindBufferMemory(m_vk_device, m_staging_buffer, m_staging_memory, 0);
    fp_vkMapMemory(m_vk_device, m_staging_memory, 0, m_staging_size, 0, &m_staging_mapped_ptr);
    m_current_frame_index = 0;

    LOG_INFO("VULKAN", "Staging frame buffer initialized (" + std::to_string(m_staging_size) + " bytes, " +
             std::to_string(MAX_FRAMES_IN_FLIGHT) + " frame slots).");
    return true;
}

void VulkanContext::PresentFrame(const void* frame_data) {
    if (!m_initialized || !m_vk_swapchain || !fp_vkQueuePresentKHR || !m_vk_device) return;

    u32 frame_idx = m_current_frame_index;

    // 1. Wait for previous submission on this frame slot to complete
    if (fp_vkWaitForFences && !m_in_flight_fences.empty() && m_in_flight_fences[frame_idx]) {
        fp_vkWaitForFences(m_vk_device, 1, &m_in_flight_fences[frame_idx], VK_TRUE, UINT64_MAX);
        fp_vkResetFences(m_vk_device, 1, &m_in_flight_fences[frame_idx]);
    }

    // 2. Acquire next swapchain image
    u32 image_index = 0;
    VkSemaphore img_avail_sem = m_image_available_semaphores.empty() ? VK_NULL_HANDLE : m_image_available_semaphores[frame_idx];
    if (fp_vkAcquireNextImageKHR && img_avail_sem) {
        VkResult res = fp_vkAcquireNextImageKHR(
            m_vk_device,
            m_vk_swapchain,
            UINT64_MAX,
            img_avail_sem,
            VK_NULL_HANDLE,
            &image_index
        );
        if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
            LOG_INFO("VULKAN", "vkAcquireNextImageKHR returned OUT_OF_DATE/SUBOPTIMAL (" + std::to_string(res) + "), recreating swapchain...");
            if (RecreateSwapchain()) {
                res = fp_vkAcquireNextImageKHR(
                    m_vk_device,
                    m_vk_swapchain,
                    UINT64_MAX,
                    img_avail_sem,
                    VK_NULL_HANDLE,
                    &image_index
                );
            }
        }
        if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR) {
            static int s_logged_acq_err = 0;
            if (s_logged_acq_err++ < 10) {
                LOG_WARN("VULKAN", "vkAcquireNextImageKHR returned error: " + std::to_string(res));
            }
            return;
        }
        m_current_image_index = image_index;
    }

    // 3. If game frame data is provided and staging resources exist, blit frame into swapchain image!
    VkCommandBuffer cmd_buf = (frame_idx < m_cmd_buffers.size()) ? m_cmd_buffers[frame_idx] : VK_NULL_HANDLE;
    VkSemaphore rnd_done_sem = (frame_idx < m_render_finished_semaphores.size()) ? m_render_finished_semaphores[frame_idx] : VK_NULL_HANDLE;
    VkFence flight_fence = (frame_idx < m_in_flight_fences.size()) ? m_in_flight_fences[frame_idx] : VK_NULL_HANDLE;

    if (frame_data && m_staging_mapped_ptr && cmd_buf && fp_vkBeginCommandBuffer) {
        const void* src_pixels = frame_data;
        if (Memory::VirtualMemoryManager::Instance().IsGuestAddress(frame_data)) {
            src_pixels = Memory::VirtualMemoryManager::Instance().GuestToHost(reinterpret_cast<uintptr_t>(frame_data));
        }

        u64 single_frame_bytes = static_cast<u64>(m_width) * m_height * 4;
        u64 slot_offset = static_cast<u64>(frame_idx) * single_frame_bytes;
        u8* slot_dst = static_cast<u8*>(m_staging_mapped_ptr) + slot_offset;

        if (src_pixels) {
            std::memcpy(slot_dst, src_pixels, single_frame_bytes);

            static u64 s_frame_check_counter = 0;
            if (++s_frame_check_counter <= 10 || s_frame_check_counter % 60 == 0) {
                const u32* px = static_cast<const u32*>(src_pixels);
                u32 total_pixels = m_width * m_height;
                u32 non_zero = 0;
                for (u32 p = 0; p < total_pixels; p += 64) {
                    if (px[p] != 0) non_zero++;
                }
                LOG_INFO("VULKAN", "Frame #" + std::to_string(s_frame_check_counter) + 
                         " src_pixels at 0x" + ToHex(reinterpret_cast<uintptr_t>(src_pixels)) + 
                         ": non-zero sample count=" + std::to_string(non_zero) + 
                         " (center=0x" + ToHex(px[total_pixels / 2]) + ")");
            }
        }

        VkCommandBufferBeginInfo bi{};
        bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        fp_vkBeginCommandBuffer(cmd_buf, &bi);

        VkImage target_image = m_swapchain_images[m_current_image_index];

        // Transition swapchain image UNDEFINED -> TRANSFER_DST_OPTIMAL
        VkImageMemoryBarrier barrier1{};
        barrier1.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier1.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier1.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier1.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier1.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier1.image = target_image;
        barrier1.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier1.subresourceRange.levelCount = 1;
        barrier1.subresourceRange.layerCount = 1;
        barrier1.srcAccessMask = 0;
        barrier1.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        fp_vkCmdPipelineBarrier(
            cmd_buf,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier1
        );

        // Copy staging buffer slot to swapchain image (clamped to swapchain extent)
        u32 copy_w = std::min(m_width, m_swapchain_width);
        u32 copy_h = std::min(m_height, m_swapchain_height);

        VkBufferImageCopy region{};
        region.bufferOffset = slot_offset;
        region.bufferRowLength = m_width;
        region.bufferImageHeight = m_height;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.layerCount = 1;
        region.imageExtent = {copy_w, copy_h, 1};

        fp_vkCmdCopyBufferToImage(
            cmd_buf,
            m_staging_buffer,
            target_image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &region
        );

        // Transition swapchain image TRANSFER_DST_OPTIMAL -> PRESENT_SRC_KHR
        VkImageMemoryBarrier barrier2{};
        barrier2.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier2.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier2.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        barrier2.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier2.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier2.image = target_image;
        barrier2.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier2.subresourceRange.levelCount = 1;
        barrier2.subresourceRange.layerCount = 1;
        barrier2.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier2.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;

        fp_vkCmdPipelineBarrier(
            cmd_buf,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier2
        );

        fp_vkEndCommandBuffer(cmd_buf);

        // Submit with semaphores and fence
        VkPipelineStageFlags wait_stages = VK_PIPELINE_STAGE_TRANSFER_BIT;
        VkSubmitInfo si{};
        si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        si.waitSemaphoreCount = 1;
        si.pWaitSemaphores = &img_avail_sem;
        si.pWaitDstStageMask = &wait_stages;
        si.commandBufferCount = 1;
        si.pCommandBuffers = &cmd_buf;
        si.signalSemaphoreCount = 1;
        si.pSignalSemaphores = &rnd_done_sem;
        fp_vkQueueSubmit(m_graphics_queue, 1, &si, flight_fence);

        VkPresentInfoKHR pi{};
        pi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        pi.waitSemaphoreCount = 1;
        pi.pWaitSemaphores = &rnd_done_sem;
        pi.swapchainCount = 1;
        pi.pSwapchains = &m_vk_swapchain;
        pi.pImageIndices = &m_current_image_index;

        VkResult pres_res = fp_vkQueuePresentKHR(m_present_queue, &pi);
        if (pres_res == VK_ERROR_OUT_OF_DATE_KHR || pres_res == VK_SUBOPTIMAL_KHR) {
            LOG_INFO("VULKAN", "fp_vkQueuePresentKHR returned OUT_OF_DATE/SUBOPTIMAL (" + std::to_string(pres_res) + "), recreating swapchain...");
            RecreateSwapchain();
        } else if (pres_res != VK_SUCCESS) {
            static int s_logged_pres_err = 0;
            if (s_logged_pres_err++ < 10) {
                LOG_WARN("VULKAN", "fp_vkQueuePresentKHR returned: " + std::to_string(pres_res));
            }
        }
    } else {
        // Fallback present
        if (fp_vkQueueSubmit && rnd_done_sem && img_avail_sem) {
            VkPipelineStageFlags wait_stages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            VkSubmitInfo si{};
            si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            si.waitSemaphoreCount = 1;
            si.pWaitSemaphores = &img_avail_sem;
            si.pWaitDstStageMask = &wait_stages;
            si.signalSemaphoreCount = 1;
            si.pSignalSemaphores = &rnd_done_sem;
            fp_vkQueueSubmit(m_graphics_queue, 1, &si, flight_fence);

            VkPresentInfoKHR pi{};
            pi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            pi.waitSemaphoreCount = 1;
            pi.pWaitSemaphores = &rnd_done_sem;
            pi.swapchainCount = 1;
            pi.pSwapchains = &m_vk_swapchain;
            pi.pImageIndices = &m_current_image_index;

            VkResult pres_res = fp_vkQueuePresentKHR(m_present_queue, &pi);
            if (pres_res == VK_ERROR_OUT_OF_DATE_KHR || pres_res == VK_SUBOPTIMAL_KHR) {
                RecreateSwapchain();
            }
        }
    }

    m_current_frame_index = (m_current_frame_index + 1) % MAX_FRAMES_IN_FLIGHT;
}

bool VulkanContext::ProcessWindowMessages() {
    return m_window_running.load(std::memory_order_relaxed);
}

} // namespace Video
