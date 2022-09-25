//
// Created by Бушев Дмитрий on 13.03.2022.
//

#include "RenderPassesImpl.h"
#include <span>
vkw::RenderPassCreateInfo TestApp::LightPass::m_compile_info(VkFormat colorFormat, VkFormat depthFormat, VkImageLayout colorLayout) {
    auto attachmentDescription = vkw::AttachmentDescription{colorFormat,
                                                            VK_SAMPLE_COUNT_1_BIT,
                                                            VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE,
                                                            VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                                            VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                                            colorLayout,
                                                            colorLayout};
    auto depthAttachment = vkw::AttachmentDescription{depthFormat,
                                                      VK_SAMPLE_COUNT_1_BIT,
                                                      VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                                      VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                                      VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                                      VK_IMAGE_LAYOUT_UNDEFINED,
                                                      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
    m_attachments.push_back(attachmentDescription);
    m_attachments.push_back(depthAttachment);

    auto subpassDescription = vkw::SubpassDescription{};
    subpassDescription.addColorAttachment(m_attachments.at(0), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    subpassDescription.addDepthAttachment(m_attachments.at(1), VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

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


    return vkw::RenderPassCreateInfo{std::span<vkw::AttachmentDescription, 2>{m_attachments.begin(), m_attachments.begin() + 1},
                                     {subpassDescription},
                                                              {inputDependency,       outputDependency}};
}

vkw::RenderPassCreateInfo TestApp::ShadowPass::m_compile_info(VkFormat attachmentFormat) {
    auto depthAttachment = vkw::AttachmentDescription{attachmentFormat,
                                                      VK_SAMPLE_COUNT_1_BIT,
                                                      VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE,
                                                      VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                                      VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                                      VK_IMAGE_LAYOUT_UNDEFINED,
                                                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    m_attachments.push_back(depthAttachment);
    auto subpassDescription = vkw::SubpassDescription{};
    subpassDescription.addDepthAttachment(m_attachments.at(0), VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

    auto inputDependency = vkw::SubpassDependency{};
    inputDependency.setDstSubpass(subpassDescription);
    inputDependency.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    inputDependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    inputDependency.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    inputDependency.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    inputDependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    auto outputDependency = vkw::SubpassDependency{};
    outputDependency.setSrcSubpass(subpassDescription);
    outputDependency.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    outputDependency.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    outputDependency.srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    outputDependency.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    outputDependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;


    return vkw::RenderPassCreateInfo{{m_attachments.at(0)},
                                     {subpassDescription},
                                     {inputDependency, outputDependency}};
}
