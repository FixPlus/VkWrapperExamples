#ifndef TESTAPP_SPHEREMESH_HPP
#define TESTAPP_SPHEREMESH_HPP

#include "GUI.h"
#include "RenderEngine/Pipelines/Geometry.h"
#include "RenderEngine/RecordingState.h"
#include <glm/glm.hpp>
namespace TestApp {

class SphereMesh {
public:
  struct Vertex : public vkw::AttributeBase<vkw::VertexAttributeType::VEC3F,
                                            vkw::VertexAttributeType::VEC3F,
                                            vkw::VertexAttributeType::VEC2F> {
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec2 UV;
  };

  void remesh(unsigned subdivisions, bool inverseNormal);

  SphereMesh(vkw::Device &device, unsigned subdivisions, bool inverseNormal);

  auto indexCount() const { return m_indexBuffer.size(); }

  auto vertexCount() const { return m_vertexBuffer.size(); }

protected:
  auto &device() const { return m_device.get(); }
  auto &vertexBuffer() const { return m_vertexBuffer; }
  auto &indexBuffer() const { return m_indexBuffer; }

private:
  vkw::StrongReference<vkw::Device> m_device;
  vkw::VertexBuffer<Vertex> m_vertexBuffer;
  vkw::IndexBuffer<VK_INDEX_TYPE_UINT16> m_indexBuffer;
};

class SimpleSphereMesh : public SphereMesh,
                         public RenderEngine::GeometryLayout,
                         public RenderEngine::Geometry {
public:
  SimpleSphereMesh(vkw::Device &device, unsigned subdivisions,
                   bool inverseNormal);

  struct Transform {
    glm::vec3 translate = glm::vec3(0.0f);
    glm::vec3 scale = glm::vec3(1.0f);
    glm::vec3 rotate;
  } transform;

  void update();

  void draw(RenderEngine::GraphicsRecordingState &recorder) const;

private:
  vkw::UniformBuffer<glm::mat4> m_transform;
};

class SphereMeshOptions : public GUIWindow {
public:
  SphereMeshOptions(GUIFrontEnd &gui, SphereMesh &mesh,
                    std::string_view title = "Mesh options");

protected:
  void onGui() override;

  template <typename T = SphereMesh>
  requires std::derived_from<T, SphereMesh> T &mesh() {
    return static_cast<T &>(m_mesh.get());
  }

private:
  bool m_inverseNormal = false;
  int m_subdivisions = 6;
  std::reference_wrapper<SphereMesh> m_mesh;
};

class SimpleSphereMeshOptions : public SphereMeshOptions {
public:
  SimpleSphereMeshOptions(GUIFrontEnd &gui, SimpleSphereMesh &mesh,
                          std::string_view title = "Simple Mesh options");

protected:
  void onGui() override;
};

} // namespace TestApp
#endif // TESTAPP_SPHEREMESH_HPP
