#include "ShadowPass.h"
#include "Model.h"
#include "GlobalLayout.h"
#include "SkyBox.h"
#include "ErrorCallbackWrapper.h"
#include "CommonApp.h"

using namespace TestApp;

class GUI : public GUIFrontEnd, public GUIBackend {
public:
    GUI(TestApp::SceneProjector &window, vkw::Device &device, vkw::RenderPass &pass, uint32_t subpass,
        RenderEngine::TextureLoader const &textureLoader)
            : GUIBackend(device, pass, subpass, textureLoader),
              m_window(window) {
        ImGui::SetCurrentContext(context());
        auto &io = ImGui::GetIO();

        ImGuiStyle &style = ImGui::GetStyle();
        style.Colors[ImGuiCol_TitleBg] = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
        style.Colors[ImGuiCol_TitleBgActive] = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
        style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(1.0f, 0.0f, 0.0f, 0.1f);
        style.Colors[ImGuiCol_MenuBarBg] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
        style.Colors[ImGuiCol_Header] = ImVec4(0.8f, 0.0f, 0.0f, 0.4f);
        style.Colors[ImGuiCol_HeaderActive] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
        style.Colors[ImGuiCol_HeaderHovered] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
        style.Colors[ImGuiCol_FrameBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.8f);
        style.Colors[ImGuiCol_CheckMark] = ImVec4(1.0f, 0.0f, 0.0f, 0.8f);
        style.Colors[ImGuiCol_SliderGrab] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
        style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(1.0f, 0.0f, 0.0f, 0.8f);
        style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(1.0f, 1.0f, 1.0f, 0.1f);
        style.Colors[ImGuiCol_FrameBgActive] = ImVec4(1.0f, 1.0f, 1.0f, 0.2f);
        style.Colors[ImGuiCol_Button] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
        style.Colors[ImGuiCol_ButtonHovered] = ImVec4(1.0f, 0.0f, 0.0f, 0.6f);
        style.Colors[ImGuiCol_ButtonActive] = ImVec4(1.0f, 0.0f, 0.0f, 0.8f);

        m_updateFontTexture();

        io.DisplaySize = {800, 600};
    }

