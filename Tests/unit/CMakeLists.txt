# Unit/regression tests for M1-Panner logic that does not need JUCE.
# Enabled from the project root with -DBUILD_UNIT_TESTS=ON, run with ctest.

add_executable(m1-panner-policy-tests ChannelConfigPolicyTests.cpp)

target_include_directories(m1-panner-policy-tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../Source
        ${CMAKE_CURRENT_SOURCE_DIR}/../../Modules/m1-sdk/libmach1spatial/api_encode/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../Modules/m1-sdk/libmach1spatial/api_common/include)

target_compile_features(m1-panner-policy-tests PRIVATE cxx_std_17)

add_test(NAME m1-panner-policy COMMAND m1-panner-policy-tests)
