#pragma once
#include "JsEngine.h"

namespace EquipmentApi {
JsValue CloneWeapoForLeftHand(const JsFunctionArguments& args);

inline void Register(JsValue& exports)
{
  exports.SetProperty("cloneWeapoForLeftHand",
                      JsValue::Function(CloneWeapoForLeftHand));
}
}
