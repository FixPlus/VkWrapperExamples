#include "Camera.h"


namespace TestApp {


    void Camera::m_set_matrix() {
        m_view = glm::mat4(1.0f);

        m_view = glm::rotate(m_view, glm::radians(m_Psi),
                             glm::vec3(1.0f, 0.0f, 0.0f));
        m_view = glm::rotate(m_view, glm::radians(m_Phi),
                             glm::vec3(0.0f, 1.0f, 0.0f));
        m_view = glm::rotate(m_view, glm::radians(m_Tilt),
                             glm::vec3(0.0f, 0.0f, 1.0f));

        m_view = glm::translate(m_view, -m_position);
        m_projection = m_get_projection();
        m_projection[1][1] *= -1.0f;
    }

    glm::vec3 Camera::viewDirection() const {

        glm::vec3 camFront;
        camFront.x =
                -glm::cos(glm::radians(psi())) * glm::sin(glm::radians(phi()));
        camFront.y = glm::sin(glm::radians(psi()));
        camFront.z = glm::cos(glm::radians(psi())) *
                     glm::cos(glm::radians(phi()));
        camFront = glm::normalize(camFront);

        return camFront;
    }

    static bool intersects(float scope1begin, float scope1end,float scope2begin, float scope2end){
        if(scope1begin < scope2begin){
            return scope1end > scope2begin;
        } else{
            return scope1begin < scope2end;
        }
    }
    bool Camera::offBounds(glm::vec3 cubeBegin, glm::vec3 cubeEnd) const{

        glm::vec4 temp;
        glm::vec3 min, max;

        temp = m_projection * m_view * glm::vec4(cubeBegin.x, cubeBegin.y, cubeBegin.z, 1.0f);

        temp /= glm::abs(temp.w);
        min = glm::vec3(temp);
        max = glm::vec3(temp);

        temp = m_projection * m_view * glm::vec4(cubeEnd.x, cubeBegin.y, cubeBegin.z, 1.0f);

        temp /= glm::abs(temp.w);
        min = glm::vec3(glm::min(temp.x, min.x), glm::min(temp.y, min.y), glm::min(temp.z, min.z));
        max = glm::vec3(glm::max(temp.x, max.x), glm::max(temp.y, max.y), glm::max(temp.z, max.z));

        temp = m_projection * m_view * glm::vec4(cubeBegin.x, cubeEnd.y, cubeBegin.z, 1.0f);

        temp /= glm::abs(temp.w);
        min = glm::vec3(glm::min(temp.x, min.x), glm::min(temp.y, min.y), glm::min(temp.z, min.z));
        max = glm::vec3(glm::max(temp.x, max.x), glm::max(temp.y, max.y), glm::max(temp.z, max.z));

        temp = m_projection * m_view * glm::vec4(cubeBegin.x, cubeBegin.y, cubeEnd.z, 1.0f);

        temp /= glm::abs(temp.w);
        min = glm::vec3(glm::min(temp.x, min.x), glm::min(temp.y, min.y), glm::min(temp.z, min.z));
        max = glm::vec3(glm::max(temp.x, max.x), glm::max(temp.y, max.y), glm::max(temp.z, max.z));

        temp = m_projection * m_view * glm::vec4(cubeEnd.x, cubeEnd.y, cubeBegin.z, 1.0f);

        temp /= glm::abs(temp.w);
        min = glm::vec3(glm::min(temp.x, min.x), glm::min(temp.y, min.y), glm::min(temp.z, min.z));
        max = glm::vec3(glm::max(temp.x, max.x), glm::max(temp.y, max.y), glm::max(temp.z, max.z));

        temp = m_projection * m_view * glm::vec4(cubeEnd.x, cubeBegin.y, cubeEnd.z, 1.0f);

        temp /= glm::abs(temp.w);
        min = glm::vec3(glm::min(temp.x, min.x), glm::min(temp.y, min.y), glm::min(temp.z, min.z));
        max = glm::vec3(glm::max(temp.x, max.x), glm::max(temp.y, max.y), glm::max(temp.z, max.z));

        temp = m_projection * m_view * glm::vec4(cubeBegin.x, cubeEnd.y, cubeEnd.z, 1.0f);

        temp /= glm::abs(temp.w);
        min = glm::vec3(glm::min(temp.x, min.x), glm::min(temp.y, min.y), glm::min(temp.z, min.z));
        max = glm::vec3(glm::max(temp.x, max.x), glm::max(temp.y, max.y), glm::max(temp.z, max.z));

        temp = m_projection * m_view * glm::vec4(cubeEnd.x, cubeEnd.y, cubeEnd.z, 1.0f);

        temp /= glm::abs(temp.w);
        min = glm::vec3(glm::min(temp.x, min.x), glm::min(temp.y, min.y), glm::min(temp.z, min.z));
        max = glm::vec3(glm::max(temp.x, max.x), glm::max(temp.y, max.y), glm::max(temp.z, max.z));



        return !intersects(-1.0f, 1.0f, min.x, max.x) ||
                !intersects(-1.0f, 1.0f, min.y, max.y) ||
                !intersects(0.0f, 1.0f, min.z, max.z);

    }
    void ControlledCamera::update(float deltaTime) {
        float currentSpeed = glm::length(m_velocity);

        if (currentSpeed < 0.01f)
            stop();
        else
            m_velocity *= 1.0f - (1.5f) * deltaTime / inertia;
        auto camFront = viewDirection();

        float acceleration = force * deltaTime / inertia;

        if (keys.up)
            m_velocity -= camFront * acceleration;
        if (keys.down)
            m_velocity += camFront * acceleration;
        if (keys.left)
            m_velocity +=
                    glm::normalize(glm::cross(camFront, glm::vec3(0.0f, 1.0f, 0.0f))) *
                    acceleration;
        if (keys.right)
            m_velocity -=
                    glm::normalize(glm::cross(camFront, glm::vec3(0.0f, 1.0f, 0.0f))) *
                    acceleration;
        if (keys.space)
            m_velocity += glm::vec3(0.0f, 1.0f, 0.0f) * acceleration;
        if (keys.shift)
            m_velocity -= glm::vec3(0.0f, 1.0f, 0.0f) * acceleration;

        move(m_velocity * deltaTime);

        targetPsi = std::clamp(targetPsi, -89.0f, 89.0f);

        float deltaPhi = targetPhi - phi();
        float deltaPsi = targetPsi - psi();

        float rotPhi = deltaPhi * deltaTime * 100.0f / rotateInertia;
        float rotPsi = deltaPsi * deltaTime * 100.0f / rotateInertia;

        auto circles = rotate(rotPhi, rotPsi, 0.0f);

        targetPhi = targetPhi - static_cast<float>(circles) * 360.0f;
    }

    glm::mat4 CameraPerspective::m_get_projection() const {
        return glm::perspective(m_fov, m_ratio, m_near_plane, m_far_plane);
    }

    void Camera::lookAt(glm::vec3 eyePos, glm::vec3 center, glm::vec3 up) {
        m_view = glm::lookAt(eyePos, center, up);
        m_position = eyePos;
        glm::vec3 dir = glm::normalize(center - eyePos);
        m_Psi = glm::asin(dir.y);
        m_Phi = glm::acos(dir.x / glm::cos(glm::asin(dir.y)));
        if(dir.z < 0.0f)
          m_Phi = 2.0f * glm::pi<float>() - m_Phi;
        m_Tilt = 0.0f;
        m_projection = m_get_projection();
        m_projection[1][1] *= -1.0f;
    }

    glm::mat4 CameraOrtho::m_get_projection() const{
        return glm::ortho(m_left, m_right, m_bottom, m_top, m_z_near, m_z_far);
    }
}