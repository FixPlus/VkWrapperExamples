#ifndef TESTAPP_CAMERA_H
#define TESTAPP_CAMERA_H

#include <algorithm>
#include <array>
#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace TestApp {

class Camera {
public:
  Camera() = default;

  void lookAt(glm::vec3 eyePos, glm::vec3 center,
              glm::vec3 up = glm::vec3{0.0f, 1.0f, 0.0f});

  void set(glm::vec3 position) {
    m_position = position;
    m_need_set_matrix = true;
  }

  void move(glm::vec3 shift) {
    m_position += shift;
    m_need_set_matrix = true;
  }

  void setOrientation(float phi, float psi, float tilt) {
    m_Psi = std::clamp(psi, -89.0f, 89.0f);
    int extra = phi / 360.0f;

    m_Phi = phi - static_cast<float>(extra) * 360.0f;

    m_Tilt = std::clamp(tilt, -90.0f, 90.0f);
    m_need_set_matrix = true;
  }

  int rotate(float dphi, float dpsi, float dtilt) {
    m_Psi += dpsi;
    m_Phi += dphi;
    m_Tilt += dtilt;

    m_Psi = std::clamp(m_Psi, -89.0f, 89.0f);
    int extra = glm::floor(m_Phi / 360.0f);

    m_Phi = m_Phi - static_cast<float>(extra) * 360.0f;

    m_Tilt = std::clamp(m_Tilt, -90.0f, 90.0f);
    m_need_set_matrix = true;

    return extra;
  }

  glm::mat4 const &projection() const { return m_projection; }

  glm::mat4 const &cameraSpace() const { return m_view; }

  void setMatrices() {
    if (m_need_set_matrix)
      m_set_matrix();
  }

  glm::vec3 position() const { return m_position; }

  glm::vec3 viewDirection() const;

  float phi() const { return m_Phi; }

  float psi() const { return m_Psi; }

  float tilt() const { return m_Tilt; }

  bool offBounds(glm::vec3 cubeBegin, glm::vec3 cubeEnd) const;
  /** Here child classes can push their state during frame time. */
  virtual void update(float deltaTime) { setMatrices(); };

  virtual ~Camera() = default;

private:
  glm::vec3 m_position{};

  float m_Phi = 0.0f;
  float m_Psi = 0.0f;
  float m_Tilt = 0.0f;

  void m_set_matrix();

  glm::mat4 m_view{};

protected:
  bool m_need_set_matrix = true;
  virtual glm::mat4 m_get_projection() const = 0;
  glm::mat4 m_projection{};
};

class CameraPerspective : virtual public Camera {
public:
  explicit CameraPerspective(float fov = 60.0f, float ratio = 16.0f / 9.0f)
      : m_fov(glm::radians(fov)), m_ratio(ratio) {}

  float fov() const { return m_fov; }

  void setFov(float fov) {
    m_fov = fov;
    m_need_set_matrix = true;
  }

  float ratio() const { return m_ratio; }

  void setRatio(float ratio) {
    m_ratio = ratio;
    m_need_set_matrix = true;
  }

  float nearPlane() const { return m_near_plane; }

  void setNearPlane(float nearPlane) {
    m_near_plane = nearPlane;
    m_need_set_matrix = true;
  }

  float farPlane() const { return m_far_plane; }

  void setFarPlane(float farPlane) {
    m_far_plane = farPlane;
    m_need_set_matrix = true;
  }

private:
  glm::mat4 m_get_projection() const final;
  float m_ratio;
  float m_fov;
  float m_near_plane = 0.1f;
  float m_far_plane = 1000.0f;
};

class CameraOrtho : virtual public Camera {
public:
  explicit CameraOrtho(float left = -1.0f, float right = 1.0f,
                       float bottom = 1.0f, float top = -1.0f,
                       float zNear = 0.0f, float zFar = 1.0f)
      : m_left(left), m_right(right), m_top(top), m_bottom(bottom),
        m_z_near(zNear), m_z_far(zFar){};

  void setLeft(float left) {
    m_left = left;
    m_need_set_matrix = true;
  }

  float left() const { return m_left; }

  void setRight(float right) {
    m_right = right;
    m_need_set_matrix = true;
  }

  float right() const { return m_right; }

  void setTop(float top) {
    m_top = top;
    m_need_set_matrix = true;
  }

  float top() const { return m_left; }

  void setBottom(float bottom) {
    m_bottom = bottom;
    m_need_set_matrix = true;
  }

  float bottom() const { return m_bottom; }

  void setZNear(float zNear) {
    m_z_near = zNear;
    m_need_set_matrix = true;
  }

  float zNear() const { return m_z_near; }

