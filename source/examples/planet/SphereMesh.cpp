#include "SphereMesh.hpp"
#include "RenderEngine/Window/Boxer.h"
#include <array>
#include <glm/gtx/transform.hpp>
#include <vkw/CommandPool.hpp>
#include <vkw/Queue.hpp>
#include <vkw/StagingBuffer.hpp>

namespace TestApp {
namespace {
glm::vec2 getUV(glm::vec3 dir) {
  auto angle = acos(std::abs(dir.x) + std::abs(dir.z) < 0.001f
                        ? 0.0f
                        : glm::normalize(glm::vec2(dir.x, dir.z)).x);

  if (dir.z < 0.0f)
    angle = 2.0f * glm::pi<float>() - angle;

  return {1.0f - angle / (2.0f * glm::pi<float>()),
          acos(dir.y) / glm::pi<float>()};
}

glm::vec3 getTangent(glm::vec3 normal) {
  constexpr auto upDir = glm::vec3{0.0, 1.0f, 0.0f};
  auto cross = glm::cross(normal, upDir);
  if (glm::length(cross) < 0.0001f) {
    return glm::vec3{1.0, 0.0f, 0.0f};
  } else {
    return -glm::normalize(glm::cross(normal, upDir));
  }
}

std::pair<std::vector<SphereMesh::Vertex>, std::vector<unsigned short>>
createIcosahedron(bool inverseNormal) {
  std::vector<SphereMesh::Vertex> vertices;
  std::vector<unsigned short> indices;

  SphereMesh::Vertex vertex;
  auto pi6 = glm::pi<float>() / 6.0f;

  auto upDir = glm::vec3{0.0, 1.0f, 0.0f};
  vertex.Position = glm::vec3{cos(pi6), sin(pi6), 0.0f};
  vertex.Normal = !inverseNormal ? vertex.Position : -vertex.Position;
  vertex.Tangent = getTangent(vertex.Normal);
  vertex.UV = getUV(vertex.Position);

  vertices.emplace_back(vertex);

  vertex.Position = glm::vec3{-cos(pi6), sin(pi6), 0.0f};
  vertex.Normal = !inverseNormal ? vertex.Position : -vertex.Position;
  vertex.Tangent = getTangent(vertex.Normal);
  vertex.UV = getUV(vertex.Position);

  vertices.emplace_back(vertex);

  vertex.Position = glm::vec3{-cos(pi6), -sin(pi6), 0.0f};
  vertex.Normal = !inverseNormal ? vertex.Position : -vertex.Position;
  vertex.Tangent = getTangent(vertex.Normal);
  vertex.UV = getUV(vertex.Position);

  vertices.emplace_back(vertex);

  vertex.Position = glm::vec3{cos(pi6), -sin(pi6), 0.0f};
  vertex.Normal = !inverseNormal ? vertex.Position : -vertex.Position;
  vertex.Tangent = getTangent(vertex.Normal);
  vertex.UV = getUV(vertex.Position);

  vertices.emplace_back(vertex);

  vertices.resize(12);
  std::transform(vertices.begin(), std::next(vertices.begin(), 4),
                 std::next(vertices.begin(), 4), [&upDir](auto &vertex) {
                   SphereMesh::Vertex newVertex = vertex;
                   std::swap(newVertex.Position.x, newVertex.Position.z);
                   std::swap(newVertex.Normal.x, newVertex.Normal.z);
                   std::swap(newVertex.Position.x, newVertex.Position.y);
                   std::swap(newVertex.Normal.x, newVertex.Normal.y);
                   newVertex.UV = getUV(newVertex.Position);
                   newVertex.Tangent = getTangent(vertex.Normal);
                   return newVertex;
                 });

  std::transform(vertices.begin(), std::next(vertices.begin(), 4),
                 std::next(vertices.begin(), 8), [&upDir](auto &vertex) {
                   SphereMesh::Vertex newVertex = vertex;
                   std::swap(newVertex.Position.y, newVertex.Position.z);
                   std::swap(newVertex.Normal.y, newVertex.Normal.z);
                   std::swap(newVertex.Position.y, newVertex.Position.x);
                   std::swap(newVertex.Normal.y, newVertex.Normal.x);
                   newVertex.UV = getUV(newVertex.Position);
                   newVertex.Tangent = getTangent(vertex.Normal);
                   return newVertex;
                 });

  indices.emplace_back(7);
  indices.emplace_back(4);
  indices.emplace_back(8);

  indices.emplace_back(4);
  indices.emplace_back(0);
  indices.emplace_back(8);

  indices.emplace_back(0);
  indices.emplace_back(11);
  indices.emplace_back(8);

  indices.emplace_back(0);
  indices.emplace_back(5);
  indices.emplace_back(11);

  indices.emplace_back(5);
  indices.emplace_back(6);
  indices.emplace_back(11);

  indices.emplace_back(6);
  indices.emplace_back(1);
  indices.emplace_back(11);

  indices.emplace_back(1);
  indices.emplace_back(8);
  indices.emplace_back(11);

  indices.emplace_back(1);
  indices.emplace_back(7);
  indices.emplace_back(8);

  indices.emplace_back(4);
  indices.emplace_back(7);
  indices.emplace_back(9);

  indices.emplace_back(3);
  indices.emplace_back(4);
  indices.emplace_back(9);

  indices.emplace_back(3);
  indices.emplace_back(9);
  indices.emplace_back(10);

  indices.emplace_back(5);
  indices.emplace_back(3);
  indices.emplace_back(10);

  indices.emplace_back(6);
  indices.emplace_back(5);
  indices.emplace_back(10);

  indices.emplace_back(2);
  indices.emplace_back(6);
  indices.emplace_back(10);

  indices.emplace_back(2);
  indices.emplace_back(10);
  indices.emplace_back(9);

  indices.emplace_back(7);
  indices.emplace_back(2);
  indices.emplace_back(9);

  indices.emplace_back(0);
  indices.emplace_back(4);
  indices.emplace_back(3);

  indices.emplace_back(0);
  indices.emplace_back(3);
  indices.emplace_back(5);

  indices.emplace_back(1);
  indices.emplace_back(6);
  indices.emplace_back(2);

  indices.emplace_back(1);
  indices.emplace_back(2);
  indices.emplace_back(7);

  return {vertices, indices};
}

bool uvDifferenceWrap(float a, float b) { return abs(a - b) > 0.5f; }
void fixUVArtifacts(std::vector<SphereMesh::Vertex> &vertices,
                    std::vector<unsigned short> &indices) {
  for (auto i = 0u; i < indices.size() / 3; ++i) {
    std::array<unsigned short, 3> polyIndices;
    std::copy(std::next(indices.begin(), i * 3),
              std::next(indices.begin(), i * 3 + 3), polyIndices.begin());

    auto &vertex1 = vertices.at(polyIndices[0]);
    auto &vertex2 = vertices.at(polyIndices[1]);
    auto &vertex3 = vertices.at(polyIndices[2]);

    if (!uvDifferenceWrap(vertex1.UV.x, vertex2.UV.x) &&
        !uvDifferenceWrap(vertex2.UV.x, vertex3.UV.x) &&
        !uvDifferenceWrap(vertex3.UV.x, vertex1.UV.x))
      continue;
    std::sort(polyIndices.begin(), polyIndices.end(),
              [&vertices](unsigned short index1, unsigned short index2) {
                return vertices.at(index1).UV.x < vertices.at(index2).UV.x;
              });

    if (uvDifferenceWrap(vertices.at(polyIndices[0]).UV.x,
                         vertices.at(polyIndices[1]).UV.x)) {
      SphereMesh::Vertex newVertex = vertices.at(polyIndices[1]);
      newVertex.UV.x -= 1.0f;
      if (vertices.size() > std::numeric_limits<unsigned short>::max()) {
        throw std::runtime_error(
            std::string("Mesh exceeded maximum index count: ")
                .append(std::to_string(vertices.size())));
      }
      polyIndices[1] = vertices.size();
      vertices.emplace_back(newVertex);
    }

    if (uvDifferenceWrap(vertices.at(polyIndices[0]).UV.x,
                         vertices.at(polyIndices[2]).UV.x)) {
      SphereMesh::Vertex newVertex = vertices.at(polyIndices[2]);
      newVertex.UV.x -= 1.0f;
      if (vertices.size() > std::numeric_limits<unsigned short>::max()) {
        throw std::runtime_error(
            std::string("Mesh exceeded maximum index count: ")
                .append(std::to_string(vertices.size())));
      }
      polyIndices[2] = vertices.size();
      vertices.emplace_back(newVertex);
    }
    // restore clockwiseness
    if (glm::dot(glm::cross(vertices.at(polyIndices[1]).Position -
                                vertices.at(polyIndices[0]).Position,
                            vertices.at(polyIndices[2]).Position -
                                vertices.at(polyIndices[1]).Position),
                 vertices.at(polyIndices[0]).Position) < 0.0f) {
      std::swap(polyIndices[0], polyIndices[1]);
    }

    std::copy(polyIndices.begin(), polyIndices.end(),
              std::next(indices.begin(), i * 3));
  }
}
void subdivide(std::vector<SphereMesh::Vertex> &vertices,
               std::vector<unsigned short> &indices) {
  class Subdivisions
      : public std::map<std::pair<unsigned short, unsigned short>, short> {
  public:
    explicit Subdivisions(std::vector<SphereMesh::Vertex> &vertices)
        : m_vertices(vertices) {}

    short getOrConstruct(unsigned short index1, unsigned short index2) {
      if (contains({index1, index2})) {
        return at({index1, index2});
      } else if (contains({index2, index1})) {
        return at({index2, index1});
      } else {
        if (m_vertices.get().size() >
            std::numeric_limits<unsigned short>::max()) {
          throw std::runtime_error(
              std::string("Mesh exceeded maximum index count: ")
                  .append(std::to_string(m_vertices.get().size())));
        }
        auto v1 = m_vertices.get().at(index1);
        auto v2 = m_vertices.get().at(index2);
        SphereMesh::Vertex sub = v1;
        sub.Position = glm::normalize(v1.Position + v2.Position);
        sub.Normal = glm::normalize(v1.Normal + v2.Normal);
        sub.UV = getUV(sub.Position);
        sub.Tangent = getTangent(sub.Normal);
        short index = m_vertices.get().size();
        m_vertices.get().emplace_back(sub);
        emplace(std::pair<unsigned short, unsigned short>{index1, index2},
                index);
        return index;
      }
    }

  private:
    std::reference_wrapper<std::vector<SphereMesh::Vertex>> m_vertices;
  } subdivisions{vertices};

  std::vector<unsigned short> newIndexes;

  for (auto i = 0u; i < indices.size() / 3; ++i) {
    auto index1 = indices[i * 3];
    auto index2 = indices[i * 3 + 1];
    auto index3 = indices[i * 3 + 2];
    unsigned short subdiv1Index, subdiv2Index, subdiv3Index;
    subdiv1Index = subdivisions.getOrConstruct(index1, index2);
    subdiv2Index = subdivisions.getOrConstruct(index2, index3);
    subdiv3Index = subdivisions.getOrConstruct(index3, index1);

    newIndexes.emplace_back(index1);
    newIndexes.emplace_back(subdiv1Index);
    newIndexes.emplace_back(subdiv3Index);

    newIndexes.emplace_back(subdiv1Index);
    newIndexes.emplace_back(index2);
    newIndexes.emplace_back(subdiv2Index);

    newIndexes.emplace_back(subdiv3Index);
    newIndexes.emplace_back(subdiv2Index);
    newIndexes.emplace_back(index3);

    newIndexes.emplace_back(subdiv1Index);
    newIndexes.emplace_back(subdiv2Index);
    newIndexes.emplace_back(subdiv3Index);
  }

  indices = newIndexes;
}

std::pair<std::vector<SphereMesh::Vertex>, std::vector<unsigned short>>
createIcosphere(unsigned subdivisions, bool inverseNormal) {

  auto &&[vertices, indices] = createIcosahedron(inverseNormal);

  for (auto i = 0; i < subdivisions; ++i)
    subdivide(vertices, indices);

  fixUVArtifacts(vertices, indices);
  return {vertices, indices};
}

} // namespace
SphereMesh::SphereMesh(vkw::Device &device, unsigned subdivisions,
                       bool inverseNormal)
    : m_vertexBuffer(device, 1,
                     VmaAllocationCreateInfo{
                         .usage = VMA_MEMORY_USAGE_GPU_ONLY,
                         .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT}),
      m_indexBuffer(device, 1,
                    VmaAllocationCreateInfo{
                        .usage = VMA_MEMORY_USAGE_GPU_ONLY,
                        .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT}),
      m_device(device) {
  remesh(subdivisions, inverseNormal);
}
void SimpleSphereMesh::draw(
    RenderEngine::GraphicsRecordingState &recorder) const {
  Geometry::bind(recorder);
  recorder.bindPipeline();
  recorder.commands().bindVertexBuffer(vertexBuffer(), 0, 0);
  recorder.commands().bindIndexBuffer(indexBuffer(), 0);
  recorder.commands().drawIndexed(indexBuffer().size(), 1, 0, 0, 0);
}
void SimpleSphereMesh::update() {
  auto transfromMat = glm::translate(transform.translate);
  transfromMat = transfromMat * glm::scale(transform.scale);
  m_transform.mapped().front() = transfromMat;
  m_transform.flush();
}

void SphereMesh::remesh(unsigned subdivisions, bool inverseNormal) {
  auto &&[vertices, indices] = createIcosphere(subdivisions, inverseNormal);

  auto &device = m_device.get();

  vkw::StagingBuffer<Vertex> vertStaging{device, vertices};
  vkw::StagingBuffer<unsigned short> indexStaging{device, indices};

  m_vertexBuffer = vkw::VertexBuffer<Vertex>{
      device, vertices.size(),
      VmaAllocationCreateInfo{.usage = VMA_MEMORY_USAGE_GPU_ONLY,
                              .requiredFlags =
                                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT},
      VK_BUFFER_USAGE_TRANSFER_DST_BIT};

  m_indexBuffer = vkw::IndexBuffer<VK_INDEX_TYPE_UINT16>{
      device, indices.size(),
      VmaAllocationCreateInfo{.usage = VMA_MEMORY_USAGE_GPU_ONLY,
                              .requiredFlags =
                                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT},
      VK_BUFFER_USAGE_TRANSFER_DST_BIT};
  auto &queue = device.anyTransferQueue();

  auto commandPool = vkw::CommandPool(
      device, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queue.family().index());

  auto transferBuffer = vkw::PrimaryCommandBuffer(commandPool);

  transferBuffer.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  VkBufferCopy verticesCopy{};
  verticesCopy.size = vertStaging.bufferSize();

  transferBuffer.copyBufferToBuffer(vertStaging, m_vertexBuffer,
                                    {&verticesCopy, 1});

  VkBufferCopy indicesCopy{};
  indicesCopy.size = indexStaging.bufferSize();

  transferBuffer.copyBufferToBuffer(indexStaging, m_indexBuffer,
                                    {&indicesCopy, 1});

  transferBuffer.end();

  auto submit = vkw::SubmitInfo(transferBuffer);

  queue.submit(submit);

  queue.waitIdle();
}

SphereMeshOptions::SphereMeshOptions(GUIFrontEnd &gui, SphereMesh &mesh,
                                     std::string_view title)
    : GUIWindow(gui, WindowSettings{.title = std::string(title)}),
      m_mesh(mesh) {
  mesh.remesh(m_subdivisions, m_inverseNormal);
}

void SphereMeshOptions::onGui() {
  auto oldSubdivs = m_subdivisions;
  ImGui::Text("vtx: %llu; idx: %llu", mesh().vertexCount(),
              mesh().indexCount());
  if (ImGui::SliderInt("subdivisions", &m_subdivisions, 0, 6)) {
    try {
      mesh().remesh(m_subdivisions, m_inverseNormal);
    } catch (std::runtime_error &e) {
      RenderEngine::Boxer::show(e.what(), "Index range error",
                                RenderEngine::Boxer::Style::Error);
      m_subdivisions = oldSubdivs;
    }
  }
  if (ImGui::Checkbox("inverse normal", &m_inverseNormal))
    mesh().remesh(m_subdivisions, m_inverseNormal);
}

SimpleSphereMesh::SimpleSphereMesh(vkw::Device &device, RenderEngine::ShaderLoaderInterface& shaderLoader,
                                   unsigned int subdivisions,
                                   bool inverseNormal)
    : SphereMesh(device, subdivisions, inverseNormal),
      RenderEngine::GeometryLayout(
          device, shaderLoader,
          RenderEngine::GeometryLayout::CreateInfo{
              .vertexInputState =
                  std::make_unique<vkw::VertexInputStateCreateInfo<
                      vkw::per_vertex<Vertex, 0>>>(),
              .substageDescription =
                  {.shaderSubstageName = "planet"},
              .maxGeometries = 1}),
      RenderEngine::Geometry(
          static_cast<RenderEngine::GeometryLayout &>(*this)),
      m_transform(device,
                  VmaAllocationCreateInfo{
                      .usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
                      .requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT}) {
  set().write(0, m_transform);
  m_transform.map();
}
SimpleSphereMeshOptions::SimpleSphereMeshOptions(GUIFrontEnd &gui,
                                                 SimpleSphereMesh &mesh,
                                                 std::string_view title)
    : SphereMeshOptions(gui, mesh, title) {}
void SimpleSphereMeshOptions::onGui() {
  SphereMeshOptions::onGui();
  bool needUpdate = false;
  auto &meshRef = mesh<SimpleSphereMesh>();
  if (ImGui::SliderFloat("scale", &meshRef.transform.scale.x, 0.5f, 10.0f)) {
    meshRef.transform.scale.y = meshRef.transform.scale.z =
        meshRef.transform.scale.x;
    needUpdate = true;
  }
  if (ImGui::SliderFloat3("translate", &meshRef.transform.translate.x, -10.0f,
                          10.0f)) {
    needUpdate = true;
  }
  if (needUpdate)
    meshRef.update();
}
} // namespace TestApp