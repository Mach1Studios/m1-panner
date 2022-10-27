# m1-panner
GUI and plugin concept for Mach1Encode API


### Modes
The M1-Panner can be compiled into two "modes"
 1. Multichannel Internal Spatial Audio Processing: The spatial audio is processed within the M1-Panner instance internally but is only supported in plugin hosts that support multichannel input/output
 	- Multichannel Internal Processing: `CUSTOM_CHANNEL_LAYOUT`: This mode is for plugin hosts that do not support changing the input/output bussing to the plugin after it has already been instantiated.
 	- Multichannel Internal Processing: `DYNAMIC_IO_PLUGIN_MODE`: This mode is for plugin hosts that can handle changing the input/output bussing and exposes the parameters/UI for managing changing the input/output layouts of the plugin
 2. Streaming External Spatial Audio Processing: The spatial audio is processed externally and the M1-Panner is compiled with the needed parameters/UI for indicating to the external processor what the expected I/O is, this is intended for plugin hosts that only support up to stereo audio I/O for plugins.
 	- Streaming External Processing: `STREAMING_PANNER_PLUGIN`: This is the preprocessor definition needed to activate this mode

### Compiler Options

#### STREAMING_PANNER_PLUGIN

##### CMake
- Add as a preprocess definition via `-DSTREAMING_PANNER_PLUGIN`

##### JUCE

#### CUSTOM_CHANNEL_LAYOUT

##### CMake

##### JUCE
Defining the `Plugin Channel Configuration` into the .jucer will automatically define the `CUSTOM_CHANNEL_LAYOUT` definition
![Custom_CHANNEL_LAYOUT JUCE Setup](./.readme/setup_custom_channel_layout.gif)
