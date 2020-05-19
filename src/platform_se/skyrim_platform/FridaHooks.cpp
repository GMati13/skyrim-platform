/*
 * To build, set up your Release configuration like this:
 *
 * [Runtime Library]
 * Multi-threaded (/MT)
 *
 * Visit www.frida.re to learn more about Frida.
 */

#include <frida/frida-gum.h>

#include "EventsApi.h"
#include "PapyrusTESModPlatform.h"
#include "PatternScanner.h"
#include "StringHolder.h"
#include <RE/ConsoleLog.h>
#include <RE/TESObjectREFR.h>
#include <sstream>
#include <windows.h>

typedef struct _ExampleListener ExampleListener;
typedef enum _ExampleHookId ExampleHookId;

struct _ExampleListener
{
  GObject parent;
};

enum _ExampleHookId
{
  HOOK_SEND_ANIMATION_EVENT,
  DRAW_SHEATHE_WEAPON_ACTOR,
  DRAW_SHEATHE_WEAPON_PC,
  SET_ANIMATION_VARIABLE_FLOAT
};

static void example_listener_iface_init(gpointer g_iface, gpointer iface_data);

#define EXAMPLE_TYPE_LISTENER (example_listener_get_type())
G_DECLARE_FINAL_TYPE(ExampleListener, example_listener, EXAMPLE, LISTENER,
                     GObject)
G_DEFINE_TYPE_EXTENDED(ExampleListener, example_listener, G_TYPE_OBJECT, 0,
                       G_IMPLEMENT_INTERFACE(GUM_TYPE_INVOCATION_LISTENER,
                                             example_listener_iface_init))

static GumInterceptor* g_interceptor = nullptr;
static GumInvocationListener* g_listener = nullptr;
static PatternScanner::MemoryRegion* g_mr = nullptr;

void Intercept(int offset, _ExampleHookId hookId)
{
  int r = gum_interceptor_attach(g_interceptor,
                                 (void*)(REL::Module::BaseAddr() + offset),
                                 g_listener, GSIZE_TO_POINTER(hookId));

  if (GUM_ATTACH_OK != r) {
    char buf[1025];
    sprintf_s(buf, "Interceptor failed with %d trying to hook %d", int(r),
              offset);
    MessageBox(0, buf, "Error", MB_ICONERROR);
  }
}

void SetupFridaHooks()
{
  gum_init_embedded();

  g_interceptor = gum_interceptor_obtain();
  g_listener =
    (GumInvocationListener*)g_object_new(EXAMPLE_TYPE_LISTENER, NULL);
  g_mr = new PatternScanner::MemoryRegion;
  PatternScanner::GetSkyrimMemoryRegion(g_mr);

  int r;

  gum_interceptor_begin_transaction(g_interceptor);

  Intercept(6353472, HOOK_SEND_ANIMATION_EVENT);
  Intercept(6104992, DRAW_SHEATHE_WEAPON_ACTOR);
  Intercept(7141008, DRAW_SHEATHE_WEAPON_PC);

  auto ptr_SetAnimationVariableFloat = PatternScanner::PatternScanInternal(
    g_mr,
    std::vector<BYTE>{ 0xF3, 0x0F, 0x11, 0x54, 0x24, 0x18, 0x48, 0x83, 0xEC,
                       0x28, 0x4C, 0x8D });
  Intercept((size_t)ptr_SetAnimationVariableFloat -
              (size_t)REL::Module::BaseAddr(),
            SET_ANIMATION_VARIABLE_FLOAT);

  gum_interceptor_end_transaction(g_interceptor);
}

