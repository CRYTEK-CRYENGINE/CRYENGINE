#!/bin/bash

# Create a symbolic link to Code/SDKs/SDL2. (symbolic link created during bootstraping)

"G:/NVPACK/android-ndk-r13/ndk-build.cmd" NDK_MODULE_PATH=.:"G:/NVPACK/android-ndk-r13/prebuilt" APP_PLATFORM=android-23 APP_ABI=arm64-v8a APP_OPTIM=release

cp libs/armeabi-v7a/libSDL2Ext.so ../lib/android-armeabi-v7a/libSDL2Ext.so