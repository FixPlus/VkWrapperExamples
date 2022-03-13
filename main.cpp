#include <iostream>

#define VKW_EXTENSION_INITIALIZERS_MAP_DEFINITION 1

#include "vkw/SymbolTable.hpp"
#include "Instance.hpp"
#include "SceneProjector.h"
#include <chrono>
#include <thread>
#include <SwapChain.hpp>
#include <Buffer.hpp>
#include <Queue.hpp>
#include <Semaphore.hpp>
#include <Fence.hpp>
#include <Image.hpp>
#include <CommandPool.hpp>
#include <CommandBuffer.hpp>
#include <VertexBuffer.hpp>
#include <Pipeline.hpp>
#include <cassert>
#include <Shader.hpp>
#include <fstream>
#include <RenderPass.hpp>
#include <FrameBuffer.hpp>
#include <DescriptorPool.hpp>
#include <DescriptorSet.hpp>
#include <UniformBuffer.hpp>
#include <cmath>
#include <chrono>
#include <Sampler.hpp>
#include <Surface.hpp>
#include <thread>

#define STB_IMAGE_IMPLEMENTATION

#include "external/stb_image.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "SwapChainImpl.h"

struct GlobalUniform {
    glm::mat4 perspective = glm::perspective(60.0f, 16.0f / 9.0f, 0.01f, 1000.0f);
} myUniform;



template<typename T>
requires std::derived_from<T, vkw::ShaderBase>
T loadShader(vkw::Device &device, std::string const &filename) {
    std::ifstream input{filename, std::ios::binary};
    std::ifstream is(filename, std::ios::binary | std::ios::in | std::ios::ate);

    if (is.is_open()) {
        size_t size = is.tellg();
        is.seekg(0, std::ios::beg);
        std::vector<char> shaderCode;

        shaderCode.resize(size);

        is.read(shaderCode.data(), size);
        is.close();

        if (size == 0)
            throw std::runtime_error(
                    "Failed to read " + filename);

        return T{device, shaderCode.size(), reinterpret_cast<uint32_t *>(shaderCode.data())};
    } else {
        throw std::runtime_error("Failed to open file " + filename);
    }

    return T{device, 0, nullptr};
}

vkw::ColorImage2D loadTexture(const char *filename, vkw::Device &device) {
    int width, height, bits;
    auto *imageData = stbi_load(filename, &width, &height, &bits, 4);
    if (!imageData)
        throw std::runtime_error("Failed to load image: " + std::string(filename));

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    allocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    vkw::Buffer<unsigned char> stageBuffer{device, width * height * 4u, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, allocInfo};

    auto mappedBuffer = stageBuffer.map();
    memcpy(mappedBuffer, imageData, width * height * 4u);
    stageBuffer.flush();

    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    auto ret = vkw::ColorImage2D{device.getAllocator(), allocInfo, VK_FORMAT_R8G8B8A8_UNORM,
                                 static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1,
                                 VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT};

    VkImageMemoryBarrier transitLayout1{};
    transitLayout1.image = ret;
    transitLayout1.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    transitLayout1.pNext = nullptr;
    transitLayout1.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    transitLayout1.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    transitLayout1.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    transitLayout1.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    transitLayout1.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    transitLayout1.subresourceRange.baseArrayLayer = 0;
    transitLayout1.subresourceRange.baseMipLevel = 0;
    transitLayout1.subresourceRange.layerCount = 1;
    transitLayout1.subresourceRange.levelCount = 1;
    transitLayout1.dstAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
    transitLayout1.srcAccessMask = 0;

    VkImageMemoryBarrier transitLayout2{};
    transitLayout2.image = ret;
    transitLayout2.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    transitLayout2.pNext = nullptr;
    transitLayout2.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    transitLayout2.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    transitLayout2.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    transitLayout2.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    transitLayout2.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    transitLayout2.subresourceRange.baseArrayLayer = 0;
    transitLayout2.subresourceRange.baseMipLevel = 0;
    transitLayout2.subresourceRange.layerCount = 1;
    transitLayout2.subresourceRange.levelCount = 1;
    transitLayout2.dstAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
    transitLayout2.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;

    auto transferQueue = device.getTransferQueue();
    auto commandPool = vkw::CommandPool{device, 0, transferQueue->familyIndex()};
    auto transferCommand = vkw::PrimaryCommandBuffer{commandPool};
    transferCommand.begin(0);
    transferCommand.imageMemoryBarrier(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                       {transitLayout1});
    VkBufferImageCopy bufferCopy{};
    bufferCopy.imageExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1};
    bufferCopy.imageSubresource.mipLevel = 0;
    bufferCopy.imageSubresource.layerCount = 1;
    bufferCopy.imageSubresource.baseArrayLayer = 0;
    bufferCopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    transferCommand.copyBufferToImage(stageBuffer, ret, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, {bufferCopy});

    transferCommand.imageMemoryBarrier(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                       {transitLayout2});
    transferCommand.end();

    transferQueue->submit(transferCommand);
    transferQueue->waitIdle();


    stbi_image_free(imageData);

    return ret;
}


