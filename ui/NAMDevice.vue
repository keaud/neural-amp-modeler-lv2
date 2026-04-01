<template>
  <div class="nam-ui">
    <!-- Preload the off image so toggling bypass is instant -->
    <link rel="preload" as="image" href="/Factory/Devices/NAM/ui/nam-ui-off.jpg" />
    <img src="/Factory/Devices/NAM/ui/nam-ui.jpg" alt="NAM UI" class="full-screen-image" />
    <div class="bypass-overlay" :class="{ active: isBypassed }"></div>

    <div class="model-display" @click="openModelBrowser">
      <span v-if="!isBypassed" class="model-label">{{ currentModel || 'No Model Loaded' }}</span>
    </div>

    <template v-if="!isBypassed">
      <div class="neural-tube-container tube-gain">
        <NeuralTube :width="102" :height="160" :rate="normalizeValue(inputLevelValue, 'input_level')" />
      </div>

      <div class="neural-tube-container tube-bass">
        <NeuralTube :width="102" :height="160" :rate="normalizeValue(bassValue, 'bass')" />
      </div>

      <div class="neural-tube-container tube-mid">
        <NeuralTube :width="102" :height="160" :rate="normalizeValue(midValue, 'mid')" />
      </div>

      <div class="neural-tube-container tube-treble">
        <NeuralTube :width="102" :height="160" :rate="normalizeValue(trebleValue, 'treble')" />
      </div>

      <div class="neural-tube-container tube-volume">
        <NeuralTube :width="102" :height="160" :rate="normalizeValue(outputLevelValue, 'output_level')" />
      </div>
    </template>
    <div class="control-knobs">
      <UIKnob class="knob-gain touchable" :indicatorOffset="20" :size="92" :model-value="inputLevelValue" :min="inputLevelMin" :max="inputLevelMax" @change="onGainChange" />
      <UIKnob class="knob-bass touchable" :indicatorOffset="20" :size="92" :model-value="bassValue" :min="bassMin" :max="bassMax" @change="onBassChange" />
      <UIKnob class="knob-mid touchable" :indicatorOffset="20" :size="92" :model-value="midValue" :min="midMin" :max="midMax" @change="onMidChange" />
      <UIKnob class="knob-treble touchable" :indicatorOffset="20" :size="92" :model-value="trebleValue" :min="trebleMin" :max="trebleMax" @change="onTrebleChange" />
      <UIKnob class="knob-volume touchable" :indicatorOffset="20" :size="92" :model-value="outputLevelValue" :min="outputLevelMin" :max="outputLevelMax" @change="onVolumeChange" />
    </div>

    <!-- Power switch hitbox -->
    <div class="power-switch touchable" @click="toggleBypass"></div>

    <!-- File Browser Modal -->
    <Teleport to="body">
      <UIFileBrowser v-if="showModelBrowser" title="Select an Amp Model" root="/Models"
        starting-folder="/Models" extension-regex="\.nam$" @file-selected="onModelSelected"
        @close="closeModelBrowser" />
    </Teleport>
  </div>
</template>

<script setup>
import { ref, computed, onMounted, onUnmounted, watch } from 'vue';
import NeuralTube from './NeuralTube.vue';
import {
  useHardware,
  pushLedLayer,
  scaleEncoderValue,
  normalizeValue as normalizeEncoderValue,
  ledBrightness,
} from 'signalSDK';

const props = defineProps({
  instanceId: String,
  deviceInfo: Object
});

const status = ref(null);
const currentModel = ref(null);
const showModelBrowser = ref(false);
const encoderUnsubscribes = ref([]);

const sdk = window.signalSDK;
const hardware = useHardware();
let ledHandle = null;

// Preload the off image so it's cached before first bypass toggle
const offImage = new Image();
offImage.src = '/Factory/Devices/NAM/ui/nam-ui-off.jpg';

// Bypass state
const isBypassed = computed(() => {
  const device = sdk?.getDeviceInstance(props.instanceId);
  return (device?.parameterValues?.['device.bypassed'] ?? 0) >= 0.5;
});

// Get parameter values reactively from device in globalState
const inputLevelValue = computed(() => {
  const device = sdk?.getDeviceInstance(props.instanceId);
  return device?.parameterValues?.['input_level'] ?? 0;
});

