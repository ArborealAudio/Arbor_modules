// Arbor_modules.h
/***
BEGIN_JUCE_MODULE_DECLARATION
ID: Arbor_modules
vendor: Arboreal Audio
version: 1.0
name: Arbor_modules
description: Parent module for several sub-modules, including VolumeMeter, SVTFilter,
Release Pool, and Delay
dependencies: juce_core, juce_dsp, juce_graphics, juce_gui_basics
END_JUCE_MODULE_DECLARATION
***/

#pragma once
#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace strix
{
#include "VolumeMeter/VolumeMeter.h"
#include "SVTFilter/SVTFilter.h"
#include "ReleasePool/ReleasePool.h"
#include "Delay/Delay.h"
}

using namespace juce;