class Cube{
public:

    Cube(glm::vec3 position, glm::vec3 scale, glm::vec3 rotation):
    m_translate(position), m_scale(scale), m_rotate(rotation),
    m_id(Cube::m_cubeCount++){
        m_cubes.emplace(m_id, this);
        m_velocity = {std::rand() % 10 - 5.0f, std::rand() % 10 - 5.0f, std::rand() % 10 - 5.0f};
        m_rotation = {std::rand() % 40 - 20.0f, std::rand() % 40 - 20.0f, std::rand() % 40 - 20.0f};
    }

    Cube(Cube&& another) noexcept: m_translate(another.m_translate), m_scale(another.m_scale), m_rotate(another.m_rotate),
                                   m_CubeUniform(another.m_CubeUniform),
                                   m_id(another.m_id), m_velocity(another.m_velocity), m_rotation(another.m_rotation) {
        m_cubes.at(m_id) = this;
        another.m_id = M_MOVED_OUT;
    };

    template<typename T>
    static void writeGlobalUniform(vkw::UniformBuffer<T> const& ubo){
        m_descriptorSet->write(0, ubo);
    }

    ~Cube(){
        if(m_id == M_MOVED_OUT)
            return;

        m_cubeCount--;

        if(m_cubes.at(m_cubeCount) != this){
            auto* swapCube = m_cubes.at(m_cubeCount);
            swapCube->m_id = m_id;
            m_cubes.erase(m_cubeCount);
            m_cubes.at(m_id) = swapCube;
        }

    }

    void update(double deltaTime){

        m_translate += m_velocity * (float)deltaTime;
        m_rotate += m_rotation * (float)deltaTime;


        m_CubeUniform.model = glm::translate(glm::mat4(1.0f), m_translate);
        m_CubeUniform.model = glm::rotate(m_CubeUniform.model, glm::radians(m_rotate.x), glm::vec3{1.0f, 0.0f, 0.0f});
        m_CubeUniform.model = glm::rotate(m_CubeUniform.model, glm::radians(m_rotate.y), glm::vec3{0.0f, 1.0f, 0.0f});
        m_CubeUniform.model = glm::rotate(m_CubeUniform.model, glm::radians(m_rotate.z), glm::vec3{0.0f, 0.0f, 1.0f});
        m_CubeUniform.model = glm::scale(m_CubeUniform.model, m_scale);

        m_instance_mapped[m_id] = m_CubeUniform;

    }

    static void draw(vkw::CommandBuffer& commandBuffer){

        m_instances->flush();

        commandBuffer.bindGraphicsPipeline(m_texturePipeline.value());
        commandBuffer.bindVertexBuffer(m_vbuf.value(), 0, 0);
        commandBuffer.bindVertexBuffer(m_instances.value(), 1, 0);

        commandBuffer.bindDescriptorSets(m_texturePipelineLayout.value(), VK_PIPELINE_BIND_POINT_GRAPHICS, *m_descriptorSet, 0);
        commandBuffer.draw(m_vertices.size(), m_cubeCount);

    }

