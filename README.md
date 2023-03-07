# m1-panner
GUI and plugin concept for Mach1Encode API

## Modes
The M1-Panner can be compiled into two "modes"
 1. *Multichannel Internal Spatial Audio Processing*: The spatial audio is processed within the M1-Panner instance internally but is only supported in plugin hosts that support multichannel input/output
 	- Multichannel Internal Processing: `CUSTOM_CHANNEL_LAYOUT`: This mode is for plugin hosts that do not support changing the input/output bussing to the plugin after it has already been instantiated.
 	- Multichannel Internal Processing: This mode is for plugin hosts that can handle changing the input/output bussing and exposes the parameters/UI for managing changing the input/output layouts of the plugin
 2. *Streaming External Spatial Audio Processing*: The spatial audio is processed externally and the M1-Panner is compiled with the needed parameters/UI for indicating to the external processor what the expected I/O is, this is intended for plugin hosts that only support up to stereo audio I/O for plugins.
 	- Streaming External Processing: This is the preprocessor definition needed to activate this mode

## Compiler Options

**It is recommended to compile production plugins via CMake style, all example commands will be supplied at the bottom of this section**

**WARNING: When changing the compiler options make sure to clean the build folder**

### `CUSTOM_CHANNEL_LAYOUT`

##### CMake
- Add as a preprocess definition via `-DCUSTOM_CHANNEL_LAYOUT` and also define the `INPUTS` and `OUTPUTS`

Example:
`-DCUSTOM_CHANNEL_LAYOUT=1 -DINPUTS=1 -DOUTPUTS=8`

### `ITD_PARAMETERS`
Enables the Interaural Time Difference processing parameters for the M1-Panner for simulating creative headshadowing effects while panning.

#### CMake
- Add as a preprocess definition via `-DITD_PARAMETERS`

#### JUCE
- Add `ITD_PARAMETERS` in the .jucer's Exporters->[Target]->Extra Preprocessor Definitions

### Examples

- MacOS setup M1-Panner
```
cmake -Bbuild -G "Xcode" -DBUILD_VST3=ON -DBUILD_AAX=ON -DAAX_PATH="/Users/[USERNAME]/SDKs/aax-sdk-2-4-1" -DBUILD_AU=ON -DBUILD_VST=ON -DVST2_PATH="/Users/[USERNAME]/SDKs/VST_SDK_vst2/VST2_SDK" -DBUILD_AUV3=ON -DBUILD_UNITY=ON -DBUILD_STANDALONE=ON
```

- MacOS setup & compile M1-Panner with specific I/O:
```
cmake -Bbuild_i1o8 -G "Xcode" -DBUILD_WITH_CUSTOM_CHANNEL_LAYOUT=1 -DINPUTS=1 -DOUTPUTS=8 -DBUILD_VST2=1 -DVST2_PATH="/Users/[USERNAME]/SDKs/VST_SDK_vst2/VST2_SDK"
cmake --build build_i1o8 --config Release
```