    std::function<void(void)> customGui = []() {};

protected:
    void gui() const override {
        ImGui::Begin("Scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::Text("FPS: %.2f", m_window.get().clock().fps());
        auto &camera = m_window.get().camera();
        auto pos = camera.position();
        ImGui::Text("X: %.2f, Y: %.2f, Z: %.2f,", pos.x, pos.y, pos.z);
        ImGui::Text("(%.2f,%.2f)", camera.phi(), camera.psi());

        ImGui::SliderFloat("Cam rotate inertia", &camera.rotateInertia, 0.1f, 5.0f);
        ImGui::SliderFloat("Mouse sensitivity", &m_window.get().mouseSensitivity, 1.0f, 10.0f);

        ImGui::End();

        customGui();
    }

private:
    std::reference_wrapper<TestApp::SceneProjector> m_window;

};

std::vector<std::filesystem::path> listAvailableModels(std::filesystem::path const &root) {
    if (!exists(root))
        return {};
    std::vector<std::filesystem::path> ret{};
    for (const auto &entry: std::filesystem::directory_iterator(root)) {
        auto &entry_path = entry.path();
        if (entry_path.has_extension()) {
            if (entry_path.extension() == ".gltf" || entry_path.extension() == ".glb") {
                ret.emplace_back(entry_path);
            }
        }
    }
    return ret;
}

class ModelApp final: public CommonApp{
public:
    ModelApp(): CommonApp(AppCreateInfo{true, "Model"}),
                shadowPass(device()),
                gui(new GUI(window(), device(), onScreenPass(), 0, textureLoader())),
                skybox(device(), onScreenPass(), 0, shaderLoader()),
                globalState{device(), onScreenPass(), 0, window().camera(), shadowPass, skybox},
                skyboxSettings(*gui, skybox, "Sky box"),
                defaultTextures(device()){
        modelList = listAvailableModels("data/models");

        std::transform(modelList.begin(), modelList.end(), std::back_inserter(modelListString),
                       [](std::filesystem::path const &path) {
                           return path.filename().string();
                       });
        std::transform(modelListString.begin(), modelListString.end(), std::back_inserter(modelListCstr),
                       [](std::string const &name) {
                           return name.c_str();
                       });
        if (modelList.empty()) {
            throw std::runtime_error("No GLTF model files found in data/models");
        }

        model = std::make_unique<GLTFModel>(device(), defaultTextures, modelList.front());
        instances.reserve(100);
        for(int i = 0; i < 100; ++i){
            auto& inst = instances.emplace_back(model->createNewInstance());
            inst.translation = glm::vec3((float)(i % 10) * 5.0f, -10.0f, (float)(i / 10) * 5.0f);
            inst.update();
        }
        instance = std::make_unique<GLTFModelInstance>(model->createNewInstance());
        instance->update();


        gui->customGui = [this]() {
            //ImGui::SetNextWindowSize({400.0f, 150.0f}, ImGuiCond_Once);
            ImGui::Begin("Model", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize);

            static int current_model = 0;
            bool upd = false;
            upd = ImGui::SliderFloat("rot x", &instance->rotation.x, -180.0f, 180.0f) | upd;
            upd = ImGui::SliderFloat("rot y", &instance->rotation.y, -180.0f, 180.0f) | upd;
            upd = ImGui::SliderFloat("rot z", &instance->rotation.z, -180.0f, 180.0f) | upd;
            upd = ImGui::SliderFloat("scale x", &instance->scale.x, 0.1f, 10.0f) | upd;
            upd = ImGui::SliderFloat("scale y", &instance->scale.y, 0.1f, 10.0f) | upd;
            upd = ImGui::SliderFloat("scale z", &instance->scale.z, 0.1f, 10.0f) | upd;

            if (upd)
                instance->update();

            if (ImGui::Combo("models", &current_model, modelListCstr.data(), modelListCstr.size())) {
                {
                    // hack to get instance destroyed
                    auto dummy = std::move(instance);
                }
                instance.reset();
                instances.clear();
                model.reset();
                model = std::make_unique<GLTFModel>(device(), defaultTextures, modelList.at(current_model));
                instance = std::make_unique<GLTFModelInstance>(model->createNewInstance());
                instance->update();
                for(int i = 0; i < 100; ++i){
                    auto& inst = instances.emplace_back(model->createNewInstance());
                    inst.translation = glm::vec3((float)(i % 10) * 5.0f, -10.0f, (float)(i / 10) * 5.0f);
                    inst.update();
                }
            }


            ImGui::End();

            ImGui::Begin("Globals", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

            static float splitL = window().camera().splitLambda();
            if(ImGui::SliderFloat("Split lambda", &splitL, 0.0f, 1.0f)){
                window().camera().setSplitLambda(splitL);
            }
            static float farClip = window().camera().farClip();
            if(ImGui::SliderFloat("Far clip", &farClip, window().camera().nearPlane(), window().camera().farPlane())){
                window().camera().setFarClip(farClip);
            }
            ImGui::End();
        };

        shadowPass.onPass = [this](RenderEngine::GraphicsRecordingState& state, const Camera& camera){
            instance->drawGeometryOnly(state);

            for(auto& inst: instances)
                inst.drawGeometryOnly(state);
        };
        attachGUI(gui.get());
    }

protected:
    virtual void preMainPass(vkw::PrimaryCommandBuffer& buffer, RenderEngine::GraphicsPipelinePool& pool) {
        RenderEngine::GraphicsRecordingState recorder{buffer, pool};
        shadowPass.execute(buffer, recorder);
    }

    virtual void onMainPass(RenderEngine::GraphicsRecordingState& recorder) {
        skybox.draw(recorder);

        globalState.bind(recorder);

        instance->draw(recorder);

        for(auto& inst: instances)
            inst.draw(recorder);

    }

    virtual void onPollEvents() {
        gui->frame();
        gui->push();
        globalState.update();
        shadowPass.update(window().camera(), skybox.sunDirection());
        skybox.update(window().camera());
        globalState.update();
        if(skyboxSettings.needRecomputeOutScatter()){
            skybox.recomputeOutScatter();
        }

    }

private:
    ShadowRenderPass shadowPass;
    std::unique_ptr<GUI> gui;
    SkyBox skybox;
    GlobalLayout globalState;
    SkyBoxSettings skyboxSettings;
    TestApp::DefaultTexturePool defaultTextures;
    std::unique_ptr<GLTFModel> model;
    std::vector<GLTFModelInstance> instances;
    std::unique_ptr<GLTFModelInstance> instance;
    std::vector<std::filesystem::path> modelList;
    std::vector<std::string> modelListString{};
    std::vector<const char *> modelListCstr{};
};

int runModel() {
    ModelApp app{};
    app.run();
    return 0;
}

int main(){
    return ErrorCallbackWrapper<decltype(&runModel)>::run(&runModel);
}

