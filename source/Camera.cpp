#include "Camera.h"


namespace TestApp{


    void Camera::m_set_matrix() {
        m_camera_mat = glm::mat4(1.0f);

        m_camera_mat = glm::rotate(m_camera_mat, glm::radians(-m_Psi),
                           glm::vec3(1.0f, 0.0f, 0.0f));
        m_camera_mat = glm::rotate(m_camera_mat, glm::radians(-m_Tilt),
                           glm::vec3(0.0f, 1.0f, 0.0f));
        m_camera_mat = glm::rotate(m_camera_mat, glm::radians(-m_Phi),
                                   glm::vec3(0.0f, 0.0f, 1.0f));

        m_camera_mat = glm::translate(m_camera_mat, -m_position);
        auto perspective = glm::perspective(m_fov, m_ratio, 0.1f, 100.0f);
        perspective[1][1] *= -1.0f;
        perspective = perspective * glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f),
                                     glm::vec3(1.0f, 0.0f, 0.0f));
        m_camera_mat = perspective * m_camera_mat;
    }

    void ControlledCamera::update(float deltaTime) {
        float currentSpeed = glm::length(m_velocity);

        if (currentSpeed < 0.01f)
            stop();
        else
            m_velocity *= 1.0f - (1.5f) * deltaTime / inertia;

        glm::vec3 camFront;
        camFront.x =
                glm::cos(glm::radians(psi())) * glm::sin(glm::radians(phi()));
        camFront.z = -glm::sin(glm::radians(psi()));
        camFront.y = -glm::cos(glm::radians(psi())) *
                     glm::cos(glm::radians(phi()));
        camFront = glm::normalize(-camFront);

        float acceleration = force * deltaTime / inertia;

        if (keys.up)
            m_velocity += camFront * acceleration;
        if (keys.down)
            m_velocity -= camFront * acceleration;
        if (keys.left)
            m_velocity -=
                    glm::normalize(glm::cross(camFront, glm::vec3(0.0f, 0.0f, 1.0f))) *
                    acceleration;
        if (keys.right)
            m_velocity +=
                    glm::normalize(glm::cross(camFront, glm::vec3(0.0f, 0.0f, 1.0f))) *
                    acceleration;
        if (keys.space)
            m_velocity += glm::vec3(0.0f, 0.0f, 1.0f) * acceleration;
        if (keys.shift)
            m_velocity -= glm::vec3(0.0f, 0.0f, 1.0f) * acceleration;

        move(m_velocity * deltaTime);
    }
}