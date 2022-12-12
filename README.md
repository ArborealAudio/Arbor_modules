# Arbor_modules

A collection of modules to be used with JUCE, mostly forks of JUCE dsp modules with xsimd capability.

Single-header for the time being. Include it in your project however you wish, then just `#include Arbor_modules.h`

Everything can be accessed within the `strix` namespace, except the xsimd library

Includes a few useful utilities as well, like:
 - Fast Math (only has one function so far lol)
 - CLAP-able parameters
 - SIMD helpers for creating interleaved SIMD audio blocks
 - Release Pool for threadsafe deletion of processors, based on [Timur Doumler's presentation](https://github.com/CppCon/CppCon2015/blob/master/Presentations/C++%20In%20the%20Audio%20Industry/C++%20In%20the%20Audio%20Industry%20-%20Timur%20Doumler%20-%20CppCon%202015.pdf)
 - Smooth Gain helpers
 - A worker thread class (Chat GPT-generated, doens't currently compile L.o.L)
 - A volume meter component not exactly fit for plug-and-play usage

## License

Arbor_modules is provided under the GPLv3 license

xsimd is licensed under the BSD-3-clause license
