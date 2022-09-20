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
#include <chrono>

namespace RenderEngine {

    class FrameClock {
    public:
        FrameClock() {
            tStart = std::chrono::high_resolution_clock::now();
        }

        void frame() {
            tFinish = std::chrono::high_resolution_clock::now();
            m_frame_time = std::chrono::duration<double, std::milli>(tFinish - tStart).count() / 1000.0;
            tStart = tFinish;
            m_total_time += m_frame_time;
            m_frames_elapsed++;
            m_time_elapsed += m_frame_time;
            if (m_time_elapsed > 1.0) {
                m_fps = (double) m_frames_elapsed / m_time_elapsed;
                m_time_elapsed = 0.0f;
                m_frames_elapsed = 0u;
            }
        }

        double frameTime() const {
            return m_frame_time;
        }

        double fps() const {
            return m_fps;
        }

        double totalTime() const {
            return m_total_time;
        }

    private:
        std::chrono::time_point<std::chrono::high_resolution_clock,
                std::chrono::duration<double>> tStart, tFinish;
        double m_frame_time, m_fps, m_total_time;
        uint32_t m_frames_elapsed;
        double m_time_elapsed;
    };

    class Window {
    public:
        Window(uint32_t width, uint32_t height, std::string const &title);

        Window(Window const &another) = delete;

        Window(Window &&another) noexcept: m_window(another.m_window) {
            m_windowMap[m_window] = this;
            another.m_window = nullptr;
        }

        Window &operator=(Window const &another) = delete;

        Window &operator=(Window &&another) noexcept {
            m_window = another.m_window;
            m_windowMap[m_window] = this;
            another.m_window = nullptr;
            return *this;
        };

        virtual ~Window();


        bool shouldClose() const {
            return glfwWindowShouldClose(m_window);
        }

        bool minimized() const{
            return glfwGetWindowAttrib(m_window, GLFW_ICONIFIED);
        }

        void close() const {
            glfwSetWindowShouldClose(m_window, true);
        }

        void disableCursor() {
            if (m_cursorDisabled)
                return;
            glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            m_cursorDisabled = true;
        }

        void enableCursor() {
            if (!m_cursorDisabled)
                return;
            glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            m_cursorDisabled = false;
        }

        void toggleCursor() {
            if (cursorDisabled())
                enableCursor();
            else
                disableCursor();
        }

        bool cursorDisabled() const {
            return m_cursorDisabled;
        }

        bool keyPressed(int key) const {
            return glfwGetKey(m_window, key) == GLFW_PRESS;
        }

        std::pair<double, double> cursorPos() const {
            std::pair<double, double> ret{};

            glfwGetCursorPos(m_window, &ret.first, &ret.second);

            return ret;
        }

        bool mouseButtonPressed(int button) const {
            return glfwGetMouseButton(m_window, button) == GLFW_PRESS;
        }

        std::pair<int, int> framebufferExtents() const {
            std::pair<int, int> ret{};
            glfwGetFramebufferSize(m_window, &ret.first, &ret.second);
            return ret;
        }

        FrameClock const &clock() const {
            return m_clock;
        }

        vkw::Surface surface(vkw::Instance &instance) const;

        static void pollEvents() {
            initImpl();
            glfwPollEvents();
            for (auto &window: m_windowMap) {
                window.second->onPollEvents();
                window.second->m_clock.frame();
            }
        }

        static vkw::Instance
        vulkanInstance(vkw::Library &vulkanLib, std::vector<vkw::ext> extensions = {}, bool enableValidation = true);

    protected:
        virtual void keyInput(int key, int scancode, int action, int mods) {};

        virtual void mouseMove(double xpos, double ypos, double xdelta, double ydelta) {};

        virtual void mouseInput(int button, int action, int mods) {}

        virtual void mouseScroll(double xOffset, double yOffset) {};

        virtual void charInput(unsigned int codepoint) {}

        virtual void onPollEvents() {};

    private:

        static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods) {
            m_windowMap.at(window)->keyInput(key, scancode, action, mods);
        }

        static void cursor_position_callback(GLFWwindow *window, double xpos, double ypos) {
            auto *window_handle = m_windowMap.at(window);
            double xdelta = xpos - window_handle->m_cursor_x;
            double ydelta = ypos - window_handle->m_cursor_y;
            window_handle->m_cursor_x = xpos;
            window_handle->m_cursor_y = ypos;
            window_handle->mouseMove(xpos, ypos, xdelta, ydelta);
        }

        static void mouse_button_callback(GLFWwindow *window, int button, int action, int mods) {
            m_windowMap.at(window)->mouseInput(button, action, mods);
        }

        static void character_callback(GLFWwindow* window, unsigned int codepoint)
        {
            m_windowMap.at(window)->charInput(codepoint);
        }

        static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
            m_windowMap.at(window)->mouseScroll(xoffset, yoffset);
        }

        static void initImpl();

        static std::map<GLFWwindow *, Window *> m_windowMap;

        double m_cursor_x = 0.0;
        double m_cursor_y = 0.0;
        bool m_cursorDisabled = false;
        FrameClock m_clock;
        GLFWwindow *m_window = nullptr;
    };
}
#endif //TESTAPP_WINDOW_H
