#ifndef TESTAPP_SCENEPROJECTOR_H
#define TESTAPP_SCENEPROJECTOR_H
#include "Window.h"
#include "Camera.h"
#include <iostream>
namespace TestApp{

    template<uint32_t Cascades>
    class SceneCamera: public ControlledCamera, public ShadowCascadesCamera<Cascades>{
    public:
        SceneCamera(float fov, float ratio): Camera(fov, ratio){};

        void update(float deltaTime) override{
            ControlledCamera::update(deltaTime);
            ShadowCascadesCamera<Cascades>::update(deltaTime);
        }
    };

    constexpr const uint32_t SHADOW_CASCADES_COUNT = 4;
    class SceneProjector: public Window{
    public:
        using SceneCameraT = SceneCamera<SHADOW_CASCADES_COUNT>;

        SceneProjector(uint32_t width, uint32_t height, std::string const& title): Window(width, height, title),
                                                                                   m_camera(glm::radians(60.0f), (float)width / (float)height){

        }

        void update(float deltaTime){
            m_camera.update(deltaTime);
        }

        SceneCameraT const& camera() {
            m_camera.setMatrices();
            return m_camera;
        }

    protected:
        void keyInput(int key, int scancode, int action, int mods) override {
            if(action == GLFW_PRESS)
                keyDown(key, mods);
            else if(action == GLFW_RELEASE)
                keyUp(key, mods);
            else
                keyRepeat(key, mods);
        };

        void mouseMove(double xpos, double ypos, double xdelta, double ydelta) override{
            if(cursorDisabled())
                m_camera.rotate(xdelta, ydelta, 0.0f);
        };



    private:
        void keyDown(int key, int mods){
            switch(key){
                case GLFW_KEY_W: m_camera.keys.up = true; break;
                case GLFW_KEY_A: m_camera.keys.left = true; break;
                case GLFW_KEY_S: m_camera.keys.down = true; break;
                case GLFW_KEY_D: m_camera.keys.right = true; break;
                case GLFW_KEY_N: m_camera.moveSplitLambda(-0.05f); std::cout << "lambda: " << m_camera.splitLambda() << std::endl; break;
                case GLFW_KEY_M: m_camera.moveSplitLambda(+0.05f); std::cout << "lambda: " << m_camera.splitLambda() << std::endl; break;
                case GLFW_KEY_C: toggleCursor(); break;
                case GLFW_KEY_LEFT_SHIFT: m_camera.keys.shift = true; break;
                case GLFW_KEY_SPACE: m_camera.keys.space = true; break;
                case GLFW_KEY_ESCAPE: close(); break;
                default:;
            }
        }
        void keyUp(int key, int mods){
            switch(key){
                case GLFW_KEY_W: m_camera.keys.up = false; break;
                case GLFW_KEY_A: m_camera.keys.left = false; break;
                case GLFW_KEY_S: m_camera.keys.down = false; break;
                case GLFW_KEY_D: m_camera.keys.right = false; break;
                case GLFW_KEY_LEFT_SHIFT: m_camera.keys.shift = false; break;
                case GLFW_KEY_SPACE: m_camera.keys.space = false; break;
                default:;
            }
        }

        void keyRepeat(int key, int mods){

        }

        SceneCameraT m_camera;
    };
}
#endif //TESTAPP_SCENEPROJECTOR_H