static void example_listener_on_enter(GumInvocationListener* listener,
                                      GumInvocationContext* ic)
{
  ExampleListener* self = EXAMPLE_LISTENER(listener);
  auto hook_id = gum_invocation_context_get_listener_function_data(ic);

  auto _ic = (_GumInvocationContext*)ic;

  switch ((size_t)hook_id) {
    case DRAW_SHEATHE_WEAPON_PC: {
      auto refr =
        _ic->cpu_context->rcx ? (RE::Actor*)(_ic->cpu_context->rcx) : nullptr;
      uint32_t formId = refr ? refr->formID : 0;

      union
      {
        size_t draw;
        uint8_t byte[8];
      };

      draw = (size_t)gum_invocation_context_get_nth_argument(ic, 1);

      auto falseValue = gpointer(*byte ? draw - 1 : draw);
      auto trueValue = gpointer(*byte ? draw : draw + 1);

      auto mode = TESModPlatform::GetWeapDrawnMode(formId);
      if (mode == TESModPlatform::WEAP_DRAWN_MODE_ALWAYS_TRUE) {
        gum_invocation_context_replace_nth_argument(ic, 1, trueValue);
      } else if (mode == TESModPlatform::WEAP_DRAWN_MODE_ALWAYS_FALSE) {
        gum_invocation_context_replace_nth_argument(ic, 1, falseValue);
      }
      break;
    }
    case DRAW_SHEATHE_WEAPON_ACTOR: {
      auto refr =
        _ic->cpu_context->rcx ? (RE::Actor*)(_ic->cpu_context->rcx) : nullptr;
      uint32_t formId = refr ? refr->formID : 0;

      auto draw = (uint32_t*)gum_invocation_context_get_nth_argument(ic, 1);

      auto mode = TESModPlatform::GetWeapDrawnMode(formId);
      if (mode == TESModPlatform::WEAP_DRAWN_MODE_ALWAYS_TRUE) {
        gum_invocation_context_replace_nth_argument(ic, 1, gpointer(1));
      } else if (mode == TESModPlatform::WEAP_DRAWN_MODE_ALWAYS_FALSE) {
        gum_invocation_context_replace_nth_argument(ic, 1, gpointer(0));
      }
      break;
    }
    case HOOK_SEND_ANIMATION_EVENT: {
      auto refr = _ic->cpu_context->rcx
        ? (RE::TESObjectREFR*)(_ic->cpu_context->rcx - 0x38)
        : nullptr;
      uint32_t formId = refr ? refr->formID : 0;

      constexpr int argIdx = 1;
      auto animEventName =
        (char**)gum_invocation_context_get_nth_argument(ic, argIdx);

      if (!refr || !animEventName)
        break;

      std::string str = *animEventName;
      EventsApi::SendAnimationEventEnter(formId, str);
      if (str != *animEventName) {
        auto fs = const_cast<RE::BSFixedString*>(
          &StringHolder::ThreadSingleton()[str]);
        auto newAnimEventName = reinterpret_cast<char**>(fs);
        gum_invocation_context_replace_nth_argument(ic, argIdx,
                                                    newAnimEventName);
      }
      break;
    }
    case SET_ANIMATION_VARIABLE_FLOAT: {
      auto refr = _ic->cpu_context->rcx
        ? (RE::TESObjectREFR*)(_ic->cpu_context->rcx - 0x38)
        : nullptr;
      uint32_t formId = refr ? refr->formID : 0;

      std::vector<gpointer> v;

      for (int i = 0; i < 5; ++i) {
        v.push_back(gum_invocation_context_get_nth_argument(ic, i));
      }
      if (0x14 != formId)
        RE::ConsoleLog::GetSingleton()->Print(
          "SET_ANIMATION_VARIABLE_FLOAT %x %p %s %f %p %p", formId, v[0],
          *(char**)v[1], v[2], v[3], v[4]);

      /*auto name = (char**)gum_invocation_context_get_nth_argument(ic, 2);
      auto valRaw = gum_invocation_context_get_nth_argument(ic, 3);
      float* value = reinterpret_cast<float*>(&valRaw);

      if (!name || !value)
        break;

      RE::ConsoleLog::GetSingleton()->Print(
        "SET_ANIMATION_VARIABLE_FLOAT %s %f", *name, *value);*/
      break;
    }
  }
}

static void example_listener_on_leave(GumInvocationListener* listener,
                                      GumInvocationContext* ic)
{
  ExampleListener* self = EXAMPLE_LISTENER(listener);
  auto hook_id = gum_invocation_context_get_listener_function_data(ic);

  switch ((size_t)hook_id) {
    case HOOK_SEND_ANIMATION_EVENT:
      bool res = !!gum_invocation_context_get_return_value(ic);
      EventsApi::SendAnimationEventLeave(res);
      break;
  }
}

static void example_listener_class_init(ExampleListenerClass* klass)
{
  (void)EXAMPLE_IS_LISTENER;
#ifndef _MSC_VER
  (void)glib_autoptr_cleanup_ExampleListener;
#endif
}

static void example_listener_iface_init(gpointer g_iface, gpointer iface_data)
{
  auto iface = (GumInvocationListenerInterface*)g_iface;

  iface->on_enter = example_listener_on_enter;
  iface->on_leave = example_listener_on_leave;
}

static void example_listener_init(ExampleListener* self)
{
}
