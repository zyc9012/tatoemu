let emulator = null;
let isRunning = false;
let animationFrameId = null;
let audioContext = null;
let audioWorkletNode = null;
let audioBufferQueue = [];
let ModuleInstance = null;

// Frame timing
let lastFrameTime = 0;
let frameAccumulator = 0;

const canvas = document.getElementById('canvas');
const ctx = canvas.getContext('2d');
const statusEl = document.getElementById('status');

// Create an ImageData object for faster rendering
const imageData = ctx.createImageData(160, 144);

function setStatus(message, type = 'info') {
    statusEl.textContent = message;
    statusEl.className = type === 'error' ? 'error' : type === 'success' ? 'success' : '';
}

// Initialize the WASM module
console.log('Loading WASM module...');
Module().then(function(Module) {
    console.log('WASM module loaded');
    ModuleInstance = Module;
    
    try {
        emulator = new Module.Emulator();
        if (emulator.initialize()) {
            console.log('Emulator initialized successfully');
            setStatus('Emulator ready - Load a ROM to start', 'success');
        } else {
            setStatus('Failed to initialize emulator', 'error');
        }
    } catch (e) {
        console.error('Error initializing emulator:', e);
        setStatus('Error: ' + e.message, 'error');
    }
}).catch(function(err) {
    console.error('Failed to load WASM module:', err);
    setStatus('Failed to load emulator: ' + err.message, 'error');
});

// Initialize Web Audio API
async function initAudio() {
    if (audioContext) return;
    
    try {
        audioContext = new (window.AudioContext || window.webkitAudioContext)({
            sampleRate: ModuleInstance.SAMPLE_RATE,
            latencyHint: 'interactive'
        });
        
        console.log('Audio context initialized');
    } catch (e) {
        console.error('Failed to initialize audio:', e);
    }
}

// Simple audio playback using ScriptProcessorNode (deprecated but widely supported)
function startAudioPlayback() {
    if (!audioContext) return;
    
    // Create a script processor node
    const bufferSize = 2048;
    const processor = audioContext.createScriptProcessor(bufferSize, 0, 2);
    
    processor.onaudioprocess = function(e) {
        if (!emulator || !isRunning) return;
        
        const outputL = e.outputBuffer.getChannelData(0);
        const outputR = e.outputBuffer.getChannelData(1);
        
        // Get audio samples from the emulator
        const maxSamples = bufferSize * 2; // stereo
        const samples = emulator.getAudioSamples(maxSamples);
        
        if (samples && samples.length > 0) {
            // Deinterleave stereo samples
            for (let i = 0; i < bufferSize; i++) {
                if (i * 2 < samples.length) {
                    outputL[i] = samples[i * 2];
                    outputR[i] = samples[i * 2 + 1];
                } else {
                    outputL[i] = 0;
                    outputR[i] = 0;
                }
            }
        } else {
            // Silence
            outputL.fill(0);
            outputR.fill(0);
        }
    };
    
    processor.connect(audioContext.destination);
    audioWorkletNode = processor;
}

// Connect buttons to file inputs
document.getElementById('loadRomBtn').addEventListener('click', function() {
    document.getElementById('romFile').click();
});

document.getElementById('loadBootromBtn').addEventListener('click', function() {
    document.getElementById('bootromFile').click();
});

// ROM file loader
document.getElementById('romFile').addEventListener('change', async function(e) {
    const file = e.target.files[0];
    if (!file) return;
    
    // Check if module and emulator are ready
    if (!ModuleInstance || !emulator) {
        setStatus('Emulator not ready yet, please wait...', 'error');
        return;
    }
    
    try {
        setStatus('Loading ROM...', 'info');
        
        const arrayBuffer = await file.arrayBuffer();
        const uint8Array = new Uint8Array(arrayBuffer);
        
        // Allocate memory in WASM heap
        const dataPtr = ModuleInstance._malloc(uint8Array.length);
        ModuleInstance.HEAPU8.set(uint8Array, dataPtr);
        
        // Load ROM
        const success = emulator.loadROMFromTypedArray(dataPtr, uint8Array.length);
        
        // Free the allocated memory
        ModuleInstance._free(dataPtr);

        // Unfocus the button
        document.getElementById('loadRomBtn').blur();
        
        if (success) {
            const title = emulator.getCartridgeTitle();
            setStatus('ROM loaded: ' + title, 'success');
            
            // Initialize and start audio
            await initAudio();
            if (audioContext.state === 'suspended') {
                await audioContext.resume();
            }
            startAudioPlayback();
            
            // Start emulation
            startEmulation();
        } else {
            setStatus('Failed to load ROM', 'error');
        }
    } catch (e) {
        console.error('Error loading ROM:', e);
        setStatus('Error: ' + e.message, 'error');
    }
});