    static void Init(vkw::Device& device, vkw::RenderPass& renderPass, uint32_t subpass, uint32_t maxCubes, vkw::ColorImage2D& texture){
        m_device = &device;
        m_renderPass = &renderPass;
        m_vertexShader.emplace(loadShader<vkw::VertexShader>(device, "triangle.vert.spv"));
        m_textureShader.emplace(loadShader<vkw::FragmentShader>(device, "triangle.frag.spv"));

        std::vector<VkDescriptorPoolSize> poolSizes{};

        poolSizes.push_back({.type= VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount=1});
        poolSizes.push_back({.type= VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount=1});

        m_descriptorPool.emplace(vkw::DescriptorPool{device, maxCubes, poolSizes, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT});
        vkw::DescriptorSetLayoutBinding bindings[2] = {{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER}, {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER}};;

        m_textureLayout.emplace(vkw::DescriptorSetLayout{device, {bindings[0], bindings[1]}});

        m_texturePipelineLayout.emplace(device, m_textureLayout.value());

        vkw::GraphicsPipelineCreateInfo texturePipeCreateInfo{renderPass, subpass, m_texturePipelineLayout.value()};

        vkw::RasterizationStateCreateInfo rasterizerCI{VK_FALSE, VK_FALSE, VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE};

        texturePipeCreateInfo.addRasterizationState(rasterizerCI);

        texturePipeCreateInfo.addVertexShader(m_vertexShader.value());

        vkw::DepthTestStateCreateInfo depthTestCI{VK_COMPARE_OP_LESS, true};

        texturePipeCreateInfo.addDepthTestState(depthTestCI);

        texturePipeCreateInfo.addFragmentShader(m_textureShader.value());

        texturePipeCreateInfo.addVertexInputState(m_inputState);

        m_texturePipeline.emplace(device, texturePipeCreateInfo);

        m_vertices = m_makeCube();

        VkSamplerCreateInfo samplerCI{};

        samplerCI.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerCI.minLod = 0.0f;
        samplerCI.maxLod = 0.0f;
        samplerCI.magFilter = VK_FILTER_LINEAR;
        samplerCI.minFilter = VK_FILTER_LINEAR;

        m_sampler.emplace(*m_device, samplerCI);

        VmaAllocationCreateInfo createInfo{};
        createInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        createInfo.requiredFlags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;

        vkw::Buffer<CubeAttrs> stageBuf{*m_device, m_vertices.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, createInfo};
        createInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        createInfo.requiredFlags = 0;
        m_vbuf.emplace(*m_device, m_vertices.size(), createInfo, VK_BUFFER_USAGE_TRANSFER_DST_BIT);

        auto* mapped = stageBuf.map();
        memcpy(mapped, m_vertices.data(), m_vertices.size() * sizeof(CubeAttrs));
        stageBuf.flush();
        stageBuf.unmap();

        auto queue = device.getTransferQueue();
        auto commandPool = vkw::CommandPool{device, 0, queue->familyIndex()};
        auto transferCommand = vkw::PrimaryCommandBuffer{commandPool};
        transferCommand.begin(0);
        VkBufferCopy region{};
        region.size = stageBuf.size() * sizeof(CubeAttrs);
        region.dstOffset = 0;
        region.srcOffset = 0;

        transferCommand.copyBufferToBuffer(stageBuf, m_vbuf.value(), {region});

        transferCommand.end();
        queue->submit(transferCommand);
        queue->waitIdle();


        createInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        createInfo.requiredFlags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;

        m_instances.emplace(*m_device, maxCubes, createInfo);
        m_instance_mapped = m_instances->map();

        m_texture = &texture;

        static VkComponentMapping mapping;
        mapping.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        mapping.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        mapping.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        mapping.a = VK_COMPONENT_SWIZZLE_IDENTITY;

        m_view = &texture.getView<vkw::ColorImageView>(*m_device, texture.format(), mapping);

        m_descriptorSet = std::make_unique<vkw::DescriptorSet>(m_descriptorPool.value(), m_textureLayout.value());

        m_descriptorSet->write(1, *m_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, m_sampler.value());

        for(int i = 0; i < m_inputState.totalAttributes(); ++i){
            auto attrib = m_inputState.attribute(i);
            std::cout << "Attribute #" << attrib.location << ": binding=" << attrib.binding << ", offset=" << attrib.offset
            << ", format=" << attrib.format << std::endl;
        }

        std::cout << std::endl;

        for(int i = 0; i < m_inputState.totalBindings(); ++i){
            auto binding = m_inputState.binding(i);
            std::cout << "Binding #" <<  binding.binding << ": rate= " << (binding.inputRate == VK_VERTEX_INPUT_RATE_VERTEX ? "per_vertex" : "per_instance")
            << ", stride = " << binding.stride << std::endl;
        }
    }
    static void Terminate(){
        m_instances.reset();
        m_vbuf.reset();
        m_sampler.reset();
        m_texturePipeline.reset();
        m_texturePipelineLayout.reset();
        m_textureLayout.reset();
        m_descriptorSet.reset();
        m_descriptorPool.reset();
        m_vertexShader.reset();
        m_textureShader.reset();
    }

private:

