#ifndef TESTAPP_WINDOW_H
#define TESTAPP_WINDOW_H


#include <vkw/Surface.hpp>

#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(__linux__)
#define GLFW_EXPOSE_NATIVE_X11
#endif

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

namespace TestApp {

    class Window {
    public:
        Window(uint32_t width, uint32_t height, std::string const& title);
        Window(Window const& another) = delete;
        Window(Window&& another) noexcept: m_window(another.m_window){
            m_windowMap[m_window] = this;
            another.m_window = nullptr;
        }

        Window& operator=(Window const& another) = delete;
        Window& operator=(Window&& another) noexcept{
            m_window = another.m_window;
            m_windowMap[m_window] = this;
            another.m_window = nullptr;
            return *this;
        };

        virtual ~Window();



        bool shouldClose() const{
            return glfwWindowShouldClose(m_window);
        }

        void disableCursor() {
            if(m_cursorDisabled)
                return;
            glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            m_cursorDisabled = true;
        }
        void enableCursor() {
            if(!m_cursorDisabled)
                return;
            glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            m_cursorDisabled = false;
        }

        void toggleCursor() {
            if(cursorDisabled())
                enableCursor();
            else
                disableCursor();
        }

        bool cursorDisabled() const{
            return m_cursorDisabled;
        }

        vkw::Surface surface(vkw::Instance& instance) const;

        static void pollEvents(){
            initImpl();
            glfwPollEvents();
        }
        static vkw::Instance vulkanInstance(vkw::Library &vulkanLib, std::vector<std::string> extensions = {}, bool enableValidation = true);

    protected:
        virtual void keyInput(int key, int scancode, int action, int mods) {};

        virtual void mouseMove(double xpos, double ypos, double xdelta, double ydelta) {};

        virtual void mouseInput(int button, int action, int mods) {}
    private:

        static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods){
            m_windowMap.at(window)->keyInput(key, scancode, action, mods);
        }
        static void cursor_position_callback(GLFWwindow* window, double xpos, double ypos){
            auto* window_handle = m_windowMap.at(window);
            double xdelta = xpos - window_handle->m_cursor_x;
            double ydelta = ypos - window_handle->m_cursor_y;
            window_handle->m_cursor_x = xpos;
            window_handle->m_cursor_y = ypos;
            window_handle->mouseMove(xpos, ypos, xdelta, ydelta);
        }

        static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods){
            m_windowMap.at(window)->mouseInput(button, action, mods);
        }

        static void initImpl();

        static std::map<GLFWwindow*, Window*> m_windowMap;

        double m_cursor_x = 0.0;
        double m_cursor_y = 0.0;
        bool m_cursorDisabled = false;
        GLFWwindow* m_window = nullptr;
    };
}
#endif //TESTAPP_WINDOW_H
