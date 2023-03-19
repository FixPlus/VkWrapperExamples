#ifndef TESTAPP_PLANETCAMERA_HPP
#define TESTAPP_PLANETCAMERA_HPP

#include "Planet.hpp"

namespace TestApp {

class PlanetCamera : public CameraPerspective {
public:
  PlanetCamera() = default;

  enum class Mode { Centered = 0, Surface = 1 };

  struct Settings {
    float rotateInertia = 10.0f;
    float axisXSensitivity = 1.0f;
    float axisYSensitivity = 1.0f;
    float axisZSensitivity = 0.2f;
    Mode mode = Mode::Centered;
  } settings;

  struct Controls {
    bool forward = false;
    bool backward = false;
    bool left = false;
    bool right = false;
    bool up = false;
    bool down = false;

    double axisDeltaX = 0.0f;
    double axisDeltaY = 0.0f;
    double axisDeltaZ = 0.0f;
  } controls;

  void update(float deltaTime) override;

  auto phi() const { return currentPhi; }

  auto psi() const { return currentPsi; }

  auto longitude() const { return currentLongitude; }
  auto latitude() const { return currentLatitude; }
  auto distance() const { return currentDistance * currentDistance; }

  void setTarget(Planet const &planet) { m_planet = &planet; }

private:
  void updateCenteredMode(float deltaTime);
  void updateSurfaceMode(float deltaTime);
  void resetControls();
  float targetPsi = 0.0f;
  float targetPhi = 0.0f;
  float currentPsi = 0.0f;
  float currentPhi = 0.0f;

  float targetLatitude = 0.0f;
  float currentLatitude = 0.0f;
  float targetLongitude = 0.0f;
  float currentLongitude = 0.0f;

  float targetDistance = 5.0f;
  float currentDistance = 5.0f;

  Planet const *m_planet = nullptr;
};

class PlanetCameraSettings : public GUIWindow {
public:
  PlanetCameraSettings(GUIFrontEnd &gui, PlanetCamera &camera);

protected:
  void onGui() override;

private:
  int currentMode = 0;
  float m_farPlane = 5.0f;
  std::reference_wrapper<PlanetCamera> m_camera;
};
} // namespace TestApp
#endif // TESTAPP_PLANETCAMERA_HPP
