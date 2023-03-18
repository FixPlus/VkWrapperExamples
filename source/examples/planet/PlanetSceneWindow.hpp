#ifndef TESTAPP_PLANETSCENEWINDOW_HPP
#define TESTAPP_PLANETSCENEWINDOW_HPP
#include "GUI.h"
#include "PlanetCamera.hpp"
namespace TestApp {

class PlanetSceneWindow : public WindowIO {
public:
  PlanetSceneWindow(uint32_t width, uint32_t height, std::string const &title);

  auto &camera() const { return m_camera; }

  PlanetCameraSettings createCameraSettings(GUIFrontEnd &gui);

  void update();

  void setCameraTarget(Planet const &planet) { m_camera.setTarget(planet); }

protected:
  void onWindowResize(int width, int height) override {
    Window::onWindowResize(width, height);
    auto extents = framebufferExtents();
    m_camera.setRatio((float)extents.first / (float)extents.second);
  }

  void keyInput(int key, int scancode, int action, int mods) override {
    WindowIO::keyInput(key, scancode, action, mods);
    if (guiWantCaptureKeyboard())
      return;

    if (action == GLFW_PRESS)
      keyDown(key, mods);
    else if (action == GLFW_RELEASE)
      keyUp(key, mods);
    else
      keyRepeat(key, mods);
  };

  void mouseScroll(double xOffset, double yOffset) override {
    WindowIO::mouseScroll(xOffset, xOffset);
    if (guiWantCaptureMouse() || !cursorDisabled())
      return;
    m_camera.controls.axisDeltaZ += yOffset;
  };
  void mouseMove(double xpos, double ypos, double xdelta,
                 double ydelta) override {
    WindowIO::mouseMove(xpos, ypos, xdelta, ydelta);
    if (guiWantCaptureMouse() || !cursorDisabled())
      return;

    // if (cursorDisabled()) {
    m_camera.controls.axisDeltaX += xdelta;
    m_camera.controls.axisDeltaY += ydelta;
    //}
  };

private:
private:
  void keyDown(int key, int mods) {

    switch (key) {

    case GLFW_KEY_C:
      toggleCursor();
      break;
    default:;
    }
  }

  void keyUp(int key, int mods) {

    switch (key) { default:; }
  }

  void keyRepeat(int key, int mods) {}

  PlanetCamera m_camera;
};

} // namespace TestApp
#endif // TESTAPP_PLANETSCENEWINDOW_HPP
