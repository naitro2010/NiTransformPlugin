#include "GameEventHandler.h"
#include "Hooks.h"
#include "detours/detours.h"
namespace plugin {
    static std::recursive_mutex nitransform_task_mutex;
    static std::queue<std::function<void()>> nitransform_task_pool;
    static auto original_process_task = (void (*)(void* main, void* arg2, void* arg3, void* arg4)) nullptr;
    static void ProcessNiTransformTasks(void* main, void* arg2, void* arg3, void* arg4) {
        original_process_task(main, arg2, arg3, arg4);
        std::queue<std::function<void()>> queue_copy;
        {
            std::lock_guard l(nitransform_task_mutex);
            queue_copy = (nitransform_task_pool);
            nitransform_task_pool = std::queue<std::function<void()>>();
        }
        while (true) {
            auto task = std::function<void()>(nullptr);
            {
                if (queue_copy.empty()) {
                    return;
                }
                task = queue_copy.front();
                queue_copy.pop();
            }
            task();
        }
    }

    static void AddNiTransformTask(std::function<void()> fn) {
        std::lock_guard l(nitransform_task_mutex);
        nitransform_task_pool.push(fn);
    }

    class NiQuatTransform {
    public:
        RE::NiPoint3 translation;
        RE::NiQuaternion rotation;
        float scale;
    };
    RE::NiTimeController* WalkControllers(RE::NiNode* n, RE::BSFixedString& nodeName) {
        if (!n) {
            return nullptr;
        }
        if (auto obj = n->GetObjectByName(nodeName)) {
            if (auto found_node = obj) {
                if (auto controller=found_node->GetController<RE::NiTimeController>()) {
                    return controller;
                }
            }
        }
        if (std::string(n->name.c_str()).ends_with(nodeName)) {
        
            if (auto found_node = n) {
                if (auto controller = found_node->GetController<RE::NiTimeController>()) {
                    return controller;
                }
            }

        }
        for (auto& c: n->GetChildren()) {
            if (!c.get()) {
                continue;
            }
            if (auto geo = c.get()->AsGeometry()) {
            
                if (std::string(geo->name.c_str()).ends_with(nodeName)) {
                    if (geo->GetGeometryRuntimeData().properties[RE::BSGeometry::States::kProperty]) {
                        if (auto controller=geo->GetGeometryRuntimeData()
                                .properties[RE::BSGeometry::States::kProperty]
                                ->GetController<RE::NiTimeController>()) {
                        
                            return controller;
                        
                        }
                    }
                }        
            
            
            
            }
            if (auto controller = WalkControllers(c.get()->AsNode(), nodeName)) {
                return controller;
            }
        }

        return nullptr;

        
    }

    RE::NiNode* WalkNode(RE::NiNode* n, RE::BSFixedString& nodeName) {
        if (!n) {
            return nullptr;
        }
        if (auto obj = n->GetObjectByName(nodeName)) {
            if (auto found_node = obj->AsNode()) {
                return found_node;
            }
        }
        if (std::string(n->name.c_str()).ends_with(nodeName)) {
            return n;
        }
        for (auto& c : n->GetChildren()) {
            if (!c.get()) {
                continue;
            }
            if (auto geo = c.get()->AsGeometry()) {

                if (auto skin = geo->GetGeometryRuntimeData().skinInstance) {
                    if (skin->skinData) {
                        for (unsigned int bone_idx = 0; bone_idx < skin->skinData->bones; bone_idx++) {
                            auto bone = skin->bones[bone_idx];
                            if (bone->AsNode() && std::string(bone->name.c_str()).ends_with(nodeName)) {
                                return bone->AsNode();
                            }
                            if (auto found_node = WalkNode(bone->AsNode(), nodeName)) {
                                return found_node;
                            }
                        }
                    }
                }
            }
            if (auto found_node = WalkNode(c.get()->AsNode(), nodeName)) {
                return found_node;
            }
        }

        return nullptr;
    }
    bool NiTransformOperation(RE::Actor* actor, RE::TESObjectARMO* armor, RE::BSFixedString nodeName,
                              std::function<bool(RE::NiTimeController*)> callback) {
        bool retval = false;
        if (!actor) {
            return retval;
        }
        if (!armor) {
            return retval;
        }
        if (auto& biped = actor->GetBiped1(false)) {
            for (uint32_t idx = 0; idx < RE::BIPED_OBJECTS::kTotal; idx++) {
                if (biped->objects[idx].item != armor) {
                    continue;
                }
                if (!biped->objects[idx].partClone) {
                    continue;
                }
                auto obj = WalkNode(biped->objects[idx].partClone->AsNode(), nodeName);
                if (!obj) {
                    continue;
                }
                if (auto node = obj->AsNode()) {

                    if (auto controller = node->GetController((RE::NiRTTI*) RE::NiRTTI_NiTimeController.address())) {
                        retval = callback((RE::NiTimeController*) controller);
                    }
                }
                if (auto controller = WalkControllers(biped->objects[idx].partClone->AsNode(), nodeName)) {
                    retval = callback((RE::NiTimeController*) controller);
                }
            }
            for (uint32_t idx = 0; idx < RE::BIPED_OBJECTS::kTotal; idx++) {
                if (biped->bufferedObjects[idx].item != armor) {
                    continue;
                }
                if (!biped->bufferedObjects[idx].partClone) {
                    continue;
                }
                auto obj = WalkNode(biped->bufferedObjects[idx].partClone->AsNode(), nodeName);
                if (!obj) {
                    continue;
                }
                if (auto node = obj->AsNode()) {
                    if (auto controller = node->GetController((RE::NiRTTI*) RE::NiRTTI_NiTimeController.address())) {
                        retval = callback((RE::NiTimeController*) controller);
                    }
                }
                if (auto controller=WalkControllers(biped->bufferedObjects[idx].partClone->AsNode(), nodeName)) {
                    retval = callback((RE::NiTimeController*) controller);
                }
            }
        }
        return retval;
    }
    bool SetNiTransformFrequency(RE::StaticFunctionTag*, RE::Actor* actor, RE::TESObjectARMO* armor, RE::BSFixedString nodeName,
                                 float frequency) {
        return NiTransformOperation(actor, armor, nodeName, [frequency](RE::NiTimeController* controller) {
            controller->frequency = frequency;
            return true;
        });
    }

