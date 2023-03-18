#ifndef TESTAPP_SCENEPROJECTOR_H
#define TESTAPP_SCENEPROJECTOR_H

#include "RenderEngine/Window/Window.h"
#include "Camera.h"
#include "GUI.h"

namespace TestApp {

    template<uint32_t Cascades>
    class SceneCamera : public ControlledCamera, public ShadowCascadesCamera<Cascades> {
    public:
        SceneCamera(float fov, float ratio) : ShadowCascadesCamera<Cascades>(fov, ratio) {};

        void update(float deltaTime) override {
            ControlledCamera::update(deltaTime);
            ShadowCascadesCamera<Cascades>::update(deltaTime);
        }
    };

    constexpr const uint32_t SHADOW_CASCADES_COUNT = 4;

    class SceneProjector : public WindowIO {
    public:
        using SceneCameraT = SceneCamera<SHADOW_CASCADES_COUNT>;

        SceneProjector(uint32_t width, uint32_t height, std::string const &title) : Window(width, height, title),
                                                                                    m_camera(60.0f,
                                                                                             (float) width /
                                                                                             (float) height) {

        }

        void update() {
            m_camera.update(clock().frameTime());
            m_camera.setMatrices();
        }

        SceneCameraT &camera() {
            m_camera.setMatrices();
            return m_camera;
        }

        float mouseSensitivity = 1.0f;

    protected:
        void keyInput(int key, int scancode, int action, int mods) override {
            WindowIO::keyInput(key, scancode, action, mods);
            if (guiWantCaptureKeyboard())
                return;

            if (action == GLFW_PRESS)
                keyDown(key, mods);
            else if (action == GLFW_RELEASE)
                keyUp(key, mods);
            else
                keyRepeat(key, mods);
        };

        void mouseMove(double xpos, double ypos, double xdelta, double ydelta) override {
            WindowIO::mouseMove(xpos, ypos, xdelta, ydelta);
            if (guiWantCaptureMouse())
                return;

            if (cursorDisabled()) {
                m_camera.targetPhi += xdelta * mouseSensitivity;
                m_camera.targetPsi += ydelta * mouseSensitivity;
            }

        };


    private:
        void keyDown(int key, int mods) {

            switch (key) {
                case GLFW_KEY_W:
                    m_camera.keys.up = true;
                    break;
                case GLFW_KEY_A:
                    m_camera.keys.left = true;
                    break;
                case GLFW_KEY_S:
                    m_camera.keys.down = true;
                    break;
                case GLFW_KEY_D:
                    m_camera.keys.right = true;
                    break;
                case GLFW_KEY_C:
                    toggleCursor();
                    break;
                case GLFW_KEY_LEFT_SHIFT:
                    m_camera.keys.shift = true;
                    break;
                case GLFW_KEY_SPACE:
                    m_camera.keys.space = true;
                    break;
                case GLFW_KEY_ESCAPE:
                    close();
                    break;
                default:;
            }
        }

        void keyUp(int key, int mods) {

            switch (key) {
                case GLFW_KEY_W:
                    m_camera.keys.up = false;
                    break;
                case GLFW_KEY_A:
                    m_camera.keys.left = false;
                    break;
                case GLFW_KEY_S:
                    m_camera.keys.down = false;
                    break;
                case GLFW_KEY_D:
                    m_camera.keys.right = false;
                    break;
                case GLFW_KEY_LEFT_SHIFT:
                    m_camera.keys.shift = false;
                    break;
                case GLFW_KEY_SPACE:
                    m_camera.keys.space = false;
                    break;
                default:;
            }
        }

        void keyRepeat(int key, int mods) {

        }

        SceneCameraT m_camera;
    };
}
#endif //TESTAPP_SCENEPROJECTOR_H
