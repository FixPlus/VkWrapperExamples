#ifndef TESTAPP_CUBEGEOMETRY_H
#define TESTAPP_CUBEGEOMETRY_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <optional>
#include <vkw/VertexBuffer.hpp>
#include <vkw/Common.hpp>
#include <map>
#include <vkw/Pipeline.hpp>

namespace TestApp {


    class Cube;

    class CubePool {
    public:

        CubePool(vkw::Device &device, uint32_t maxCubes);

        CubePool(CubePool const &another) = delete;

        CubePool(CubePool &&another) noexcept;

        CubePool& operator=(CubePool const& another) = delete;
        CubePool& operator=(CubePool&& another) = delete;


        struct PerVertex : vkw::AttributeBase<vkw::VertexAttributeType::VEC3F,
                vkw::VertexAttributeType::VEC3F, vkw::VertexAttributeType::VEC3F, vkw::VertexAttributeType::VEC2F> {
            glm::vec3 pos;
            glm::vec3 color = {1.0f, 1.0f, 1.0f};
            glm::vec3 normal;
            glm::vec2 uv;
        };

        struct PerInstance : public vkw::AttributeBase<vkw::VertexAttributeType::VEC4F,
                vkw::VertexAttributeType::VEC4F,
                vkw::VertexAttributeType::VEC4F,
                vkw::VertexAttributeType::VEC4F> {
            glm::mat4 model;
        };

        void bindGeometry(vkw::CommandBuffer &commandBuffer) const;

        void drawGeometry(vkw::CommandBuffer &commandBuffer) const;

        static vkw::VertexInputStateCreateInfoBase const& geometryInputState(){
            return m_vsci;
        }

        virtual ~CubePool() = default;

    private:

        friend class Cube;