    bool SetNiTransformStart(RE::StaticFunctionTag*, RE::Actor* actor, RE::TESObjectARMO* armor, RE::BSFixedString nodeName) {
        return NiTransformOperation(actor, armor, nodeName, [](RE::NiTimeController* controller) {
            auto update_fn = (void (*)(RE::NiTimeController* c, float* t))((*(uint64_t**) controller)[0x27]);
            {
                float* real_world_time_ptr = (float*) REL::RelocationID(517597, 400912, 517597).address();
                float time_per_anim = ((controller->hiKeyTime - controller->loKeyTime));

                if (controller->frequency != 0.0) {
                    controller->phase =
                        fmodf(controller->phase + (time_per_anim+0.00001f) - controller->ComputeScaledTime(*real_world_time_ptr), time_per_anim);
                }
                controller->flags.set(RE::NiTimeController::Flag::kActive);
                float tm = *real_world_time_ptr;
                if (controller->flags.any(RE::NiTimeController::Flag::kForceUpdate)) {
                    update_fn(controller, &tm);
                } else {
                    controller->flags.set(RE::NiTimeController::Flag::kForceUpdate);
                    update_fn(controller, &tm);
                    controller->flags.reset(RE::NiTimeController::Flag::kForceUpdate);
                }
            }
            return true;
        });
    }
    bool NiTransformStop(RE::Actor* actor, RE::TESObjectARMO* armor, RE::BSFixedString nodeName) {
        return NiTransformOperation(actor, armor, nodeName, [](RE::NiTimeController* controller) {
            auto update_fn = (void (*)(RE::NiTimeController* c, float* t))((*(uint64_t**) controller)[0x27]);
            {
                float* real_world_time_ptr = (float*) REL::RelocationID(517597, 400912, 517597).address();
                float time_per_anim = ((controller->hiKeyTime - controller->loKeyTime));
                if (controller->frequency != 0.0) {
                    controller->phase = fmodf(controller->phase + (time_per_anim - 0.00001f) +
                                                  (time_per_anim - controller->ComputeScaledTime(*real_world_time_ptr)),
                                              time_per_anim);
                }

                float tm = *real_world_time_ptr;
                if (controller->flags.any(RE::NiTimeController::Flag::kForceUpdate)) {
                    update_fn(controller, &tm);
                } else {
                    controller->flags.set(RE::NiTimeController::Flag::kForceUpdate);
                    update_fn(controller, &tm);
                    controller->flags.reset(RE::NiTimeController::Flag::kForceUpdate);
                }

                controller->flags.reset(RE::NiTimeController::Flag::kActive);

                return true;
            }
        });
    }
    bool SetNiTransformStop(RE::StaticFunctionTag*, RE::Actor* actor, RE::TESObjectARMO* armor, RE::BSFixedString nodeName) {
        return NiTransformStop(actor, armor, nodeName);
    }
    bool SetNiTransformPause(RE::StaticFunctionTag*, RE::Actor* actor, RE::TESObjectARMO* armor, RE::BSFixedString nodeName, bool paused) {
        return NiTransformOperation(actor, armor, nodeName, [paused](RE::NiTimeController* controller) {
            //auto update_fn = (void (*)(RE::NiTimeController* c, float* t))((*(uint64_t**) controller)[0x27]);

            float* real_world_time_ptr = (float*) REL::RelocationID(517597, 400912, 517597).address();
            float time_per_anim = ((controller->hiKeyTime - controller->loKeyTime));
            auto lastscaledtime = 0.0f;
            //if (auto interpolator = controller->GetInterpolator()) {
            lastscaledtime = controller->scaledTime;
            //}

            if (paused) {
                controller->flags.reset(RE::NiTimeController::Flag::kActive);
            } else {
                if (!controller->flags.any(RE::NiTimeController::Flag::kActive)) {
                    //float time_diff = (*real_world_time_ptr - controller->lastTime);
                    if (controller->frequency != 0.0) {
                        auto original_scaled_phase = fmodf((controller->phase + (lastscaledtime)), time_per_anim);
                        auto new_scaled_phase =
                            fmodf((controller->phase + (controller->ComputeScaledTime(*real_world_time_ptr))), time_per_anim);
                        controller->phase = fmodf(controller->phase + (original_scaled_phase - new_scaled_phase), time_per_anim);
                        //controller->phase = fmodf(time_diff, 1.0f / controller->frequency);
                    }
                }
                controller->flags.set(RE::NiTimeController::Flag::kActive);
            }
            return true;
        });
    }
    bool CheckNiTransformStopDelayed(RE::ActorHandle handle, RE::FormID armor_form, RE::BSFixedString nodeName) {
        if (!handle) {
            return false;
        }
        if (auto actor_ptr = handle.get()) {
            if (auto armor = RE::TESForm::LookupByID<RE::TESObjectARMO>(armor_form)) {
                return NiTransformOperation(
                    actor_ptr.get(), armor, nodeName, [actor_ptr, armor, handle, armor_form, nodeName](RE::NiTimeController* controller) {
                        float* real_world_time_ptr = (float*) REL::RelocationID(517597, 400912, 517597).address();
                        if (!controller->flags.any(RE::NiTimeController::Flag::kActive)) {
                            return true;
                        }
                        auto last_scaled_time = controller->scaledTime;
                        if (last_scaled_time > (controller->ComputeScaledTime(*real_world_time_ptr) + 0.00001f)) {
                            return NiTransformStop(actor_ptr.get(), armor, nodeName);
                        }
                        AddNiTransformTask(
                            std::function([handle, armor_form, nodeName]() { CheckNiTransformStopDelayed(handle, armor_form, nodeName); }));
                        return true;
                    });
            }
        }
        return false;
    }
    bool SetNiTransformStopDelayed(RE::StaticFunctionTag*, RE::Actor* actor, RE::TESObjectARMO* armor, RE::BSFixedString nodeName) {
        if (actor && armor && actor->Is3DLoaded()) {
            auto handle = actor->GetHandle();
            auto armor_form = armor->GetFormID();
            return CheckNiTransformStopDelayed(handle, armor_form, nodeName);
        }
        return false;
    }
    void GameEventHandler::onLoad() {
        logger::info("onLoad()");
        Hooks::install();
    }