  void setZFar(float zFar) {
    m_z_far = zFar;
    m_need_set_matrix = true;
  }

  float zFar() const { return m_z_far; }

private:
  glm::mat4 m_get_projection() const final;
  float m_left, m_right, m_top, m_bottom;
  float m_z_near, m_z_far;
};

/** This class sets shadow cascade centers and radius automatically. */
template <uint32_t Cascades>
class ShadowCascadesCamera : public CameraPerspective {
public:
  explicit ShadowCascadesCamera(float fov = 60.0f, float ratio = 16.0f / 9.0f)
      : CameraPerspective(fov, ratio), m_far_clip(farPlane()){};

  void moveSplitLambda(float deltaLambda) {
    m_split_lambda = std::clamp(m_split_lambda + deltaLambda, 0.0f, 1.0f);
  }

  void setSplitLambda(float lambda) {
    m_split_lambda = std::clamp(lambda, 0.0f, 1.0f);
  }

  void setFarClip(float farClip) { m_far_clip = farClip; }

  float farClip() const { return m_far_clip; }

  void update(float deltaTime) override {
    float cascadeSplits[Cascades];

    float farClip = m_far_clip;
    float frustumRatio = (farClip / farPlane());
    float nearClip = nearPlane();
    float clipRange = farClip - nearClip;
    float clipRatio = farClip / nearClip;

    for (int i = 0; i < Cascades; ++i) {
      float p = (float)(i + 1) / (float)Cascades;
      float log = nearClip * std::pow(clipRatio, p);
      float linear = nearClip + clipRange * p;
      float d = (log - linear) * m_split_lambda + linear;
      cascadeSplits[i] = (d - nearClip) / clipRange;
    }
    auto inverseProjection = glm::inverse(projection() * cameraSpace());

    std::array<glm::vec3, 8> frustumCorners = {
        glm::vec3(-1.0f, 1.0f, -1.0f), glm::vec3(1.0f, 1.0f, -1.0f),
        glm::vec3(1.0f, -1.0f, -1.0f), glm::vec3(-1.0f, -1.0f, -1.0f),
        glm::vec3(-1.0f, 1.0f, 1.0f),  glm::vec3(1.0f, 1.0f, 1.0f),
        glm::vec3(1.0f, -1.0f, 1.0f),  glm::vec3(-1.0f, -1.0f, 1.0f),
    };

    for (int i = 0; i < 8; ++i) {
      auto worldSpace =
          inverseProjection * glm::vec4(frustumCorners.at(i), 1.0f);
      frustumCorners.at(i) = worldSpace / worldSpace.w;
    }

    auto lastSplitDist = 0.0f;

    for (int i = 0; i < Cascades; ++i) {
      auto splitDist = cascadeSplits[i];
      std::array<glm::vec3, 8> innerFrustumCorners{};
      std::copy(frustumCorners.begin(), frustumCorners.end(),
                innerFrustumCorners.begin());

      for (uint32_t j = 0; j < 4; j++) {
        glm::vec3 dist = frustumCorners[j + 4] - frustumCorners[j];
        innerFrustumCorners[j + 4] =
            frustumCorners[j] + (dist * splitDist * frustumRatio);
        innerFrustumCorners[j] =
            frustumCorners[j] + (dist * lastSplitDist * frustumRatio);
      }

      glm::vec3 innerFrustumCenter{0.0f};

      for (auto &corner : innerFrustumCorners)
        innerFrustumCenter += corner;

      innerFrustumCenter /= 8.0f;

      float frustumCircumscribedRadius = 0.0f;

#undef max
      for (auto &corner : innerFrustumCorners) {
        frustumCircumscribedRadius =
            glm::max(frustumCircumscribedRadius,
                     glm::length(corner - innerFrustumCenter));
      }

      m_cascades[i].radius = frustumCircumscribedRadius;
      m_cascades[i].center = innerFrustumCenter;
      m_cascades[i].split = nearClip + splitDist * clipRange;
      lastSplitDist = splitDist;
    }
  };

  struct Cascade {
    glm::vec3 center;
    float radius;
    float split;
  };

  Cascade const &cascade(uint32_t i) const { return m_cascades.at(i); }

  float splitLambda() const { return m_split_lambda; }

private:
  std::array<Cascade, Cascades> m_cascades;

  float m_split_lambda = 0.5f;
  float m_far_clip;
};

class ControlledCamera : virtual public Camera {
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

  float rotateInertia = 5.0f;
  float targetPhi = 0.0f;
  float targetPsi = 0.0f;

  void update(float deltaTime) override;

  void stop() { m_velocity *= 0.0f; }

private:
  glm::vec3 m_velocity{};
};
} // namespace TestApp
#endif // TESTAPP_CAMERA_H