const outputLevelValue = computed(() => {
  const device = sdk?.getDeviceInstance(props.instanceId);
  return device?.parameterValues?.['output_level'] ?? 0;
});

const bassValue = computed(() => {
  const device = sdk?.getDeviceInstance(props.instanceId);
  return device?.parameterValues?.['bass'] ?? 0;
});

const midValue = computed(() => {
  const device = sdk?.getDeviceInstance(props.instanceId);
  return device?.parameterValues?.['mid'] ?? 0;
});

const trebleValue = computed(() => {
  const device = sdk?.getDeviceInstance(props.instanceId);
  return device?.parameterValues?.['treble'] ?? 0;
});

// Get parameter metadata for knob ranges
function getParameterMetadata(paramName) {
  if (!sdk) return { min: 0, max: 1 };
  const systemState = sdk.getSystemState?.();
  const devices = systemState?.devices;
  if (!devices?.categories) return { min: 0, max: 1 };

  let deviceDef = null;
  for (const category of devices.categories) {
    if (category.devices) {
      deviceDef = category.devices.find(d => d.id === props.deviceInfo?.id);
      if (deviceDef) break;
    }
  }

  if (!deviceDef?.parameters?.[paramName]) {
    return { min: 0, max: 1 };
  }

  const paramDef = deviceDef.parameters[paramName];
  return {
    min: paramDef.min ?? 0,
    max: paramDef.max ?? 1
  };
}

const inputLevelMeta = computed(() => getParameterMetadata('input_level'));
const outputLevelMeta = computed(() => getParameterMetadata('output_level'));
const bassMeta = computed(() => getParameterMetadata('bass'));
const midMeta = computed(() => getParameterMetadata('mid'));
const trebleMeta = computed(() => getParameterMetadata('treble'));

const inputLevelMin = computed(() => inputLevelMeta.value?.min ?? 0);
const inputLevelMax = computed(() => inputLevelMeta.value?.max ?? 1);
const outputLevelMin = computed(() => outputLevelMeta.value?.min ?? 0);
const outputLevelMax = computed(() => outputLevelMeta.value?.max ?? 1);
const bassMin = computed(() => bassMeta.value?.min ?? -20);
const bassMax = computed(() => bassMeta.value?.max ?? 20);
const midMin = computed(() => midMeta.value?.min ?? -20);
const midMax = computed(() => midMeta.value?.max ?? 20);
const trebleMin = computed(() => trebleMeta.value?.min ?? -20);
const trebleMax = computed(() => trebleMeta.value?.max ?? 20);

// Normalize a denormalized parameter value to 0-1 range for visual feedback
function normalizeValue(value, paramName) {
  let min, max;
  switch (paramName) {
    case 'input_level':
      min = inputLevelMeta.value?.min ?? 0;
      max = inputLevelMeta.value?.max ?? 1;
      break;
    case 'output_level':
      min = outputLevelMeta.value?.min ?? 0;
      max = outputLevelMeta.value?.max ?? 1;
      break;
    case 'bass':
      min = bassMeta.value?.min ?? -20;
      max = bassMeta.value?.max ?? 20;
      break;
    case 'mid':
      min = midMeta.value?.min ?? -20;
      max = midMeta.value?.max ?? 20;
      break;
    case 'treble':
      min = trebleMeta.value?.min ?? -20;
      max = trebleMeta.value?.max ?? 20;
      break;
    default:
      return 0;
  }
  return normalizeEncoderValue(value, min, max);
}

// Handle gain knob change - directly mutate parameterValues
// The centralized parameter sync watcher will pick this up and send to API
function onGainChange(event) {
  const device = sdk.getDeviceInstance(props.instanceId);
  if (device?.parameterValues) {
    device.parameterValues['input_level'] = event.value;
    // Update encoder LED brightness
    setEncoderLedWithBrightness(0, normalizeValue(event.value, 'input_level'));
  }
}

// Handle volume knob change - directly mutate parameterValues
// The centralized parameter sync watcher will pick this up and send to API
function onVolumeChange(event) {
  const device = sdk.getDeviceInstance(props.instanceId);
  if (device?.parameterValues) {
    device.parameterValues['output_level'] = event.value;
    // Update encoder LED brightness
    setEncoderLedWithBrightness(4, normalizeValue(event.value, 'output_level'));
  }
}