    void GameEventHandler::onPostLoad() {
        logger::info("onPostLoad()");
    }

    void GameEventHandler::onPostPostLoad() {
        logger::info("onPostPostLoad()");
    }

    void GameEventHandler::onInputLoaded() {
        logger::info("onInputLoaded()");
    }

    void GameEventHandler::onDataLoaded() {
        original_process_task = (void (*)(void* main, void* arg2, void* arg3, void* arg4)) REL::RelocationID(35565, 36564, 35565).address();
        if (original_process_task) {
            DetourTransactionBegin();
            DetourUpdateThread(GetCurrentThread());
            DetourAttach(&(PVOID&) original_process_task, &ProcessNiTransformTasks);
            DetourTransactionCommit();
        }
        RE::SkyrimVM::GetSingleton()->impl->RegisterFunction("SetNiTransformFrequency", "NiTransformPlugin", SetNiTransformFrequency,
                                                             false);
        RE::SkyrimVM::GetSingleton()->impl->RegisterFunction("SetNiTransformStart", "NiTransformPlugin", SetNiTransformStart, false);
        RE::SkyrimVM::GetSingleton()->impl->RegisterFunction("SetNiTransformStop", "NiTransformPlugin", SetNiTransformStop, false);
        RE::SkyrimVM::GetSingleton()->impl->RegisterFunction("SetNiTransformStopDelayed", "NiTransformPlugin", SetNiTransformStopDelayed,
                                                             false);
        RE::SkyrimVM::GetSingleton()->impl->RegisterFunction("SetNiTransformPause", "NiTransformPlugin", SetNiTransformPause, false);
        logger::info("onDataLoaded()");
    }

    void GameEventHandler::onNewGame() {
        logger::info("onNewGame()");
    }

    void GameEventHandler::onPreLoadGame() {
        logger::info("onPreLoadGame()");
    }

    void GameEventHandler::onPostLoadGame() {
        logger::info("onPostLoadGame()");
    }

    void GameEventHandler::onSaveGame() {
        logger::info("onSaveGame()");
    }

    void GameEventHandler::onDeleteGame() {
        logger::info("onDeleteGame()");
    }
}  // namespace plugin
