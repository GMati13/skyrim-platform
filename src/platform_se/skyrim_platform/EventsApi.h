#pragma once
#include "JsEngine.h"
#include <RE/TESObjectREFR.h>

class SKSETaskInterface;
struct EventsGlobalState;

namespace EventsApi {
JsValue On(const JsFunctionArguments& args);
JsValue Once(const JsFunctionArguments& args);

void SendEvent(const char* eventName, const std::vector<JsValue>& arguments);
void Clear();

std::pair<EventsGlobalState* , uint32_t> GetEventsGlobalState();
void SetEventsGlobalState(const EventsGlobalState* newState);

  // Exceptions will be pushed to g_taskQueue
void SendAnimationEventEnter(uint32_t selfId,
                             std::string& animEventName) noexcept;
void SendAnimationEventLeave(bool animationSucceeded) noexcept;

JsValue GetHooks();

inline void Register(JsValue& exports)
{
  exports.SetProperty("on", JsValue::Function(On));
  exports.SetProperty("once", JsValue::Function(Once));
  exports.SetProperty("hooks", GetHooks());
}
}