#include "ShadowPass.h"
#include "Model.h"
#include "GlobalLayout.h"
#include "SkyBox.h"
#include "ErrorCallbackWrapper.h"
#include "CommonApp.h"

using namespace TestApp;

class ModelTransform: public GUIWindow{
public:
    ModelTransform(GUIFrontEnd& gui): GUIWindow(gui, WindowSettings{.title="Model transform"}){}

    std::function<void(void)> gui_impl = [](){};
protected:

    void onGui() override {
        gui_impl();
    }
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
                skybox(device(), onScreenPass(), 0, shaderLoader()),
                globalState{device(), onScreenPass(), 0, window().camera(), shadowPass, skybox},
                skyboxSettings(gui(), skybox, "Sky box"),
                defaultTextures(device()),
                modelPipelinePool(device(), shaderLoader()),
                modelTransform(gui()){
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


        modelTransform.gui_impl = [this]() {

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
                modelPipelinePool.clear();
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

            static float splitL = window().camera().splitLambda();
            if(ImGui::SliderFloat("Split lambda", &splitL, 0.0f, 1.0f)){
                window().camera().setSplitLambda(splitL);
            }
            static float farClip = window().camera().farClip();
            if(ImGui::SliderFloat("Far clip", &farClip, window().camera().nearPlane(), window().camera().farPlane())){
                window().camera().setFarClip(farClip);
            }
        };

        shadowPass.onPass = [this](RenderEngine::GraphicsRecordingState& state, const Camera& camera){
            instance->drawGeometryOnly(state);

            for(auto& inst: instances)
                inst.drawGeometryOnly(state);
        };
    }

protected:
    void preMainPass(vkw::PrimaryCommandBuffer& buffer, RenderEngine::GraphicsPipelinePool& pool) override{
        RenderEngine::GraphicsRecordingState recorder{buffer, modelPipelinePool};
        shadowPass.execute(buffer, recorder);
    }

    void onMainPass(vkw::PrimaryCommandBuffer& buffer, RenderEngine::GraphicsRecordingState& recorder) override{
        RenderEngine::GraphicsRecordingState localRecorder{buffer, modelPipelinePool};
        skybox.draw(recorder);

        globalState.bind(localRecorder);

        instance->draw(localRecorder);

        for(auto& inst: instances)
            inst.draw(localRecorder);

    }

    void onPollEvents() override{
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
    SkyBox skybox;
    GlobalLayout globalState;
    SkyBoxSettings skyboxSettings;
    TestApp::DefaultTexturePool defaultTextures;
    std::unique_ptr<GLTFModel> model;
    std::vector<GLTFModelInstance> instances;
    std::unique_ptr<GLTFModelInstance> instance;
    RenderEngine::GraphicsPipelinePool modelPipelinePool;
    std::vector<std::filesystem::path> modelList;
    std::vector<std::string> modelListString{};
    std::vector<const char *> modelListCstr{};
    ModelTransform modelTransform;
};

int runModel() {
    ModelApp app{};
    app.run();
    return 0;
}

int main(){
    return ErrorCallbackWrapper<decltype(&runModel)>::run(&runModel);
}

