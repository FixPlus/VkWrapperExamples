#include "Window.h"

namespace RenderEngine {

    std::map<GLFWwindow*, Window*> Window::m_windowMap{};

    Window::Window(uint32_t width, uint32_t height, const std::string &title) {
        initImpl();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        m_window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
        m_windowMap.emplace(m_window, this);

        glfwSetKeyCallback(m_window, key_callback);
        glfwSetCursorPosCallback(m_window, cursor_position_callback);
        glfwSetMouseButtonCallback(m_window, mouse_button_callback);
        glfwSetCharCallback(m_window, character_callback);
        glfwSetScrollCallback(m_window, scroll_callback);

    }

    Window::~Window() {
        if (!m_window)
            return;
        glfwDestroyWindow(m_window);
        m_windowMap.erase(m_window);
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

    vkw::Instance Window::vulkanInstance(vkw::Library &vulkanLib, std::vector<vkw::ext> extensions, bool enableValidation) {
        initImpl();
        uint32_t count = 0;
        auto ext = glfwGetRequiredInstanceExtensions(&count);

        for (int i = 0; i < count; ++i)
            extensions.emplace_back(vkw::Library::ExtensionId(ext[i]));

#ifdef __linux__
        extensions.emplace_back(vulkanLib.getExtensionId(VK_KHR_XLIB_SURFACE_EXTENSION_NAME));
#endif
        return {vulkanLib, extensions, enableValidation};
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
}