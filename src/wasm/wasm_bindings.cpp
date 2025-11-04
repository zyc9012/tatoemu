#include "wasm/emulator_wasm.h"
#include <emscripten/bind.h>
#include <emscripten/val.h>

using namespace emscripten;

// Wrapper class to handle JavaScript interactions
class EmulatorWrapper {
public:
    EmulatorWrapper() {
        m_emulator = std::make_unique<EmulatorWasm>();
    }

    bool initialize() {
        return m_emulator->initialize();
    }

    bool loadROM(const std::string& base64Data) {
        // JavaScript will pass ROM data as a typed array
        // We'll use a different approach - expose a method that takes a memory view
        (void)base64Data; // Suppress unused parameter warning
        return false;
    }

    bool loadROMFromTypedArray(uintptr_t dataPtr, size_t size) {
        const u8* data = reinterpret_cast<const u8*>(dataPtr);
        return m_emulator->loadROMFromMemory(data, size);
    }

    bool loadBootromFromTypedArray(uintptr_t dataPtr, size_t size) {
        const u8* data = reinterpret_cast<const u8*>(dataPtr);
        return m_emulator->loadBootromFromMemory(data, size);
    }

    void runFrame() {
        m_emulator->runFrame();
    }

    void pressButton(u8 button) {
        m_emulator->pressButton(button);
    }

    void releaseButton(u8 button) {
        m_emulator->releaseButton(button);
    }

    uintptr_t getFrameBufferPtr() {
        return reinterpret_cast<uintptr_t>(m_emulator->getFrameBuffer());
    }

    int getFrameBufferSize() {
        return SCREEN_WIDTH * SCREEN_HEIGHT;
    }

    int getScreenWidth() {
        return SCREEN_WIDTH;
    }

    int getScreenHeight() {
        return SCREEN_HEIGHT;
    }

    // Audio methods
    val getAudioSamples(int maxSamples) {
        std::vector<float> buffer(maxSamples);
        int samplesRead = m_emulator->getAudioSamples(buffer.data(), maxSamples);
        
        // Return only the samples that were actually read
        if (samplesRead > 0) {
            return val(typed_memory_view(samplesRead, buffer.data()));
        }
        return val::null();
    }

    void clearAudioBuffer() {
        m_emulator->clearAudioBuffer();
    }

    int getQueuedAudioSize() {
        return m_emulator->getQueuedAudioSize();
    }

    std::string getCartridgeTitle() {
        return m_emulator->getCartridgeTitle();
    }

    bool isRunning() {
        return m_emulator->isRunning();
    }

private:
    std::unique_ptr<EmulatorWasm> m_emulator;
};

// Emscripten bindings
EMSCRIPTEN_BINDINGS(gb_emulator) {
    class_<EmulatorWrapper>("Emulator")
        .constructor<>()
        .function("initialize", &EmulatorWrapper::initialize)
        .function("loadROMFromTypedArray", &EmulatorWrapper::loadROMFromTypedArray)
        .function("loadBootromFromTypedArray", &EmulatorWrapper::loadBootromFromTypedArray)
        .function("runFrame", &EmulatorWrapper::runFrame)
        .function("pressButton", &EmulatorWrapper::pressButton)
        .function("releaseButton", &EmulatorWrapper::releaseButton)
        .function("getFrameBufferPtr", &EmulatorWrapper::getFrameBufferPtr)
        .function("getFrameBufferSize", &EmulatorWrapper::getFrameBufferSize)
        .function("getScreenWidth", &EmulatorWrapper::getScreenWidth)
        .function("getScreenHeight", &EmulatorWrapper::getScreenHeight)
        .function("getAudioSamples", &EmulatorWrapper::getAudioSamples)
        .function("clearAudioBuffer", &EmulatorWrapper::clearAudioBuffer)
        .function("getQueuedAudioSize", &EmulatorWrapper::getQueuedAudioSize)
        .function("getCartridgeTitle", &EmulatorWrapper::getCartridgeTitle)
        .function("isRunning", &EmulatorWrapper::isRunning);

    // Button constants
    constant("BUTTON_RIGHT", BUTTON_RIGHT);
    constant("BUTTON_LEFT", BUTTON_LEFT);
    constant("BUTTON_UP", BUTTON_UP);
    constant("BUTTON_DOWN", BUTTON_DOWN);
    constant("BUTTON_A", BUTTON_A);
    constant("BUTTON_B", BUTTON_B);
    constant("BUTTON_SELECT", BUTTON_SELECT);
    constant("BUTTON_START", BUTTON_START);
}

