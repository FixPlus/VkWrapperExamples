#ifndef TESTAPP_GLOBALLAYOUT_H
#define TESTAPP_GLOBALLAYOUT_H

#include <glm/glm.hpp>
#include <vkw/Device.hpp>
#include <vkw/DescriptorSet.hpp>
#include <vkw/DescriptorPool.hpp>
#include <common/Camera.h>


class GlobalLayout {
public:
    struct Uniform {
        glm::mat4 perspective;
        glm::mat4 cameraSpace;
        glm::vec4 lightVec = glm::normalize(glm::vec4{-0.37, 0.37, -0.85, 0.0f});
        glm::vec4 skyColor = glm::vec4{158.0f, 146.0f, 144.0f, 255.0f} / 255.0f;
        glm::vec4 lightColor = glm::vec4{244.0f, 218.0f, 62.0f, 255.0f} / 255.0f;
    } ubo;


    GlobalLayout(vkw::Device &device, TestApp::Camera const &camera) :
            m_layout(device, {vkw::DescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER}}),
            m_pool(device, 1, {VkDescriptorPoolSize{.type=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount=1}}),
            m_set(m_pool, m_layout),
            m_uniform(device,
                      VmaAllocationCreateInfo{.usage=VMA_MEMORY_USAGE_CPU_TO_GPU, .requiredFlags=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT}),
            m_camera(camera) {
        m_set.write(0, m_uniform);
    }

    void update() {
        ubo.perspective = m_camera.get().projection();
        ubo.cameraSpace = m_camera.get().cameraSpace();
        auto *mapped = m_uniform.map();

        *mapped = ubo;

        m_uniform.flush();

        m_uniform.unmap();
    }

    vkw::DescriptorSet const &set() const {
        return m_set;
    }

    vkw::DescriptorSetLayout const &layout() const {
        return m_layout;
    }

    TestApp::Camera const &camera() const {
        return m_camera.get();
    }

private:

    vkw::DescriptorSetLayout m_layout;
    vkw::DescriptorPool m_pool;
    vkw::DescriptorSet m_set;
    std::reference_wrapper<TestApp::Camera const> m_camera;
    vkw::UniformBuffer<Uniform> m_uniform;


};


#endif //TESTAPP_GLOBALLAYOUT_H
