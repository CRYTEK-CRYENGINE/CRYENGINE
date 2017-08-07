include(Platform/Android/ndk-stl-c++)
macro(__android_stl lang)
  __android_stl_cxx(${lang} libc++_shared.so)
endmacro()
