#pragma once
#include "EquipmentApi.h"
#include <RE/TESObjectWEAP.h>
#include <map>
#include <skse64\GameForms.h>

JsValue EquipmentApi::CloneWeapoForLeftHand(const JsFunctionArguments& args)
{
  auto weaponId = (double)args[1];
  JsValue result = JsValue::Undefined();

  static std::map<uint32_t, uint32_t> weaponStorage;
  static auto factory =
    IFormFactory::GetFactoryForType((uint32_t)RE::TESObjectWEAP::FORMTYPE);
  if (!factory) {
    throw std::runtime_error("IFormFactory from weapon == nullptr");
  }

  if (weaponId) {
    auto stor = weaponStorage.find(weaponId);

    if (stor != weaponStorage.end()) {
      result = JsValue::Double(stor->second);
    } else {

      auto createdWeapon = (RE::TESObjectWEAP*)factory->Create();
      if (!createdWeapon) {
        throw std::runtime_error("IFormFactory createdWeapon == nullptr");
      }

      auto weapon =
        reinterpret_cast<RE::TESObjectWEAP*>(LookupFormByID(weaponId));
      uint32_t tempIdStorage = createdWeapon->formID;

      if (!weapon) {
        throw std::runtime_error(
          "CloneWeapoForLeftHand arg weapon == nullptr");
      }

      memcpy(createdWeapon, weapon, sizeof(RE::TESObjectWEAP));
      createdWeapon->formID = tempIdStorage;
      createdWeapon->equipSlot =
        reinterpret_cast<RE::BGSEquipSlot*>(LookupFormByID(0x00013F43));

      weaponStorage[weaponId] = tempIdStorage;
      result = JsValue::Double(tempIdStorage);
    }
  }

  return result;
}
