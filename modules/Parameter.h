/**
 * Parameter.h
 * Custom parameter class for rolling juce params & clap params into one
 * Depends on juce_core (i think) and clap-juce-extensions
*/

#pragma once
#include <clap-juce-extensions/clap-juce-extensions.h>

class FloatParameter : public juce::AudioParameterFloat,
                       public clap_juce_extensions::clap_juce_parameter_capabilities
{
public:
    FloatParameter (const ParameterID& parameterID,
                         const String& parameterName,
                         NormalisableRange<float> normalisableRange,
                         float defaultValue,
                         const AudioParameterFloatAttributes& attributes = {})
        : AudioParameterFloat(parameterID, parameterName, normalisableRange, defaultValue, attributes)
    {
    }

    FloatParameter (const ParameterID& parameterID,
                         const String& parameterName,
                         float minValue,
                         float maxValue,
                         float defaultValue)
        : AudioParameterFloat(parameterID, parameterName, minValue, maxValue, defaultValue)
    {
    }

    bool supportsMonophonicModulation() override { return true; }

    void applyMonophonicModulation (double amount) override
    {
        modulationAmount = (float)amount;
    }

private:

    float modulationAmount = 0.f;

};