        static std::vector<PerVertex> m_makeCube() {
            std::vector<PerVertex> ret{};
            // top
            ret.push_back({.pos = {0.5f, 0.5f, 0.5f}, .normal = {0.0f, 0.0f, 1.0f}, .uv = {0.0f, 0.0f}});
            ret.push_back({.pos = {-0.5f, -0.5f, 0.5f}, .normal = {0.0f, 0.0f, 1.0f}, .uv = {1.0f, 1.0f}});
            ret.push_back({.pos = {0.5f, -0.5f, 0.5f}, .normal = {0.0f, 0.0f, 1.0f}, .uv = {1.0f, 0.0f}});


            ret.push_back({.pos = {0.5f, 0.5f, 0.5f}, .normal = {0.0f, 0.0f, 1.0f}, .uv = {0.0f, 0.0f}});
            ret.push_back({.pos = {-0.5f, 0.5f, 0.5f}, .normal = {0.0f, 0.0f, 1.0f}, .uv = {0.0f, 1.0f}});
            ret.push_back({.pos = {-0.5f, -0.5f, 0.5f}, .normal = {0.0f, 0.0f, 1.0f}, .uv = {1.0f, 1.0f}});


            // bottom

            ret.push_back({.pos = {0.5f, 0.5f, -0.5f}, .normal = {0.0f, 0.0f, -1.0f}, .uv = {0.0f, 0.0f}});
            ret.push_back({.pos = {0.5f, -0.5f, -0.5f}, .normal = {0.0f, 0.0f, -1.0f}, .uv = {1.0f, 0.0f}});
            ret.push_back({.pos = {-0.5f, -0.5f, -0.5f}, .normal = {0.0f, 0.0f, -1.0f}, .uv = {1.0f, 1.0f}});


            ret.push_back({.pos = {0.5f, 0.5f, -0.5f}, .normal = {0.0f, 0.0f, -1.0f}, .uv = {0.0f, 0.0f}});
            ret.push_back({.pos = {-0.5f, -0.5f, -0.5f}, .normal = {0.0f, 0.0f, -1.0f}, .uv = {1.0f, 1.0f}});
            ret.push_back({.pos = {-0.5f, 0.5f, -0.5f}, .normal = {0.0f, 0.0f, -1.0f}, .uv = {0.0f, 1.0f}});



            // east

            ret.push_back({.pos = {0.5f, 0.5f, 0.5f}, .normal = {0.0f, 1.0f, 0.0f}, .uv = {0.0f, 0.0f}});
            ret.push_back({.pos = {0.5f, 0.5f, -0.5f}, .normal = {0.0f, 1.0f, 0.0f}, .uv = {1.0f, 0.0f}});
            ret.push_back({.pos = {-0.5f, 0.5f, -0.5f}, .normal = {0.0f, 1.0f, 0.0f}, .uv = {1.0f, 1.0f}});

            ret.push_back({.pos = {0.5f, 0.5f, 0.5f}, .normal = {0.0f, 1.0f, 0.0f}, .uv = {0.0f, 0.0f}});
            ret.push_back({.pos = {-0.5f, 0.5f, -0.5f}, .normal = {0.0f, 1.0f, 0.0f}, .uv = {1.0f, 1.0f}});
            ret.push_back({.pos = {-0.5f, 0.5f, 0.5f}, .normal = {0.0f, 1.0f, 0.0f}, .uv = {0.0f, 1.0f}});


            // west

            ret.push_back({.pos = {0.5f, -0.5f, 0.5f}, .normal = {0.0f, -1.0f, 0.0f}, .uv = {0.0f, 0.0f}});
            ret.push_back({.pos = {-0.5f, -0.5f, -0.5f}, .normal = {0.0f, -1.0f, 0.0f}, .uv = {1.0f, 1.0f}});
            ret.push_back({.pos = {0.5f, -0.5f, -0.5f}, .normal = {0.0f, -1.0f, 0.0f}, .uv = {1.0f, 0.0f}});


            ret.push_back({.pos = {0.5f, -0.5f, 0.5f}, .normal = {0.0f, -1.0f, 0.0f}, .uv = {0.0f, 0.0f}});
            ret.push_back({.pos = {-0.5f, -0.5f, 0.5f}, .normal = {0.0f, -1.0f, 0.0f}, .uv = {0.0f, 1.0f}});
            ret.push_back({.pos = {-0.5f, -0.5f, -0.5f}, .normal = {0.0f, -1.0f, 0.0f}, .uv = {1.0f, 1.0f}});



            // north

            ret.push_back({.pos = {0.5f, 0.5f, 0.5f}, .normal = {1.0f, 0.0f, 0.0f}, .uv = {0.0f, 0.0f}});
            ret.push_back({.pos = {0.5f, -0.5f, 0.5f}, .normal = {1.0f, 0.0f, 0.0f}, .uv = {1.0f, 0.0f}});
            ret.push_back({.pos = {0.5f, -0.5f, -0.5f}, .normal = {1.0f, 0.0f, 0.0f}, .uv = {1.0f, 1.0f}});


            ret.push_back({.pos = {0.5f, 0.5f, 0.5f}, .normal = {1.0f, 0.0f, 0.0f}, .uv = {0.0f, 0.0f}});
            ret.push_back({.pos = {0.5f, -0.5f, -0.5f}, .normal = {1.0f, 0.0f, 0.0f}, .uv = {1.0f, 1.0f}});
            ret.push_back({.pos = {0.5f, 0.5f, -0.5f}, .normal = {1.0f, 0.0f, 0.0f}, .uv = {0.0f, 1.0f}});



            // south

            ret.push_back({.pos = {-0.5f, 0.5f, 0.5f}, .normal = {-1.0f, 0.0f, 0.0f}, .uv = {0.0f, 0.0f}});
            ret.push_back({.pos = {-0.5f, -0.5f, -0.5f}, .normal = {-1.0f, 0.0f, 0.0f}, .uv = {1.0f, 1.0f}});
            ret.push_back({.pos = {-0.5f, -0.5f, 0.5f}, .normal = {-1.0f, 0.0f, 0.0f}, .uv = {1.0f, 0.0f}});


            ret.push_back({.pos = {-0.5f, 0.5f, 0.5f}, .normal = {-1.0f, 0.0f, 0.0f}, .uv = {0.0f, 0.0f}});
            ret.push_back({.pos = {-0.5f, 0.5f, -0.5f}, .normal = {-1.0f, 0.0f, 0.0f}, .uv = {0.0f, 1.0f}});
            ret.push_back({.pos = {-0.5f, -0.5f, -0.5f}, .normal = {-1.0f, 0.0f, 0.0f}, .uv = {1.0f, 1.0f}});


            return ret;
        }