    struct CubeAttrs : vkw::AttributeBase<vkw::VertexAttributeType::VEC3F,
            vkw::VertexAttributeType::VEC3F, vkw::VertexAttributeType::VEC3F, vkw::VertexAttributeType::VEC2F> {
        glm::vec3 pos;
        glm::vec3 color = {1.0f, 1.0f, 1.0f};
        glm::vec3 normal;
        glm::vec2 uv;
    };

    struct M_CubeUniform: public vkw::AttributeBase<vkw::VertexAttributeType::VEC4F,
            vkw::VertexAttributeType::VEC4F,
            vkw::VertexAttributeType::VEC4F,
            vkw::VertexAttributeType::VEC4F>{
        glm::mat4 model;
    } m_CubeUniform;

    uint32_t  m_id;

    glm::vec3 m_translate;
    glm::vec3 m_rotate;
    glm::vec3 m_scale;

    glm::vec3 m_rotation;
    glm::vec3 m_velocity;




    /** static members down there **/
    static constexpr const uint32_t M_MOVED_OUT = std::numeric_limits<uint32_t>::max();
    static std::unique_ptr<vkw::DescriptorSet> m_descriptorSet;
    static std::map<uint32_t, Cube*> m_cubes;
    static vkw::ColorImage2D* m_texture;
    static vkw::ColorImage2DView const* m_view;
    static std::vector<CubeAttrs> m_vertices;
    static std::optional<vkw::VertexBuffer<M_CubeUniform>> m_instances;
    static M_CubeUniform* m_instance_mapped;
    static uint32_t m_cubeCount;
    static vkw::Device* m_device;
    static vkw::RenderPass* m_renderPass;
    static std::optional<vkw::Sampler> m_sampler;
    static const vkw::VertexInputStateCreateInfo<vkw::per_vertex<CubeAttrs, 0>, vkw::per_instance<M_CubeUniform, 1>> m_inputState;
    static std::optional<vkw::VertexBuffer<CubeAttrs>> m_vbuf;
    static std::optional<vkw::DescriptorSetLayout> m_textureLayout;
    static std::optional<vkw::DescriptorPool> m_descriptorPool;
    static std::optional<vkw::PipelineLayout> m_texturePipelineLayout;
    static std::optional<vkw::GraphicsPipeline> m_texturePipeline;
    static std::optional<vkw::VertexShader> m_vertexShader;
    static std::optional<vkw::FragmentShader> m_textureShader;

