#ifndef TESTAPP_CAMERA_H
#define TESTAPP_CAMERA_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cstdint>

namespace TestApp{

    class Camera{
    public:
        Camera(float fov = 60.0f, float ratio = 16.0f / 9.0f): m_fov(fov), m_ratio(ratio){
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

        glm::mat4 const& projection() const{
            return m_projection;
        }
        glm::mat4 const& cameraSpace() const{
            return m_view;
        }

        void setMatrices() {
            if(m_need_set_matrix)
                m_set_matrix();
        }

        glm::vec3 position() const{
            return m_position;
        }

        glm::vec3 viewDirection() const;

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

        float nearPlane() const{
            return m_near_plane;
        }

        float farPlane() const{
            return m_far_plane;
        }

        /** Here child classes can push their state during frame time. */
        virtual void update(float deltaTime) { setMatrices();};

    private:
        bool m_need_set_matrix = true;
        
        float m_ratio;
        float m_fov;
        float m_near_plane = 0.1f;
        float m_far_plane = 1000.0f;

        glm::vec3 m_position{};

        float m_Phi = 0.0f;
        float m_Psi = 0.0f;
        float m_Tilt = 0.0f;

        void m_set_matrix();
        glm::mat4 m_view{};
        glm::mat4 m_projection{};
    };

    /** This class sets shadow cascade centers and radius automatically. */
    template<uint32_t Cascades>
    class ShadowCascadesCamera: virtual public Camera{
    public:

        void moveSplitLambda(float deltaLambda){
            m_split_lambda = std::clamp(m_split_lambda + deltaLambda, 0.0f, 1.0f);
        }

        void setSplitLambda(float lambda){
            m_split_lambda = std::clamp(lambda, 0.0f, 1.0f);
        }

        void update(float deltaTime) override{
            float cascadeSplits[Cascades];

            float farClip = farPlane();
            float nearClip = nearPlane();
            float clipRange = farClip - nearClip;
            float clipRatio = farClip / nearClip;

            for(int i = 0; i < Cascades; ++i){
                float p = (float)(i + 1) / (float)Cascades;
                float log = nearClip * std::pow(clipRatio, p);
                float linear = nearClip + clipRange * p;
                float d = (log - linear) * m_split_lambda + linear;
                cascadeSplits[i] = (d - nearClip) / clipRange;
            }
            auto inverseProjection = glm::inverse(projection() * cameraSpace());

            std::array<glm::vec3, 8> frustumCorners = {
                    glm::vec3(-1.0f,  1.0f, -1.0f),
                    glm::vec3( 1.0f,  1.0f, -1.0f),
                    glm::vec3( 1.0f, -1.0f, -1.0f),
                    glm::vec3(-1.0f, -1.0f, -1.0f),
                    glm::vec3(-1.0f,  1.0f,  1.0f),
                    glm::vec3( 1.0f,  1.0f,  1.0f),
                    glm::vec3( 1.0f, -1.0f,  1.0f),
                    glm::vec3(-1.0f, -1.0f,  1.0f),
            };

            for(int i = 0; i < 8; ++i){
                auto worldSpace = inverseProjection * glm::vec4(frustumCorners.at(i), 1.0f);
                frustumCorners.at(i) = worldSpace / worldSpace.w;
            }


            auto lastSplitDist = 0.0f;

            for(int i = 0; i < Cascades; ++i){
                auto splitDist = cascadeSplits[i];
                std::array<glm::vec3, 8> innerFrustumCorners{};
                std::copy(frustumCorners.begin(), frustumCorners.end(), innerFrustumCorners.begin());

                for (uint32_t j = 0; j < 4; j++) {
                    glm::vec3 dist = frustumCorners[j + 4] - frustumCorners[j];
                    innerFrustumCorners[j + 4] = frustumCorners[j] + (dist * splitDist);
                    innerFrustumCorners[j] = frustumCorners[j] + (dist * lastSplitDist);
                }

                glm::vec3 innerFrustumCenter{0.0f};

                for(auto& corner: innerFrustumCorners)
                    innerFrustumCenter += corner;

                innerFrustumCenter /= 8.0f;

                float frustumCircumscribedRadius = 0.0f;

                for(auto& corner: innerFrustumCorners){
                    frustumCircumscribedRadius = glm::max(frustumCircumscribedRadius, glm::length(corner - innerFrustumCenter));
                }

                m_cascades[i].radius = frustumCircumscribedRadius;
                m_cascades[i].center = innerFrustumCenter;
                m_cascades[i].split = nearClip + splitDist * clipRange;
                lastSplitDist = splitDist;
            }
        };



        struct Cascade{
            glm::vec3 center;
            float radius;
            float split;
        };

        Cascade const& cascade(uint32_t i) const{
            return m_cascades.at(i);
        }

        float splitLambda() const{
            return m_split_lambda;
        }

    private:
        std::array<Cascade, Cascades> m_cascades;

        float m_split_lambda = 0.5f;
    };

    class ControlledCamera: virtual public Camera{
    public:

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

        void update(float deltaTime) override;

        void stop(){
            m_velocity *= 0.0f;
        }
    private:
        glm::vec3 m_velocity{};
    };
}
#endif //TESTAPP_CAMERA_H