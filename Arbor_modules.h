// Arbor_modules.h
/***
BEGIN_JUCE_MODULE_DECLARATION
ID: Arbor_modules
vendor: Arboreal Audio
version: 1.0
name: Arbor_modules
description: Parent module for several sub-modules, including VolumeMeter, SVTFilter,
LRFilter, SIMD, AudioBlock, Buffer, Release Pool, Parameter and Delay
dependencies: juce_audio_basics, juce_dsp, juce_graphics, juce_gui_basics
END_JUCE_MODULE_DECLARATION
***/

#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "xsimd/include/xsimd/xsimd.hpp"

#include <clap-juce-extensions/clap-juce-extensions.h>

using vec = xsimd::batch<double>;

namespace strix
{
using namespace juce;
#include "modules/Buffer.h"
#include "modules/SIMD.h"
#include "modules/FastMath.h"
#include "modules/VolumeMeter.h"
#include "modules/IIRFilter.h"
#include "modules/SVTFilter.h"
#include "modules/LRFilter.h"
#include "modules/ReleasePool.h"
#include "modules/Delay.h"
#include "modules/RingBuffer.h"
#include "modules/Parameter.h"
#include "modules/SmoothGain.h"
#include "modules/StereoImaging.h"
#include "modules/LiteThread.h"
#include "modules/Config.h"
#include "modules/DownloadManager.h"
}