function onBassChange(event) {
  const device = sdk.getDeviceInstance(props.instanceId);
  if (device?.parameterValues) {
    device.parameterValues['bass'] = event.value;
    setEncoderLedWithBrightness(1, normalizeValue(event.value, 'bass'));
  }
}

function onMidChange(event) {
  const device = sdk.getDeviceInstance(props.instanceId);
  if (device?.parameterValues) {
    device.parameterValues['mid'] = event.value;
    setEncoderLedWithBrightness(2, normalizeValue(event.value, 'mid'));
  }
}

function onTrebleChange(event) {
  const device = sdk.getDeviceInstance(props.instanceId);
  if (device?.parameterValues) {
    device.parameterValues['treble'] = event.value;
    setEncoderLedWithBrightness(3, normalizeValue(event.value, 'treble'));
  }
}

// Toggle bypass state
function toggleBypass() {
  const device = sdk?.getDeviceInstance(props.instanceId);
  if (device?.parameterValues) {
    const current = device.parameterValues['device.bypassed'] ?? 0;
    device.parameterValues['device.bypassed'] = current >= 0.5 ? 0 : 1;
  }
}

// Show temporary status message
function showStatus(message, type = 'info') {
  status.value = { message, type };

  // Auto-hide after 3 seconds if success, 5 seconds if error
  const duration = type === 'error' ? 5000 : 3000;
  setTimeout(() => {
    status.value = null;
  }, duration);
}

// Open model browser
function openModelBrowser() {
  showModelBrowser.value = true;
}

// Close model browser
function closeModelBrowser() {
  showModelBrowser.value = false;
}

// Handle model selection from file browser
function onModelSelected(fileInfo) {
  try {
    // Update the display immediately (strip .nam extension)
    currentModel.value = fileInfo.name.replace(/\.nam$/, '');

    // Update state property directly - the centralized state sync watcher
    // will pick this up and send it to the API
    const modelPath = fileInfo.path;
    const device = sdk.getDeviceInstance(props.instanceId);
    if (device?.stateProperties) {
      // Update both the full URI key and the simple 'model' key for compatibility
      device.stateProperties['http://github.com/mikeoliphant/neural-amp-modeler-lv2#model'] = modelPath;
      device.stateProperties['model'] = modelPath;
    }

    showStatus(`Model loaded: ${fileInfo.name}`, 'success');
  } catch (error) {
    console.error('[NAM] Failed to load model:', error);
    showStatus('Failed to load model', 'error');
    currentModel.value = null;
  }
}

