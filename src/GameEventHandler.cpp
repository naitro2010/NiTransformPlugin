#include "GameEventHandler.h"
#include "Hooks.h"

namespace plugin {
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
        std::function<bool(RE::NiTimeController*)> callback) 
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
                    if (auto controller = node->GetController((RE::NiRTTI*) RE::NiRTTI_NiTransformController.address())) {
                        retval=callback(controller);
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
                    if (auto controller = node->GetController((RE::NiRTTI*) RE::NiRTTI_NiTransformController.address())) {
                        retval=callback(controller);
                    }
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
                    controller->lastTime = 0.0;
                    controller->flags.set(RE::NiTimeController::Flag::kActive);
                    controller->startTime = controller->lastTime;
                    controller->weightedLastTime = 0.0;
                    controller->scaledTime = controller->ComputeScaledTime(controller->startTime); 
                    float t = controller->startTime;
                    update_fn(controller, &t);
                    controller->weightedLastTime = 0.0;
                    
                    controller->startTime = controller->lastTime;
                }
                return true;
        });
    }
    bool SetNiTransformStop(RE::StaticFunctionTag*, RE::Actor* actor, RE::TESObjectARMO* armor, RE::BSFixedString nodeName) {
        return NiTransformOperation(actor, armor, nodeName, [](RE::NiTimeController* controller) {
            auto update_fn = (void (*)(RE::NiTimeController* c, float* t))((*(uint64_t**) controller)[0x27]);
            {
                {
                    controller->lastTime = 0.0;
                    controller->flags.set(RE::NiTimeController::Flag::kActive);
                    controller->startTime = controller->lastTime;
                    controller->weightedLastTime = 0.0;
                    controller->scaledTime = controller->ComputeScaledTime(controller->startTime); 
                    float t = controller->startTime;
                    update_fn(controller, &t);
                    controller->scaledTime = 0.0f;
                    controller->startTime = controller->lastTime;
                }
                controller->flags.reset(RE::NiTimeController::Flag::kActive);
            }
            return true;
        });
    }
    bool SetNiTransformPause(RE::StaticFunctionTag*, RE::Actor* actor, RE::TESObjectARMO* armor, RE::BSFixedString nodeName, bool paused) {
        return NiTransformOperation(actor, armor, nodeName, [paused](RE::NiTimeController* controller) { 
            //auto update_fn = (void (*)(RE::NiTimeController* c, float* t))((*(uint64_t**) controller)[0x27]);
            if (paused) {
                controller->flags.reset(RE::NiTimeController::Flag::kActive);
            } else {
                controller->flags.set(RE::NiTimeController::Flag::kActive);
                
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