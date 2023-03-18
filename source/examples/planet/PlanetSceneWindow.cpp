#include "PlanetSceneWindow.hpp"

namespace TestApp {

PlanetSceneWindow::PlanetSceneWindow(uint32_t width, uint32_t height,
                                     const std::string &title)
    : Window(width, height, title) {}
void PlanetSceneWindow::update() {
  auto deltaTime = clock().frameTime();

  m_camera.update(deltaTime);
}
PlanetCameraSettings PlanetSceneWindow::createCameraSettings(GUIFrontEnd &gui) {
  return PlanetCameraSettings(gui, m_camera);
}

} // namespace TestApp