#include "Camera.h"


namespace TestApp{


    void Camera::m_set_matrix() {
        m_view = glm::mat4(1.0f);

        m_view = glm::rotate(m_view, glm::radians(m_Psi),
                           glm::vec3(1.0f, 0.0f, 0.0f));
        m_view = glm::rotate(m_view, glm::radians(m_Phi),
                           glm::vec3(0.0f, 1.0f, 0.0f));
        m_view = glm::rotate(m_view, glm::radians(m_Tilt),
                                   glm::vec3(0.0f, 0.0f, 1.0f));

        m_view = glm::translate(m_view, -m_position);
        m_projection = glm::perspective(m_fov, m_ratio, m_near_plane, m_far_plane);
        m_projection[1][1] *= -1.0f;
    }

    glm::vec3 Camera::viewDirection() const{

        glm::vec3 camFront;
        camFront.x =
                -glm::cos(glm::radians(psi())) * glm::sin(glm::radians(phi()));
        camFront.y = glm::sin(glm::radians(psi()));
        camFront.z = glm::cos(glm::radians(psi())) *
                     glm::cos(glm::radians(phi()));
        camFront = glm::normalize(camFront);

        return camFront;
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
    }
}