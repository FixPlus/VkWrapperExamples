#include "ShadowPass.h"
#include "Model.h"
#include "GlobalLayout.h"
#include "SkyBox.h"
#include "ErrorCallbackWrapper.h"
#include "CommonApp.h"
#include "AssetPath.inc"
#include "RenderEngine/Window/Boxer.h"
#include <cwchar>

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
std::optional<std::filesystem::path> modelPath(std::filesystem::path const &modelDir){
    auto binary_path = modelDir / L"glTF-Binary";
    if(exists(binary_path)){
        auto dirIt = std::filesystem::directory_iterator(binary_path);
        auto foundGLB = std::ranges::find_if(dirIt, [](auto& file){ return file.path().extension() == L".glb"; });
        if(foundGLB != std::ranges::end(dirIt)){
            return foundGLB->path();
        }
    }

    auto json_path = modelDir / L"glTF";
    if(exists(json_path)){
        auto dirIt = std::filesystem::directory_iterator(json_path);
        auto foundGLTF = std::ranges::find_if(dirIt, [](auto& file){ return file.path().extension() == L".gltf"; });
        if(foundGLTF != std::ranges::end(dirIt)){
            return foundGLTF->path();
        }
    }

    return {};
}
std::vector<std::filesystem::path> listAvailableModels(std::filesystem::path const &root) {
    if (!exists(root))
        return {};
    std::vector<std::filesystem::path> ret{};
    for (const auto &entry: std::filesystem::directory_iterator(root)) {
        if(!entry.is_directory())
            continue;
        auto entryFile = modelPath(entry.path());
        if(entryFile)
            ret.push_back(entryFile.value());
    }
    return ret;
}
std::unique_ptr<GLTFModel> tryLoad(vkw::Device &renderer, DefaultTexturePool& pool, std::filesystem::path const &path){

    try{
        return std::make_unique<GLTFModel>(renderer, pool, path);
    } catch (std::runtime_error& e){
        std::stringstream ss;
        ss << "Error while loading " << path.filename();
        RenderEngine::Boxer::show(e.what(), ss.str(), RenderEngine::Boxer::Style::Error);
    }

    return nullptr;
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
        modelList = listAvailableModels(EXAMPLE_GLTF_PATH);

        std::transform(modelList.begin(), modelList.end(), std::back_inserter(modelListString),
                       [](std::filesystem::path const &path) {
                           return path.filename().string();
                       });
        std::transform(modelListString.begin(), modelListString.end(), std::back_inserter(modelListCstr),
                       [](std::string const &name) {
                           return name.c_str();
                       });
        if (modelList.empty()) {
            std::stringstream ss;
            ss << "No GLTF model files found in " << EXAMPLE_GLTF_PATH;
            throw std::runtime_error(ss.str());
        }

        int loadedModel = 0;
        for(auto& modelPath: modelList){
            model = tryLoad(device(), defaultTextures, modelPath);
            if(model)
                break;
            loadedModel++;
        }
        if(loadedModel == modelList.size()){
            throw std::runtime_error("Failed to load any GLTF model");
        }

        model = std::make_unique<GLTFModel>(device(), defaultTextures, modelList.front());
        instance = std::make_unique<GLTFModelInstance>(model->createNewInstance());
        instance->update();


        modelTransform.gui_impl = [this, loadedModel]() {

            static int current_model = loadedModel;
            bool upd = false;
            upd = ImGui::SliderFloat("rot x", &instance->rotation.x, -180.0f, 180.0f) | upd;
            upd = ImGui::SliderFloat("rot y", &instance->rotation.y, -180.0f, 180.0f) | upd;
            upd = ImGui::SliderFloat("rot z", &instance->rotation.z, -180.0f, 180.0f) | upd;
            upd = ImGui::SliderFloat("scale x", &instance->scale.x, 0.1f, 10.0f) | upd;
            upd = ImGui::SliderFloat("scale y", &instance->scale.y, 0.1f, 10.0f) | upd;
            upd = ImGui::SliderFloat("scale z", &instance->scale.z, 0.1f, 10.0f) | upd;

            if (upd)
                instance->update();

            auto oldSelect = current_model;
            if (ImGui::Combo("models", &current_model, modelListCstr.data(), modelListCstr.size())) {
                {
                    // hack to get instance destroyed
                    auto dummy = std::move(instance);
                }
                instance.reset();
                modelPipelinePool.clear();
                auto expectModel = tryLoad(device(), defaultTextures, modelList.at(current_model));
                if(!expectModel){
                    current_model = oldSelect;
                } else{
                    model.reset();
                    model = std::move(expectModel);
                }
                instance = std::make_unique<GLTFModelInstance>(model->createNewInstance());
                instance->update();
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
    std::unique_ptr<GLTFModelInstance> instance;
    RenderEngine::GraphicsPipelinePool modelPipelinePool;
    std::vector<std::filesystem::path> modelList;
    std::vector<std::string> modelListString{};
    std::vector<const char *> modelListCstr{};
    ModelTransform modelTransform;
};

int runModel() {
    setlocale(LC_ALL, "en_us.utf8");
#ifdef _MSC_VER
    _wsetlocale(LC_ALL, L"en_us.utf8");
#endif
    ModelApp app{};
    app.run();
    return 0;
}

int main(){
    return ErrorCallbackWrapper<decltype(&runModel)>::run(&runModel);
}

