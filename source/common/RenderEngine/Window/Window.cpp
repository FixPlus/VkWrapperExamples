#include "Window.h"

namespace RenderEngine {

    Window::Window(uint32_t width, uint32_t height, const std::string &title) {
        initImpl();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        m_window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
        WindowCallbackHandler::get().connectWindow(*this);
        glfwGetFramebufferSize(m_window, &m_fb_width, &m_fb_height);
        glfwGetWindowSize(m_window, &m_width, &m_height);
    }

    Window::~Window() {
        if (!m_window)
            return;
        glfwDestroyWindow(m_window);
        WindowCallbackHandler::get().disconnectWindow(*this);
    }

    vkw::Surface Window::surface(vkw::Instance &instance) const {
#ifdef _WIN32
        auto surface = vkw::Surface(instance, GetModuleHandle(NULL), glfwGetWin32Window(m_window));
#elif defined(__linux__)
        auto surface = vkw::Surface(instance, glfwGetX11Display(), glfwGetX11Window (m_window));
#else
#error "Platform is not supported"
#endif

        return surface;
    }

    vkw::Instance Window::vulkanInstance(vkw::Library &vulkanLib, vkw::InstanceCreateInfo& createInfo) {
        initImpl();
        uint32_t count = 0;
        auto ext = glfwGetRequiredInstanceExtensions(&count);

        for (int i = 0; i < count; ++i)
            createInfo.requestExtension(vkw::Library::ExtensionId(ext[i]));

#ifdef __linux__
        createInfo.requestExtension(vkw::Library::ExtensionId(VK_KHR_XLIB_SURFACE_EXTENSION_NAME));
#endif
        return {vulkanLib, createInfo};
    }

    class GlfwLibKeeper{
    public:
        GlfwLibKeeper(){
            if (glfwInit() != GLFW_TRUE) {
                throw std::runtime_error("Failed to init GLFW");
            }
        }

        ~GlfwLibKeeper(){
            glfwTerminate();
        }
    };
    void Window::initImpl() {
        static GlfwLibKeeper glfwKeeper{};
    }

    void Window::pollEvents() {
        initImpl();
        WindowCallbackHandler::get().pollEvents();
    }

    WindowCallbackHandler &WindowCallbackHandler::get() {
        static WindowCallbackHandler handler;
        return handler;
    }

    void WindowCallbackHandler::connectWindow(Window &window) {
        auto* rawHandle = window.m_window;
        m_windowMap.emplace(rawHandle, &window);

        glfwSetKeyCallback(rawHandle, key_callback);
        glfwSetCursorPosCallback(rawHandle, cursor_position_callback);
        glfwSetMouseButtonCallback(rawHandle, mouse_button_callback);
        glfwSetCharCallback(rawHandle, character_callback);
        glfwSetScrollCallback(rawHandle, scroll_callback);
        glfwSetWindowSizeCallback(rawHandle, window_size_callback);
    }

    void WindowCallbackHandler::pollEvents() {
        glfwPollEvents();

        for(auto& window: m_windowMap){
            window.second->onPollEvents();
            window.second->m_clock.frame();
        }
    }

    void WindowCallbackHandler::disconnectWindow(Window &window) {
        m_windowMap.erase(window.m_window);
    }
}