    static std::vector<CubeAttrs> m_makeCube() {
        std::vector<CubeAttrs> ret{};
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

};

std::optional<vkw::VertexBuffer<Cube::M_CubeUniform>> Cube::m_instances{};
Cube::M_CubeUniform* Cube::m_instance_mapped{};
std::unique_ptr<vkw::DescriptorSet> Cube::m_descriptorSet{};
std::map<uint32_t, Cube*> Cube::m_cubes{};
vkw::ColorImage2D* Cube::m_texture{};
vkw::ColorImage2DView const* Cube::m_view{};
std::vector<Cube::CubeAttrs> Cube::m_vertices{};
uint32_t Cube::m_cubeCount = 0;
vkw::RenderPass* Cube::m_renderPass;
std::optional<vkw::Sampler> Cube::m_sampler;
vkw::Device* Cube::m_device;
std::optional<vkw::VertexBuffer<Cube::CubeAttrs>> Cube::m_vbuf{};
const vkw::VertexInputStateCreateInfo<vkw::per_vertex<Cube::CubeAttrs, 0>, vkw::per_instance<Cube::M_CubeUniform, 1>> Cube::m_inputState{};
std::optional<vkw::DescriptorSetLayout> Cube::m_textureLayout{};
std::optional<vkw::DescriptorPool> Cube::m_descriptorPool{};
std::optional<vkw::PipelineLayout> Cube::m_texturePipelineLayout{};
std::optional<vkw::GraphicsPipeline> Cube::m_texturePipeline{};
std::optional<vkw::VertexShader> Cube::m_vertexShader{};
std::optional<vkw::FragmentShader> Cube::m_textureShader{};


vkw::DepthStencilImage2D createDepthStencilImage(vkw::Device &device, uint32_t width, uint32_t height) {
    VmaAllocationCreateInfo createInfo{};
    createInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    auto depthMap = vkw::DepthStencilImage2D{device.getAllocator(), createInfo, VK_FORMAT_D24_UNORM_S8_UINT, width,
                                             height, 1, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT};

    VkImageMemoryBarrier transitLayout{};
    transitLayout.image = depthMap;
    transitLayout.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    transitLayout.pNext = nullptr;
    transitLayout.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    transitLayout.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    transitLayout.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    transitLayout.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    transitLayout.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    transitLayout.subresourceRange.baseArrayLayer = 0;
    transitLayout.subresourceRange.baseMipLevel = 0;
    transitLayout.subresourceRange.layerCount = 1;
    transitLayout.subresourceRange.levelCount = 1;
    transitLayout.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    transitLayout.srcAccessMask = 0;

    auto queue = device.getTransferQueue();
    auto commandPool = vkw::CommandPool{device, 0, queue->familyIndex()};
    auto transferCommand = vkw::PrimaryCommandBuffer{commandPool};

    transferCommand.begin(0);
    transferCommand.imageMemoryBarrier(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                       {transitLayout});
    transferCommand.end();

    queue->submit(transferCommand);
    queue->waitIdle();

    return depthMap;
}

int main() {


    // 1. Create Instance and Window


    TestApp::SceneProjector window{800, 600, "Hello, Boez!"};

    vkw::Library vulkanLib{};

    vkw::Instance renderInstance = TestApp::Window::vulkanInstance(vulkanLib, {}, true);

    std::for_each(renderInstance.extensions_begin(), renderInstance.extensions_end(),
                  [](vkw::Instance::extension_const_iterator::value_type const &ext) {
                      std::cout << ext.first << std::endl;
                  });

    // 2. Create Device

    auto devs = renderInstance.enumerateAvailableDevices();

    vkw::PhysicalDevice deviceDesc{renderInstance, 0u};

    if (devs.empty()) {
        std::cout << "No available devices supporting vulkan on this machine." << std::endl <<
                  " Make sure your graphics drivers are installed and updated." << std::endl;
        return 1;
    }

    deviceDesc.enableExtension(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

    deviceDesc.isFeatureSupported(vkw::feature::multiViewport());

    auto device = vkw::Device{renderInstance, deviceDesc};


    uint32_t counter = 0;
    for (auto &d: devs) {
        VkPhysicalDeviceProperties props{};
        renderInstance.core<1, 0>().vkGetPhysicalDeviceProperties(d, &props);
        std::cout << "Device " << counter << ": " << props.deviceName << std::endl;
        counter++;
    }

    // 3. Create surface

    auto surface = window.surface(renderInstance);

    // 4. Create swapchain

    auto mySwapChain = std::make_unique<TestApp::SwapChainImpl>(TestApp::SwapChainImpl{device, surface});

    auto queue = device.getGraphicsQueue();

    // 5. create vertex buffer

    auto texture = loadTexture("image.png", device);

    // 6. load data to vertex buffer


    auto fence = std::make_unique<vkw::Fence>(device);

    // 7. create command pool

    auto commandPool = vkw::CommandPool{device, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                                                VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                        device.getGraphicsQueue()->familyIndex()};

    VkComponentMapping mapping;
    mapping.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    mapping.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    mapping.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    mapping.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    auto &textureView = texture.getView<vkw::ColorImageView>(device, VK_FORMAT_R8G8B8A8_UNORM, mapping);

    // 8. create swapchain images views for framebuffer

    std::vector<vkw::Image2DArrayViewCRef> swapChainImageViews;
    auto swapChainImages = mySwapChain->retrieveImages();
    for (auto &image: swapChainImages) {
        swapChainImageViews.emplace_back(image.getView<vkw::ColorImageView>(device, image.format(), 0, 1, mapping));
    }

    // 9. create render pass

    auto attachmentDescription = vkw::AttachmentDescription{swapChainImageViews.front().get().format(),
                                                            VK_SAMPLE_COUNT_1_BIT,
                                                            VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE,
                                                            VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                                            VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                                            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                                            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR};
    auto depthAttachment = vkw::AttachmentDescription{VK_FORMAT_D24_UNORM_S8_UINT,
                                                      VK_SAMPLE_COUNT_1_BIT,
                                                      VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                                      VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                                      VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                                      VK_IMAGE_LAYOUT_UNDEFINED,
                                                      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
    auto subpassDescription = vkw::SubpassDescription{};
    subpassDescription.addColorAttachment(attachmentDescription, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    subpassDescription.addDepthAttachment(depthAttachment, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

    auto inputDependency = vkw::SubpassDependency{};
    inputDependency.setDstSubpass(subpassDescription);
    inputDependency.srcAccessMask = 0;
    inputDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    inputDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    inputDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    inputDependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    auto outputDependency = vkw::SubpassDependency{};
    outputDependency.setSrcSubpass(subpassDescription);
    outputDependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    outputDependency.dstAccessMask = 0;
    outputDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    outputDependency.dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    outputDependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;


    auto renderPass = vkw::RenderPass{device, vkw::RenderPassCreateInfo{{attachmentDescription, depthAttachment},
                                                                        {subpassDescription},
                                                                        {inputDependency,       outputDependency}}};

    // 10. create framebuffer for each swapchain image view

    auto extents = surface.getSurfaceCapabilities(device.physicalDevice()).currentExtent;

    std::optional<vkw::DepthStencilImage2D> depthMap = createDepthStencilImage(device, extents.width, extents.height);
    vkw::DepthImage2DArrayView const *depthImageView = &depthMap.value().getView<vkw::DepthImageView>(device, mapping,
                                                                                                      VK_FORMAT_D24_UNORM_S8_UINT);
    std::vector<vkw::FrameBuffer> framebuffers;

    for (auto &view: swapChainImageViews) {
        framebuffers.emplace_back(device, renderPass, VkExtent2D{view.get().image()->rawExtents().width,
                                                                 view.get().image()->rawExtents().height},
                                  vkw::Image2DArrayViewConstRefArray{view, *depthImageView});
    }
    // 11. create Cubes
    VmaAllocationCreateInfo createInfo{};
    createInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    createInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;

    vkw::UniformBuffer<GlobalUniform> uBuf{device, createInfo};

    constexpr const int cubeCount = 50000;

    Cube::Init(device, renderPass, 0, cubeCount, texture);

    auto* uBufMapped = uBuf.map();

    std::vector<Cube> cubes{};

    for(int i = 0; i < cubeCount; ++i) {
        float scale_mag = (float)(rand() % 5) + 1.0f;
        glm::vec3 pos = glm::vec3((float)(rand() % 1000), (float)(rand() % 1000), (float)(rand() % 1000));
        glm::vec3 rotate = glm::vec3((float)(rand() % 1000), (float)(rand() % 1000), (float)(rand() % 1000));
        auto scale = glm::vec3(scale_mag);
        cubes.emplace_back(pos, scale, rotate);
    }

    Cube::writeGlobalUniform(uBuf);

    // 12. create command buffer and sync primitives

    auto commandBuffer = vkw::PrimaryCommandBuffer{commandPool};

    auto presentComplete = vkw::Semaphore{device};
    auto renderComplete = vkw::Semaphore{device};


    // 13. render on screen in a loop

    uint32_t framesTotal = 0;
    uint32_t framesCount = 0;
    double elapsedTime = 0.0;
    double fps = 0.0;
    std::chrono::time_point<std::chrono::high_resolution_clock,
            std::chrono::duration<double>> tStart, tFinish;

    tStart = std::chrono::high_resolution_clock::now();
    myUniform.perspective = window.projection();

    constexpr const int thread_count = 12;
    std::vector<std::thread> threads;
    threads.reserve(thread_count);

    while (!window.shouldClose()) {
        framesTotal++;

        TestApp::Window::pollEvents();

        tFinish = std::chrono::high_resolution_clock::now();

        double deltaTime = std::chrono::duration<double, std::milli>(tFinish - tStart).count() / 1000.0;
        elapsedTime += deltaTime;

        tStart = std::chrono::high_resolution_clock::now();
        framesCount++;
        if(framesCount > 100){
            fps = (float)framesCount / elapsedTime;
            std::cout << "FPS: " << fps << std::endl;
            framesCount = 0;
            elapsedTime = 0;
        }

        window.update(deltaTime);

        extents = surface.getSurfaceCapabilities(device.physicalDevice()).currentExtent;

        if (extents.width == 0 || extents.height == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        if (fence->signaled()) {
            fence->wait();
            fence->reset();
        }

        try {
            mySwapChain->acquireNextImage(presentComplete, 1000);
            myUniform.perspective = window.projection();
            memcpy(uBufMapped, &myUniform, sizeof(myUniform));
            uBuf.flush();

            commandBuffer.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
            auto &currentFrameBuffer = framebuffers.at(mySwapChain->currentImage());
            std::array<VkClearValue, 2> values{};
            values.at(0).color = {0.1f, 0.0f, 0.0f, 0.0f};
            values.at(1).depthStencil.depth = 1.0f;
            values.at(1).depthStencil.stencil = 0.0f;

            commandBuffer.beginRenderPass(renderPass, currentFrameBuffer, currentFrameBuffer.getFullRenderArea(), false,
                                          values.size(), values.data());
            VkViewport viewport;

            viewport.height = currentFrameBuffer.getFullRenderArea().extent.height;
            viewport.width = currentFrameBuffer.getFullRenderArea().extent.width;
            viewport.x = viewport.y = 0.0f;
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            VkRect2D scissor;
            scissor.extent.width = currentFrameBuffer.getFullRenderArea().extent.width;
            scissor.extent.height = currentFrameBuffer.getFullRenderArea().extent.height;
            scissor.offset.x = 0;
            scissor.offset.y = 0;
            commandBuffer.setViewports({viewport}, 0);
            commandBuffer.setScissors({scissor}, 0);

            auto per_thread = cubes.size() / thread_count;

            threads.clear();

            for(int i = 0; i < thread_count;++i){
                threads.emplace_back([&cubes, i, per_thread, deltaTime](){
                    for(int j = per_thread * i; j < std::min(per_thread * (i + 1), cubes.size()); ++j)
                        cubes.at(j).update(deltaTime);
                });
            }

            for(auto& thread: threads)
                thread.join();

            Cube::draw(commandBuffer);

            commandBuffer.endRenderPass();
            commandBuffer.end();

            queue->submit(commandBuffer, presentComplete, {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT},
                          renderComplete);

        } catch (vkw::VulkanError &e) {
            if (e.result() == VK_ERROR_OUT_OF_DATE_KHR) {
                mySwapChain.reset();
                mySwapChain = std::make_unique<TestApp::SwapChainImpl>(TestApp::SwapChainImpl{device, surface});
                swapChainImages.clear();
                swapChainImages = mySwapChain->retrieveImages();

                swapChainImageViews.clear();

                for (auto &image: swapChainImages) {
                    swapChainImageViews.emplace_back(
                            image.getView<vkw::ColorImageView>(device, image.format(), 0, 1, mapping));
                }

                extents = surface.getSurfaceCapabilities(device.physicalDevice()).currentExtent;

                depthMap.reset();
                depthMap.emplace(createDepthStencilImage(device, extents.width, extents.height));
                depthImageView = &depthMap.value().getView<vkw::DepthImageView>(device, mapping,
                                                                                VK_FORMAT_D24_UNORM_S8_UINT);
                framebuffers.clear();

                for (auto &view: swapChainImageViews) {
                    framebuffers.emplace_back(device, renderPass, VkExtent2D{view.get().image()->rawExtents().width,
                                                                             view.get().image()->rawExtents().height},
                                              vkw::Image2DArrayViewConstRefArray{view, *depthImageView});
                }

                //std::cout << "Window resized" << std::endl;
                continue;
            } else {
                throw;
            }
        }
        queue->present(*mySwapChain, renderComplete);
        queue->waitIdle();
    }


    // 14. clear resources and wait for device processes to finish

    if (fence->signaled()) {
        fence->wait();
        fence->reset();
    }

    device.waitIdle();

    fence.reset();
    mySwapChain.reset();

    cubes.clear();
    Cube::Terminate();

    return 0;
}
