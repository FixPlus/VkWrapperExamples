#include "PlanetCamera.hpp"

namespace TestApp {

void PlanetCamera::update(float deltaTime) {
  if (!m_planet)
    return;
  auto &planet = *m_planet;
  targetDistance += controls.axisDeltaZ * settings.axisZSensitivity;
  targetDistance = std::clamp(targetDistance, 1.0f, 10.0f);
  float deltaDist = targetDistance - currentDistance;
  float plusDist = deltaDist * deltaTime * 100.0f / settings.rotateInertia;
  currentDistance = std::clamp(currentDistance + plusDist, 1.0f, 10.0f);
  float distance = currentDistance * (planet.properties.planetRadius +
                                      planet.properties.atmosphereRadius);

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
  auto circles = std::floor(currentPhi / 360.0f);
  currentPhi -= circles * 360.0f;
  targetPhi -= circles * 360.0f;

  glm::vec3 center = planet.properties.position;

  auto radPsi = glm::radians(currentPsi);
  auto radPhi = glm::radians(currentPhi);

  glm::vec3 direction =
      glm::vec3{std::cos(radPsi) * std::sin(radPhi), std::sin(radPsi),
                std::cos(radPsi) * std::cos(radPhi)};
  auto viewPos = center + direction * distance;
  lookAt(viewPos, center);
  controls.axisDeltaX = 0.0f;
  controls.axisDeltaY = 0.0f;
  controls.axisDeltaZ = 0.0f;
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
  ImGui::Text("(%.2f,%.2f)", camera.phi(), camera.psi());
  ImGui::SliderFloat("rotate inertia", &camera.settings.rotateInertia, 0.0f,
                     100.0f);
  if (ImGui::SliderFloat("far plane(log)", &m_farPlane, 3.0f, 10.0f)) {
    camera.setFarPlane(glm::pow(10.0f, m_farPlane));
  }
}

} // namespace TestApp