#ifndef TESTAPP_SCENEPROJECTOR_H
#define TESTAPP_SCENEPROJECTOR_H
#include "Window.h"
#include "Camera.h"

namespace TestApp{

    class SceneProjector: public Window{
    public:
        SceneProjector(uint32_t width, uint32_t height, std::string const& title): Window(width, height, title),
                                                                                   m_camera(glm::radians(60.0f), (float)width / (float)height){

        }

        void update(float deltaTime){
            m_camera.update(deltaTime);
        }

        glm::mat4 projection() {
            return m_camera;
        }

        glm::vec3 cameraPos() {
            return m_camera.position();
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

        ControlledCamera m_camera;
    };
}
#endif //TESTAPP_SCENEPROJECTOR_H