// Setup encoder subscriptions for all parameters
// Encoder 0: input_level, Encoder 1: bass, Encoder 2: mid, Encoder 3: treble, Encoder 4: output_level
function setupEncoderSubscriptions() {
  // Unsubscribe from previous encoders
  encoderUnsubscribes.value.forEach(unsub => unsub?.());
  encoderUnsubscribes.value = [];

  if (!hardware) {
    console.warn('[NAM] Hardware not available');
    return;
  }

  const device = sdk?.getDeviceInstance(props.instanceId);
  if (!device?.parameterValues) {
    console.warn('[NAM] Device instance not available');
    return;
  }

  // Encoder 0 (first knob) - control input_level
  const unsubInput = hardware.onEncoderRotate(0, (event) => {
    const currentValue = device.parameterValues['input_level'] ?? 0;
    const newValue = scaleEncoderValue(currentValue, event.delta, inputLevelMin.value, inputLevelMax.value);
    device.parameterValues['input_level'] = newValue;
    console.log('[NAM] Updated input_level to', newValue);
    setEncoderLedWithBrightness(0, normalizeValue(newValue, 'input_level'));
  });
  encoderUnsubscribes.value.push(unsubInput);

  // Encoder 4 (fifth knob) - control output_level
  const unsubOutput = hardware.onEncoderRotate(4, (event) => {
    const currentValue = device.parameterValues['output_level'] ?? 0;
    const newValue = scaleEncoderValue(currentValue, event.delta, outputLevelMin.value, outputLevelMax.value);
    device.parameterValues['output_level'] = newValue;
    console.log('[NAM] Updated output_level to', newValue);
    setEncoderLedWithBrightness(4, normalizeValue(newValue, 'output_level'));
  });
  encoderUnsubscribes.value.push(unsubOutput);

  // Encoder 1 (second knob) - control bass
  const unsubBass = hardware.onEncoderRotate(1, (event) => {
    const currentValue = device.parameterValues['bass'] ?? 0;
    const newValue = scaleEncoderValue(currentValue, event.delta, bassMin.value, bassMax.value);
    device.parameterValues['bass'] = newValue;
    console.log('[NAM] Updated bass to', newValue);
    setEncoderLedWithBrightness(1, normalizeValue(newValue, 'bass'));
  });
  encoderUnsubscribes.value.push(unsubBass);

  // Encoder 2 (third knob) - control mid
  const unsubMid = hardware.onEncoderRotate(2, (event) => {
    const currentValue = device.parameterValues['mid'] ?? 0;
    const newValue = scaleEncoderValue(currentValue, event.delta, midMin.value, midMax.value);
    device.parameterValues['mid'] = newValue;
    console.log('[NAM] Updated mid to', newValue);
    setEncoderLedWithBrightness(2, normalizeValue(newValue, 'mid'));
  });
  encoderUnsubscribes.value.push(unsubMid);

  // Encoder 3 (fourth knob) - control treble
  const unsubTreble = hardware.onEncoderRotate(3, (event) => {
    const currentValue = device.parameterValues['treble'] ?? 0;
    const newValue = scaleEncoderValue(currentValue, event.delta, trebleMin.value, trebleMax.value);
    device.parameterValues['treble'] = newValue;
    console.log('[NAM] Updated treble to', newValue);
    setEncoderLedWithBrightness(3, normalizeValue(newValue, 'treble'));
  });
  encoderUnsubscribes.value.push(unsubTreble);

  console.log('[NAM] Encoder subscriptions set up: encoder 0 (input_level), encoder 1 (bass), encoder 2 (mid), encoder 3 (treble), encoder 4 (output_level)');
}

// Set a single encoder LED with brightness-scaled cyan
// normalized is 0-1 (linear parameter position), perceptual curve applied via ledBrightness
function setEncoderLedWithBrightness(encoderIndex, normalized) {
  if (!ledHandle) return;
  // Base RGB for cyan (#008B8B)
  const baseR = 0;
  const baseG = 139;
  const baseB = 139;

  const brightness = ledBrightness(normalized);
  const r = Math.round(baseR * brightness);
  const g = Math.round(baseG * brightness);
  const b = Math.round(baseB * brightness);

  ledHandle.setEncoderLed(encoderIndex, r, g, b, 0);
}

// Update all encoder LEDs based on current parameter values
function updateAllEncoderLeds() {
  if (!ledHandle) return;
  const device = sdk?.getDeviceInstance(props.instanceId);
  if (!device) return;

  // Base cyan color (#008B8B)
  const baseR = 0, baseG = 139, baseB = 139;
  const params = [
    { name: 'input_level', encoder: 0 },
    { name: 'bass', encoder: 1 },
    { name: 'mid', encoder: 2 },
    { name: 'treble', encoder: 3 },
    { name: 'output_level', encoder: 4 },
  ];

  const leds = [];
  for (const { name, encoder } of params) {
    const normalized = Math.max(0, Math.min(1, normalizeValue(device.parameterValues?.[name] ?? 0, name)));
    const brightness = ledBrightness(normalized);
    leds.push({
      encoder_id: encoder,
      r: Math.round(baseR * brightness),
      g: Math.round(baseG * brightness),
      b: Math.round(baseB * brightness),
      w: 0
    });
  }
  // Encoders 5-9: unused, turn off
  for (let i = 5; i < 10; i++) {
    leds.push({ encoder_id: i, r: 0, g: 0, b: 0, w: 0 });
  }
  ledHandle.setEncoderLeds(leds);
}

