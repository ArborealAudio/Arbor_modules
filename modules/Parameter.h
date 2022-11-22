/**
 * Parameter.h
 * Custom parameter class for rolling juce params & clap params into one
 * Depends on juce_core (i think) and clap-juce-extensions
*/

#pragma once

class FloatParameter : public AudioParameterFloat,
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

    float getCurrentValue() const
    {
        return range.convertFrom0to1(jlimit(0.0f, 1.0f, range.convertTo0to1(get()) + modulationAmount));
    }

    operator float() const { return getCurrentValue(); }

private:

    float modulationAmount = 0.f;

    // const NormalisableRange<float> range;

};