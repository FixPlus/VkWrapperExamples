#include "PlanetCamera.hpp"

namespace TestApp {

void PlanetCamera::update(float deltaTime) {
  if (!m_planet)
    return;
  switch (settings.mode) {
  case Mode::Centered:
    updateCenteredMode(deltaTime);
    break;
  case Mode::Surface:
    updateSurfaceMode(deltaTime);
    break;
  }
  resetControls();
}

void PlanetCamera::updateCenteredMode(float deltaTime) {
  auto &planet = *m_planet;
  targetDistance += controls.axisDeltaZ * settings.axisZSensitivity;
  targetDistance = std::clamp(targetDistance, 0.2f, 100.0f);
  float deltaDist = targetDistance - currentDistance;
  float plusDist = deltaDist * deltaTime * 100.0f / settings.rotateInertia;
  currentDistance = std::clamp(currentDistance + plusDist, 0.2f, 100.0f);
  float distance =
      (planet.properties.planetRadius +
       currentDistance * currentDistance * planet.properties.atmosphereRadius);

  targetLongitude += controls.axisDeltaX * settings.axisXSensitivity;
  targetLatitude += controls.axisDeltaY * settings.axisYSensitivity;

  targetLatitude = std::clamp(targetLatitude, -89.0f, 89.0f);

  float deltaLat = targetLatitude - currentLatitude;
  float deltaLon = targetLongitude - currentLongitude;

  float rotLat = deltaLat * deltaTime * 100.0f / settings.rotateInertia;
  float rotLon = deltaLon * deltaTime * 100.0f / settings.rotateInertia;
  currentLatitude += rotLat;
  currentLatitude = std::clamp(currentLatitude, -89.0f, 89.0f);
  currentLongitude += rotLon;
  auto circles = std::floor(currentLongitude / 360.0f);
  currentLongitude -= circles * 360.0f;
  targetLongitude -= circles * 360.0f;

  glm::vec3 center = planet.properties.position;

  auto radLat = glm::radians(currentLatitude);
  auto radLon = glm::radians(currentLongitude);

  glm::vec3 direction =
      glm::vec3{std::cos(radLat) * std::sin(radLon), std::sin(radLat),
                std::cos(radLat) * std::cos(radLon)};
  auto viewPos = center + direction * distance;
  lookAt(viewPos, center);
}

void PlanetCamera::updateSurfaceMode(float deltaTime) {
  auto &planet = *m_planet;
  targetDistance += controls.axisDeltaZ * settings.axisZSensitivity;
  targetDistance = std::clamp(targetDistance, 0.2f, 100.0f);
  float deltaDist = targetDistance - currentDistance;
  float plusDist = deltaDist * deltaTime * 100.0f / settings.rotateInertia;
  currentDistance = std::clamp(currentDistance + plusDist, 0.2f, 100.0f);
  float distance =
      (planet.properties.planetRadius +
       currentDistance * currentDistance * planet.properties.atmosphereRadius);

  glm::vec2 dir = glm::vec2(glm::cos(glm::radians(currentPhi)),
                            glm::sin(glm::radians(currentPhi)));
  glm::vec2 normalDir = glm::vec2(dir.y, -dir.x);
  if (controls.forward) {
    targetLongitude -= dir.y * settings.axisXSensitivity * deltaTime * 10.0f;
    targetLatitude -= dir.x * settings.axisYSensitivity * deltaTime * 10.0f;
  }
  if (controls.backward) {
    targetLongitude += dir.y * settings.axisXSensitivity * deltaTime * 10.0f;
    targetLatitude += dir.x * settings.axisYSensitivity * deltaTime * 10.0f;
  }
  if (controls.left) {
    targetLongitude -=
        normalDir.y * settings.axisXSensitivity * deltaTime * 10.0f;
    targetLatitude -=
        normalDir.x * settings.axisYSensitivity * deltaTime * 10.0f;
  }

  if (controls.right) {
    targetLongitude +=
        normalDir.y * settings.axisXSensitivity * deltaTime * 10.0f;
    targetLatitude +=
        normalDir.x * settings.axisYSensitivity * deltaTime * 10.0f;
  }

  targetLatitude = std::clamp(targetLatitude, -89.0f, 89.0f);

  float deltaLat = targetLatitude - currentLatitude;
  float deltaLon = targetLongitude - currentLongitude;

  float rotLat = deltaLat * deltaTime * 100.0f / settings.rotateInertia;
  float rotLon = deltaLon * deltaTime * 100.0f / settings.rotateInertia;
  currentLatitude += rotLat;
  currentLatitude = std::clamp(currentLatitude, -89.0f, 89.0f);
  currentLongitude += rotLon;
  auto circles = std::floor(currentLongitude / 360.0f);
  currentLongitude -= circles * 360.0f;
  targetLongitude -= circles * 360.0f;

  targetPhi += controls.axisDeltaX * settings.axisXSensitivity;
  targetPsi += controls.axisDeltaY * settings.axisYSensitivity;

  targetPsi = std::clamp(targetPsi, -89.0f, 89.0f);

  float deltaPsi = targetPsi - currentPsi;
  float deltaPhi = targetPhi - currentPhi;

  float rotPsi = deltaPsi * deltaTime * 100.0f / settings.rotateInertia;
  float rotPhi = deltaPhi * deltaTime * 100.0f / settings.rotateInertia;
  currentPsi += rotPsi;
  currentPsi = std::clamp(currentPsi, -89.0f, 89.0f);
  currentPhi += rotPhi;
  circles = std::floor(currentPhi / 360.0f);
  currentPhi -= circles * 360.0f;
  targetPhi -= circles * 360.0f;

  glm::vec3 center = planet.properties.position;

  auto radLat = glm::radians(currentLatitude);
  auto radLon = glm::radians(currentLongitude);

  glm::vec3 direction =
      glm::vec3{std::cos(radLat) * std::sin(radLon), std::sin(radLat),
                std::cos(radLat) * std::cos(radLon)};
  auto N1 = glm::cross(glm::normalize(direction), glm::vec3(0.0f, 1.0f, 0.0f));
  auto N2 = glm::cross(glm::normalize(direction), N1);

  auto viewFrom = center + direction * distance;
  auto viewDir =
      glm::cos(glm::radians(currentPsi)) * (N1 * dir.y + N2 * dir.x) +
      glm::sin(glm::radians(currentPsi)) * glm::normalize(direction);
  auto viewTarget = viewFrom + viewDir;
  lookAt(viewFrom, viewTarget, direction);
}

void PlanetCamera::resetControls() {
  controls.axisDeltaX = 0.0f;
  controls.axisDeltaY = 0.0f;
  controls.axisDeltaZ = 0.0f;
}
namespace {

constexpr std::array<const char *, 2> modeStrings = {"Centered", "Surface"};
}
PlanetCameraSettings::PlanetCameraSettings(GUIFrontEnd &gui,
                                           PlanetCamera &camera)
    : GUIWindow(gui,
                WindowSettings{.title = "Camera settings", .autoSize = true}),
      m_camera(camera) {
  camera.setFarPlane(glm::pow(10.0f, m_farPlane));
}

void PlanetCameraSettings::onGui() {
  auto &camera = m_camera.get();
  auto pos = camera.position();
  ImGui::Text("X: %.2f, Y: %.2f, Z: %.2f,", pos.x, pos.y, pos.z);
  ImGui::Text("lon: %.2f, lat: %.2f", camera.longitude(), camera.latitude());
  ImGui::Text("(%.2f,%.2f)", camera.phi(), camera.psi());
  ImGui::Text("Alt: %.2f", camera.distance());
  ImGui::SliderFloat("rotate inertia", &camera.settings.rotateInertia, 0.0f,
                     100.0f);
  if (ImGui::Combo("Mode", &currentMode, modeStrings.data(),
                   modeStrings.size())) {
    camera.settings.mode = static_cast<PlanetCamera::Mode>(currentMode);
  }

  if (ImGui::SliderFloat("far plane(log)", &m_farPlane, 3.0f, 10.0f)) {
    camera.setFarPlane(glm::pow(10.0f, m_farPlane));
  }
}

} // namespace TestApp