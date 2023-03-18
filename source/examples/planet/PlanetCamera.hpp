#ifndef TESTAPP_PLANETCAMERA_HPP
#define TESTAPP_PLANETCAMERA_HPP

#include "Planet.hpp"

namespace TestApp {

class PlanetCamera : public CameraPerspective {
public:
  PlanetCamera() = default;

  struct Settings {
    float rotateInertia = 10.0f;
    float axisXSensitivity = 1.0f;
    float axisYSensitivity = 1.0f;
    float axisZSensitivity = 1.0f;

  } settings;

  struct Controls {
    double axisDeltaX = 0.0f;
    double axisDeltaY = 0.0f;
    double axisDeltaZ = 0.0f;
  } controls;

  void update(float deltaTime) override;

  auto phi() const { return currentPhi; }

  auto psi() const { return currentPsi; }

  void setTarget(Planet const &planet) { m_planet = &planet; }

private:
  float targetPsi = 0.0f;
  float targetPhi = 0.0f;
  float currentPsi = 0.0f;
  float currentPhi = 0.0f;
  float targetDistance = 1.5f;
  float currentDistance = 1.5f;

  Planet const *m_planet = nullptr;
};

class PlanetCameraSettings : public GUIWindow {
public:
  PlanetCameraSettings(GUIFrontEnd &gui, PlanetCamera &camera);

protected:
  void onGui() override;

private:
  float m_farPlane = 3.0f;
  std::reference_wrapper<PlanetCamera> m_camera;
};
} // namespace TestApp
#endif // TESTAPP_PLANETCAMERA_HPP
