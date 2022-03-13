#ifndef TESTAPP_CAMERA_H
#define TESTAPP_CAMERA_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cstdint>

namespace TestApp{

    class Camera{
    public:
        Camera(float fov, float ratio): m_fov(fov), m_ratio(ratio){
        }

        void set(glm::vec3 position){
            m_position = position;
            m_need_set_matrix = true;
        }

        void move(glm::vec3 shift){
            m_position += shift;
            m_need_set_matrix = true;
        }

        void setOrientation(float phi, float psi, float tilt){
            m_Psi = std::clamp(psi, -89.0f, 89.0f);
            int extra = phi / 360.0f;

            m_Phi = phi - static_cast<float>(extra) * 360.0f;

            m_Tilt = std::clamp(tilt, -90.0f, 90.0f);
            m_need_set_matrix = true;
        }

        void rotate(float dphi, float  dpsi, float dtilt){
            m_Psi += dpsi;
            m_Phi += dphi;
            m_Tilt += dtilt;

            m_Psi = std::clamp(m_Psi, -89.0f, 89.0f);
            int extra = m_Phi / 360.0f;

            m_Phi = m_Phi - static_cast<float>(extra) * 360.0f;

            m_Tilt = std::clamp(m_Tilt, -90.0f, 90.0f);
            m_need_set_matrix = true;
        }

        void setAspectRatio(float ratio){
            m_ratio = ratio;
            m_need_set_matrix = true;
        }

        void setFOV(float fov) {
            m_fov = fov;
            m_need_set_matrix = true;
        }

        operator glm::mat4 const&() {
            if(m_need_set_matrix)
                m_set_matrix();
            return m_camera_mat;
        }

        glm::vec3 position() const{
            return m_position;
        }

        float phi() const{
            return m_Phi;
        }

        float psi() const{
            return m_Psi;
        }

        float tilt() const{
            return m_Tilt;
        }

        float fov() const{
            return m_fov;
        }

        float ratio() const{
            return m_ratio;
        }

    private:
        bool m_need_set_matrix = true;
        
        float m_ratio;
        float m_fov;

        glm::vec3 m_position{};

        float m_Phi = 0.0f;
        float m_Psi = 0.0f;
        float m_Tilt = 0.0f;

        void m_set_matrix();
        glm::mat4 m_camera_mat{};
    };


    class ControlledCamera: public Camera{
    public:

        ControlledCamera(float fov, float ratio): Camera(fov, ratio){}

        struct {
            bool left = false;
            bool right = false;
            bool up = false;
            bool down = false;
            bool space = false;
            bool shift = false;
        } keys;

        float inertia = 0.1f;
        float force = 20.0f;

        void update(float deltaTime);

        void stop(){
            m_velocity *= 0.0f;
        }
    private:
        glm::vec3 m_velocity{};
    };
}
#endif //TESTAPP_CAMERA_H