        static const vkw::VertexInputStateCreateInfo<vkw::per_vertex<PerVertex, 0>, vkw::per_instance<PerInstance, 1>> m_vsci;

        vkw::VertexBuffer<PerVertex> m_vertices;
        vkw::VertexBuffer<PerInstance> m_instances;
        PerInstance *m_instance_mapped = nullptr;

        std::map<uint32_t, Cube *> m_cubes{};
        uint32_t m_cubeCount = 0;
    };


    class Cube {
    public:

        Cube(CubePool &pool, glm::vec3 position, glm::vec3 scale, glm::vec3 rotation) :
                m_translate(position), m_scale(scale), m_rotate(rotation), m_parent(&pool),
                m_id(pool.m_cubeCount++) {
            m_parent->m_cubes.emplace(m_id, this);
            m_velocity = {std::rand() % 10 - 5.0f, std::rand() % 10 - 5.0f, std::rand() % 10 - 5.0f};
            m_rotation = {std::rand() % 40 - 20.0f, std::rand() % 40 - 20.0f, std::rand() % 40 - 20.0f};
        };


        Cube(Cube &&another) noexcept: m_translate(another.m_translate), m_scale(another.m_scale),
                                       m_rotate(another.m_rotate),
                                       m_parent(another.m_parent),
                                       m_id(another.m_id), m_velocity(another.m_velocity),
                                       m_rotation(another.m_rotation) {
            m_parent->m_cubes.at(m_id) = this;
            another.m_id = M_MOVED_OUT;
        };


        virtual ~Cube() {
            if (m_id == M_MOVED_OUT)
                return;

            m_parent->m_cubeCount--;
            auto &cubes = m_parent->m_cubes;

            if (cubes.at(m_parent->m_cubeCount) != this) {
                auto *swapCube = cubes.at(m_parent->m_cubeCount);
                swapCube->m_id = m_id;
                cubes.erase(m_parent->m_cubeCount);
                cubes.at(m_id) = swapCube;
            }
        }

        void update(double deltaTime) {

            m_translate += m_velocity * (float) deltaTime;
            m_rotate += m_rotation * (float) deltaTime;


            model.model = glm::translate(glm::mat4(1.0f), m_translate);
            model.model = glm::rotate(model.model, glm::radians(m_rotate.x), glm::vec3{1.0f, 0.0f, 0.0f});
            model.model = glm::rotate(model.model, glm::radians(m_rotate.y), glm::vec3{0.0f, 1.0f, 0.0f});
            model.model = glm::rotate(model.model, glm::radians(m_rotate.z), glm::vec3{0.0f, 0.0f, 1.0f});
            model.model = glm::scale(model.model, m_scale);

            m_parent->m_instance_mapped[m_id] = model;

        }

    private:

        friend class CubePool;

        CubePool::PerInstance model;

        static constexpr const uint32_t M_MOVED_OUT = std::numeric_limits<uint32_t>::max();

        uint32_t m_id;

        glm::vec3 m_translate;
        glm::vec3 m_rotate;
        glm::vec3 m_scale;

        glm::vec3 m_rotation;
        glm::vec3 m_velocity;

        CubePool* m_parent;

    };


}


#endif //TESTAPP_CUBEGEOMETRY_H