#include "GameEventHandler.h"
#include "Hooks.h"

namespace plugin {
    class NiQuatTransform {
        public:
            RE::NiPoint3 translation;
            RE::NiQuaternion rotation;
            float scale;
    };
    RE::NiNode* WalkNode(RE::NiNode* n, RE::BSFixedString &nodeName) {
        if (!n) {
            return nullptr;
        }
        if (auto obj=n->GetObjectByName(nodeName)) {
            if (auto found_node = obj->AsNode()) {
                return found_node;
            }
        }
        if (std::string(n->name.c_str()).ends_with(nodeName)) {
            return n;
        }
        for (auto& c: n->GetChildren()) {
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
        std::function<bool(RE::NiInterpController*)> callback) 
    {
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
                auto obj = WalkNode(biped->objects[idx].partClone->AsNode(),nodeName);
                if (!obj)
                {
                    continue;
                }
                if (auto node = obj->AsNode()) {
                    if (auto controller = node->GetController((RE::NiRTTI*) RE::NiRTTI_NiInterpController.address())) {
                        retval = callback((RE::NiInterpController*) controller);
                    }
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
                    if (auto controller = node->GetController((RE::NiRTTI*) RE::NiRTTI_NiInterpController.address())) {
                        retval=callback((RE::NiInterpController*)controller);
                    }
                }
            }
        }  
        return retval;
    }
    bool SetNiTransformFrequency(RE::StaticFunctionTag*, RE::Actor* actor, RE::TESObjectARMO* armor, RE::BSFixedString nodeName,
                                 float frequency) {
        return NiTransformOperation(actor, armor, nodeName, [frequency](RE::NiInterpController* controller) {
            controller->frequency = frequency;
            return true;
        });
    }



    bool SetNiTransformStart(RE::StaticFunctionTag*, RE::Actor* actor, RE::TESObjectARMO* armor, RE::BSFixedString nodeName) {
        return NiTransformOperation(actor, armor, nodeName, [](RE::NiInterpController* controller) {
                auto update_fn = (void (*)(RE::NiInterpController* c, float* t))((*(uint64_t**) controller)[0x27]);
                {   
                    float * real_world_time_ptr=(float*)REL::RelocationID(517597, 400912, 517597).address();
                    float time_per_anim = ((controller->hiKeyTime - controller->loKeyTime));
                    
                    if (controller->frequency != 0.0) {
                        controller->phase =
                            fmodf(controller->phase + time_per_anim - controller->ComputeScaledTime(*real_world_time_ptr), time_per_anim);
                    }
                    controller->flags.set(RE::NiInterpController::Flag::kActive);
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
    bool SetNiTransformStop(RE::StaticFunctionTag*, RE::Actor* actor, RE::TESObjectARMO* armor, RE::BSFixedString nodeName) {
        return NiTransformOperation(actor, armor, nodeName, [](RE::NiInterpController* controller) {
            auto update_fn = (void (*)(RE::NiInterpController* c, float* t))((*(uint64_t**) controller)[0x27]);
            {
                float* real_world_time_ptr = (float*) REL::RelocationID(517597, 400912, 517597).address();
                float time_per_anim = ((controller->hiKeyTime - controller->loKeyTime));
                if (controller->frequency != 0.0) {
                    controller->phase =
                        fmodf(controller->phase + (time_per_anim-0.0001f) + (time_per_anim - controller->ComputeScaledTime(*real_world_time_ptr)), time_per_anim);
                }
                if (auto interpolator = controller->GetInterpolator()) {
                    NiQuatTransform t;
                    //RE::NiQuatTransform* t_ptr = (RE::NiQuatTransform*) &t;
                    //interpolator->lastTime = controller->hiKeyTime;
                    //if (controller->target && controller->target->AsNode()) {
                    //interpolator->Update1(0.0f, controller->target, *t_ptr);
                    //}
                    //interpolator->lastTime = controller->hiKeyTime;

                    float tm = *real_world_time_ptr;
                    if (controller->flags.any(RE::NiTimeController::Flag::kForceUpdate)) {
                        update_fn(controller, &tm);
                    } else {
                        controller->flags.set(RE::NiTimeController::Flag::kForceUpdate);
                        update_fn(controller, &tm);
                        controller->flags.reset(RE::NiTimeController::Flag::kForceUpdate);
                    }
                }
                controller->flags.reset(RE::NiInterpController::Flag::kActive);

                return true;
            }
        });
    }
    bool SetNiTransformPause(RE::StaticFunctionTag*, RE::Actor* actor, RE::TESObjectARMO* armor, RE::BSFixedString nodeName, bool paused) {
        return NiTransformOperation(actor, armor, nodeName, [paused](RE::NiInterpController* controller) { 
            //auto update_fn = (void (*)(RE::NiInterpController* c, float* t))((*(uint64_t**) controller)[0x27]);

            float* real_world_time_ptr = (float*) REL::RelocationID(517597, 400912, 517597).address();
            float time_per_anim = ((controller->hiKeyTime - controller->loKeyTime));
            auto lastscaledtime = 0.0f;
            //if (auto interpolator = controller->GetInterpolator()) {
                lastscaledtime = controller->scaledTime;
            //}

            if (paused) {
                controller->flags.reset(RE::NiInterpController::Flag::kActive);
            } else {
                if (!controller->flags.any(RE::NiInterpController::Flag::kActive)) {
                    //float time_diff = (*real_world_time_ptr - controller->lastTime);
                    if (controller->frequency != 0.0) {
                        auto original_scaled_phase=fmodf((controller->phase + (lastscaledtime)),time_per_anim);
                        auto new_scaled_phase = fmodf((controller->phase + (controller->ComputeScaledTime(*real_world_time_ptr))), time_per_anim);
                        controller->phase = fmodf(controller->phase + (original_scaled_phase - new_scaled_phase), time_per_anim);
                        //controller->phase = fmodf(time_diff, 1.0f / controller->frequency);
                    }
                }
                controller->flags.set(RE::NiInterpController::Flag::kActive);
                
            }
            return true;
        });
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
        RE::SkyrimVM::GetSingleton()->impl->RegisterFunction("SetNiTransformFrequency", "NiTransformPlugin", SetNiTransformFrequency,
                                                             false);
        RE::SkyrimVM::GetSingleton()->impl->RegisterFunction("SetNiTransformStart", "NiTransformPlugin", SetNiTransformStart,
                                                             false);
        RE::SkyrimVM::GetSingleton()->impl->RegisterFunction("SetNiTransformStop", "NiTransformPlugin", SetNiTransformStop, false);
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