// Bootrom file loader
document.getElementById('bootromFile').addEventListener('change', async function(e) {
    const file = e.target.files[0];
    if (!file) return;
    
    // Check if module and emulator are ready
    if (!ModuleInstance || !emulator) {
        setStatus('Emulator not ready yet, please wait...', 'error');
        return;
    }
    
    try {
        setStatus('Loading Bootrom...', 'info');
        
        const arrayBuffer = await file.arrayBuffer();
        const uint8Array = new Uint8Array(arrayBuffer);
        
        // Allocate memory in WASM heap
        const dataPtr = ModuleInstance._malloc(uint8Array.length);
        ModuleInstance.HEAPU8.set(uint8Array, dataPtr);
        
        // Load Bootrom
        const success = emulator.loadBootromFromTypedArray(dataPtr, uint8Array.length);
        
        // Free the allocated memory
        ModuleInstance._free(dataPtr);

        // Unfocus the button
        document.getElementById('loadBootromBtn').blur();
        
        if (success) {
            setStatus('Bootrom loaded successfully', 'success');
        } else {
            setStatus('Failed to load bootrom', 'error');
        }
    } catch (e) {
        console.error('Error loading bootrom:', e);
        setStatus('Error: ' + e.message, 'error');
    }
});

// Main emulation loop
function emulationLoop(currentTime) {
    if (!isRunning || !emulator) return;
    
    // Initialize lastFrameTime on first run
    if (lastFrameTime === 0) {
        lastFrameTime = currentTime;
    }
    
    // Calculate time delta
    const deltaTime = currentTime - lastFrameTime;
    lastFrameTime = currentTime;
    
    // Accumulate time
    frameAccumulator += deltaTime;

    const targetFrameTime = 1000 / ModuleInstance.TARGET_FPS;
    
    // Run frames based on accumulated time
    // This ensures the game runs at the target FPS regardless of display refresh rate
    while (frameAccumulator >= targetFrameTime) {
        try {
            // Run one frame
            emulator.runFrame();
            
            // Get frame buffer and render to canvas
            const frameBufferPtr = emulator.getFrameBufferPtr();
            const frameBufferSize = emulator.getFrameBufferSize();
            
            // Create a view of the frame buffer (RGBA format)
            const frameBuffer = new Uint32Array(
                ModuleInstance.HEAPU32.buffer,
                frameBufferPtr,
                frameBufferSize
            );
            
            // Copy to ImageData
            const data32 = new Uint32Array(imageData.data.buffer);
            data32.set(frameBuffer);
            
            // Render to canvas
            ctx.putImageData(imageData, 0, 0);
            
        } catch (e) {
            console.error('Error in emulation loop:', e);
            stopEmulation();
            setStatus('Emulation error: ' + e.message, 'error');
            return;
        }
        
        // Subtract frame time from accumulator
        frameAccumulator -= targetFrameTime;
        
        // Prevent spiral of death - if we fall too far behind, reset
        if (frameAccumulator > targetFrameTime * 3) {
            frameAccumulator = 0;
        }
    }
    
    // Continue loop
    animationFrameId = requestAnimationFrame(emulationLoop);
}

function startEmulation() {
    if (isRunning) return;
    
    isRunning = true;
    console.log('Starting emulation');
    emulationLoop(performance.now());
}

function stopEmulation() {
    isRunning = false;
    if (animationFrameId) {
        cancelAnimationFrame(animationFrameId);
        animationFrameId = null;
    }
    // Reset timing variables
    lastFrameTime = 0;
    frameAccumulator = 0;
    console.log('Stopped emulation');
}

// Keyboard input handling
let keyMap = null;

function initKeyMap() {
    if (ModuleInstance && !keyMap) {
        keyMap = {
            'ArrowUp': ModuleInstance.BUTTON_UP,
            'ArrowDown': ModuleInstance.BUTTON_DOWN,
            'ArrowLeft': ModuleInstance.BUTTON_LEFT,
            'ArrowRight': ModuleInstance.BUTTON_RIGHT,
            'z': ModuleInstance.BUTTON_A,
            'Z': ModuleInstance.BUTTON_A,
            'x': ModuleInstance.BUTTON_B,
            'X': ModuleInstance.BUTTON_B,
            'Enter': ModuleInstance.BUTTON_START,
            'Shift': ModuleInstance.BUTTON_SELECT
        };
    }
}

document.addEventListener('keydown', function(e) {
    if (!emulator || !isRunning) return;
    
    initKeyMap();
    const button = keyMap ? keyMap[e.key] : undefined;
    if (button !== undefined) {
        e.preventDefault();
        emulator.pressButton(button);
    }
});

document.addEventListener('keyup', function(e) {
    if (!emulator || !isRunning) return;
    
    initKeyMap();
    const button = keyMap ? keyMap[e.key] : undefined;
    if (button !== undefined) {
        e.preventDefault();
        emulator.releaseButton(button);
    }
});

// Handle page visibility changes
document.addEventListener('visibilitychange', function() {
    if (document.hidden) {
        // Page is hidden, pause audio context
        if (audioContext && audioContext.state === 'running') {
            audioContext.suspend();
        }
    } else {
        // Page is visible again, resume audio context
        if (audioContext && audioContext.state === 'suspended') {
            audioContext.resume();
        }
    }
});

