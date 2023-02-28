# Arbor_modules

A collection of modules to be used with JUCE, mostly forks of JUCE dsp modules with xsimd capability.

Includes a few useful utilities as well, like:
 - Fast Math (only has one function so far lol)
 - CLAP-able parameters
 - SIMD helpers for creating interleaved SIMD audio blocks
 - Release Pool for threadsafe deletion of processors, based on [Timur Doumler's talk](https://github.com/CppCon/CppCon2015/blob/master/Presentations/C++%20In%20the%20Audio%20Industry/C++%20In%20the%20Audio%20Industry%20-%20Timur%20Doumler%20-%20CppCon%202015.pdf)
 - Smooth Gain helpers
 - A volume meter component not exactly fit for plug-and-play usage
 - Small thread module for one-off jobs
 - Helper functions for writing to a config file
 - DownloadManager, for managing plugin update checking & retrieving
