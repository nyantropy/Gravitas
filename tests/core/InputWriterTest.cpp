#include "InputWriter.hpp"
#include "InputBindingRegistry.h"
#include <stdexcept>

#if __has_include("GtsPlatform.h") || __has_include("IGtsGraphicsModule.hpp")
#error "Raw input must compile with core alone"
#endif

template <typename T> concept PublicFrameWrite = requires(T& value) { value.beginFrame(); };
template <typename T> concept PublicKeyWrite = requires(T& value) { value.onKeyEvent(GtsKey::A, true, 0); };
template <typename T> concept PublicMouseWrite = requires(T& value) { value.onMouseButtonEvent(0, true, 0); };
template <typename T> concept PublicCursorWrite = requires(T& value) { value.onCursorPositionEvent(0, 0); };
template <typename T> concept PublicScrollWrite = requires(T& value) { value.onScrollEvent(0, 0); };
template <typename T> concept PublicReset = requires(T& value) { value.reset(); };
static_assert(!PublicFrameWrite<InputManager> && !PublicKeyWrite<InputManager> &&
              !PublicMouseWrite<InputManager> && !PublicCursorWrite<InputManager> && !PublicScrollWrite<InputManager>);
static_assert(PublicFrameWrite<InputWriter> && PublicKeyWrite<InputWriter> &&
              PublicMouseWrite<InputWriter> && PublicCursorWrite<InputWriter> && PublicScrollWrite<InputWriter>);
static_assert(!PublicReset<InputWriter>);

void require(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}

int main()
{
    InputManager input;
    InputManager other;
    InputWriter writer(input);
    InputWriter otherWriter(other);
    writer.beginFrame();
    writer.onKeyEvent(GtsKey::A, true, 15);
    require(input.isKeyDown(GtsKey::A) && input.isKeyPressed(GtsKey::A) && !input.isKeyReleased(GtsKey::A), "Press");
    require(input.getModifiers() == (ModifierFlags::Shift | ModifierFlags::Ctrl | ModifierFlags::Alt | ModifierFlags::Super),
            "Modifier translation");
    writer.onKeyEvent(GtsKey::A, false, 0);
    require(!input.isKeyDown(GtsKey::A) && input.isKeyPressed(GtsKey::A) && input.isKeyReleased(GtsKey::A), "Short tap");
    writer.onMouseButtonEvent(7, true, 2);
    writer.onMouseButtonEvent(7, false, 2);
    require(input.isMouseButtonPressed(7) && input.isMouseButtonReleased(7) && !input.isMouseButtonDown(7), "Mouse tap");
    require(input.getModifiers() == ModifierFlags::Ctrl, "Mouse updates modifiers");
    writer.onCursorPositionEvent(12, 34);
    writer.onScrollEvent(1, -2);
    writer.onScrollEvent(3, 1);
    require(input.mouseX() == 12 && input.mouseY() == 34 && input.scrollX() == 4 && input.scrollY() == -1, "Position/scroll");
    writer.beginFrame();
    require(!input.isKeyPressed(GtsKey::A) && !input.isKeyReleased(GtsKey::A) &&
            !input.isMouseButtonPressed(7) && !input.isMouseButtonReleased(7) && input.scrollX() == 0 && input.scrollY() == 0,
            "Frame clears edges and scroll");
    require(input.mouseX() == 12 && input.getModifiers() == ModifierFlags::Ctrl, "Frame retains position/modifiers");
    writer.onKeyEvent(GtsKey::B, true, 0);
    writer.onMouseButtonEvent(0, true, 0);
    writer.beginFrame();
    writer.onKeyEvent(GtsKey::B, true, 0);
    writer.onMouseButtonEvent(0, true, 0);
    require(input.isKeyDown(GtsKey::B) && !input.isKeyPressed(GtsKey::B) &&
            input.isMouseButtonDown(0) && !input.isMouseButtonPressed(0), "Repeats do not create edges");
    writer.onKeyEvent(static_cast<GtsKey>(-1), true, 4);
    writer.onKeyEvent(GtsKey::COUNT, true, 4);
    writer.onMouseButtonEvent(-1, true, 8);
    writer.onMouseButtonEvent(8, true, 8);
    require(!input.isKeyDown(GtsKey::COUNT) && !input.isKeyPressed(static_cast<GtsKey>(-1)) &&
            !input.isMouseButtonDown(-1) && !input.isMouseButtonPressed(8) && input.getModifiers() == ModifierFlags::Super,
            "Out-of-range events still update modifiers without indexing state");
    require(!other.isKeyDown(GtsKey::B) && other.mouseX() == 0, "Managers remain isolated");
    otherWriter.onKeyEvent(GtsKey::C, true, 0);
    input.reset();
    require(!input.isKeyDown(GtsKey::B) && !input.isMouseButtonDown(0) && input.mouseX() == 0 &&
            input.getModifiers() == ModifierFlags::None && other.isKeyDown(GtsKey::C), "Reset affects only owner");
    writer.beginFrame();
    writer.onKeyEvent(GtsKey::A, true, 0);
    InputBindingRegistry bindings;
    bindings.bind("press", {InputTrigger::Type::Key, static_cast<int>(GtsKey::A)});
    bindings.update(InputSnapshot{&input});
    require(bindings.isPressed("press") && bindings.isSimulationPressed("press"), "Core-only writer to binding path");
    writer.beginFrame();
    bindings.update(InputSnapshot{&input});
    require(!bindings.isPressed("press") && bindings.isSimulationPressed("press"), "Tick edge outlives frame edge");
    bindings.finishSimulationTick();
    require(!bindings.isSimulationPressed("press"), "Tick consumes queued edge");
}