// Initialize component
onMounted(async () => {
  try {
    // Claim LED ownership for the NAM device UI
    ledHandle = pushLedLayer('nam-device', () => updateAllEncoderLeds());

    // Set encoder LEDs to NAM-specific color with brightness based on values
    updateAllEncoderLeds();

    // Get the device instance to load initial model state
    const device = sdk.getDeviceInstance(props.instanceId);

    if (device?.stateProperties) {
      // Try both the full URI key and the simple 'model' key
      const modelPath = device.stateProperties['http://github.com/mikeoliphant/neural-amp-modeler-lv2#model']
                     || device.stateProperties['model'];
      if (modelPath) {
        // Extract just the filename from the path and strip .nam extension
        // e.g., "Amps/TraynorYGL3.nam" -> "TraynorYGL3"
        const fileName = modelPath.split('/').pop();
        currentModel.value = fileName?.replace(/\.nam$/, '');
      }
    }

    // Setup encoder subscriptions
    setupEncoderSubscriptions();

    showStatus('NAM UI Ready', 'success');
  } catch (error) {
    console.error('[NAM] Initialization failed:', error);
    showStatus(`Error: ${error.message}`, 'error');
  }
});

// Cleanup subscriptions on unmount
onUnmounted(() => {
  encoderUnsubscribes.value.forEach(unsub => unsub?.());
  encoderUnsubscribes.value = [];

  if (ledHandle) {
    ledHandle.release();
    ledHandle = null;
  }
});
</script>

<style>
.nam-ui .model-display {
  position: absolute;
  top: 72px;
  left: 248px;
  width: 880px;
  height: 80px;
  display: flex;
  align-items: center;
  justify-content: flex-start;
}

.nam-ui .model-label {
  color: white;
  font-size: 23px;
  font-weight: 500;
  white-space: nowrap;
  overflow: hidden;
  position: absolute;
  top: 25px;
  left: 36px;
  text-overflow: ellipsis;
  max-width: 500px;
  pointer-events: none;
}


.nam-ui {
  position: relative;
  width: 100%;
  height: 100%;
  margin: 0;
  padding: 0;
  overflow: hidden;
  /* Adjust up a little bit to even out visual centering */
  user-select: none;
  -webkit-user-select: none;
  -moz-user-select: none;
  -ms-user-select: none;
}

.nam-ui .bypass-overlay {
  position: absolute;
  top: 0;
  left: 0;
  width: 100%;
  height: 100%;
  background: url('/Factory/Devices/NAM/ui/nam-ui-off.jpg') center / cover no-repeat;
  opacity: 0;
  transition: opacity 300ms ease;
  will-change: opacity;
  pointer-events: none;
  z-index: 1;
}

.nam-ui .bypass-overlay.active {
  opacity: 1;
}

.nam-ui .full-screen-image {
  position: absolute;
  top: 0;
  left: 0;
  width: 100%;
  height: 100%;
  object-fit: cover;
  pointer-events: none;
  -webkit-user-drag: none;
  user-select: none;
}

.nam-ui .neural-tube-container {
  position: absolute;
  z-index: 10;
}

.nam-ui .tube-gain {
  top: 272px;
  left: 322px;
}

.nam-ui .tube-bass {
  top: 272px;
  left: 560px;
}

.nam-ui .tube-mid {
  top: 272px;
  left: 806px;
}

.nam-ui .tube-treble {
  top: 272px;
  left: 1048px;
}

.nam-ui .tube-volume {
  top: 272px;
  left: 1290px;
}

.nam-ui .control-knobs {
  position: absolute;
  top: 552px;
  left: 0px;
  display: block;
  z-index: 2;
}

.nam-ui .control-knobs .knob-container {
  position: absolute;
  top: 16px;
}

.nam-ui .control-knobs .knob-gain {
  left: 280px;
}

.nam-ui .control-knobs .knob-bass {
  left: 540px;
}

.nam-ui .control-knobs .knob-mid {
  left: 798px;
}

.nam-ui .control-knobs .knob-treble {
  left: 1057px;
}

.nam-ui .control-knobs .knob-volume {
  left: 1314px;
}

.nam-ui .power-switch {
  position: absolute;
  top: 560px;
  left: 1500px;
  width: 150px;
  height: 140px;
  z-index: 20;
  cursor: pointer;
}

.nam-ui .control-knobs .knob-arc {
  display: none;
}
.nam-ui .control-knobs .knob-base {
  display:none;
}

.nam-ui .control-knobs .knob-indicator {
  background-color: #FFF;
}
</style>
