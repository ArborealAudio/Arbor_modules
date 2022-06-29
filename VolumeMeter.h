// VolumeMeter.h
/***
BEGIN_JUCE_MODULE_DECLARATION
ID: VolumeMeter
vendor: Arboreal Audio
version: 1.0
name: VolumeMeter
description: Set of classes for drawing a simple volume/gr meter
dependencies: juce_core, juce_dsp, juce_graphics, juce_gui_basics
END_JUCE_MODULE_DECLARATION
***/

#pragma once
#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>

using namespace juce;

struct VolumeMeterSource
{
    VolumeMeterSource() = default;

    ~VolumeMeterSource(){}

    void prepare(const dsp::ProcessSpec& spec)
    {
        rmsSize = (0.1f * spec.sampleRate) / (float)spec.maximumBlockSize;
        rmsvec[0].assign(rmsSize, 0.f);
        rmsvec[1].assign(rmsSize, 0.f);

        sum[0] = sum[1] = 0.f;

        if (rmsSize > 1){
            lPtr %= rmsvec[0].size();
            rPtr %= rmsvec[1].size();
        }
        else {
            lPtr = 0;
            rPtr = 0;
        }
    }

    void measureBlock(const AudioBuffer<float>& buffer)
    {
        rmsL = buffer.getRMSLevel(0, 0, buffer.getNumSamples());
        if (buffer.getNumChannels() > 1)
            rmsR = buffer.getRMSLevel(1, 0, buffer.getNumSamples());

        if (rmsvec[0].size() > 0) {
            rmsvec[0][lPtr] = rmsL * rmsL;
            rmsvec[1][rPtr] = rmsR * rmsR;
            lPtr = (lPtr + 1) % rmsvec[0].size();
            rPtr = (rPtr + 1) % rmsvec[1].size();
        }
        else {
            sum[0] = rmsL * rmsL;
            sum[1] = rmsR * rmsR;
        }

        newBuf = true;
    }

    void measureGR(float newGR)
    {
        if (rmsvec[0].size() > 0)
        {
            rmsvec[0][lPtr] = newGR * newGR;
            lPtr = (lPtr + 1) % rmsvec[0].size();
        }
        else
            sum[0] = newGR * newGR;

        newBuf = true;
    }

    inline float getAvgRMS(int ch) const
    {
        if (rmsvec[ch].size() > 0)
            return std::sqrt(std::accumulate(rmsvec[ch].begin(), rmsvec[ch].end(), 0.f) / (float)rmsvec[ch].size());

        return std::sqrt(sum[ch]);
    }

    void setFlag(bool flag)
    {
        newBuf = flag;
    }

    bool getFlag() const
    {
        return newBuf;
    }

private:
    bool newBuf = false;

    float rmsSize = 0.f;
    float rmsL = 0.f, rmsR = 0.f;
    std::vector<float> rmsvec[2];
    size_t lPtr = 0, rPtr = 0;
    std::atomic<float> sum[2];
};

struct VolumeMeterComponent : Component, Timer
{
    enum Type
    {
        Volume,
        Reduction
    };

    struct VolumeMeterLookAndFeel : LookAndFeel_V4
    {
        VolumeMeterLookAndFeel(VolumeMeterComponent& comp, Type t) : owner(comp), type(t)
        {}

        void setMeterType(Type newType) {type = newType;}

        void drawMeterBar(Graphics& g)
        {
            switch (type)
            {
            case Volume: {
                g.setColour(Colours::white);

                auto dbL = Decibels::gainToDecibels(owner.source.getAvgRMS(0), -100.f);
                auto dbR = Decibels::gainToDecibels(owner.source.getAvgRMS(1), -100.f);

                auto bounds = Rectangle<float>{ceilf(owner.getX()) + 1.f, ceilf(owner.getY()) + 1.f,
                                            floorf(owner.getRight()) - ceilf(owner.getX()) + 2.f,
                                            floorf(owner.getBottom()) - ceilf(owner.getY()) + 2.f};

                Rectangle<float> rectL = bounds.withTop(bounds.getY() + dbL * bounds.getHeight() / -100.f).removeFromLeft(bounds.getWidth() / 2.f + 5.f);
                Rectangle<float> rectR = bounds.withTop(bounds.getY() + dbR * bounds.getHeight() / -100.f).removeFromRight(bounds.getWidth() / 2.f);

                g.fillRect(rectL);
                g.fillRect(rectR);
                break;
                }
            case Reduction: {
                g.setColour(Colours::white);

                auto db = Decibels::gainToDecibels(owner.source.getAvgRMS(0), -60.f);

                auto bounds = Rectangle<float>{ceilf(owner.getX()) + 1.f, ceilf(owner.getY()) + 1.f,
                                            floorf(owner.getRight()) - ceilf(owner.getX()) + 2.f,
                                            floorf(owner.getBottom()) - ceilf(owner.getY()) + 2.f};

                Rectangle<float> rect = bounds.withBottom(bounds.getY() - db * bounds.getHeight() / 60.f);

                g.fillRect(rect.translated(0, 20));
                // g.drawRoundedRectangle(owner.getLocalBounds().toFloat(), 3.f, 1.f);
                if (db < 0.f) {
                    g.drawFittedText("GR", Rectangle<int>(0, 0, owner.getWidth(), 20), Justification::centred, 1);
                }
                break;
                }
            }
        }

    private:
        Type type;
        VolumeMeterComponent& owner;
    };

    void setMeterType(Type newType) 
    {
        type = newType;
        lnf.setMeterType(type);
    }

    VolumeMeterComponent(VolumeMeterSource& v) : source(v), lnf(*this, type)
    {
        setLookAndFeel(&lnf);
        startTimerHz(30);
    }

    ~VolumeMeterComponent() override
    {
        setLookAndFeel(nullptr);
        stopTimer();
    }

    void paint(Graphics& g) override
    {
        lnf.drawMeterBar(g);
    }

    void timerCallback() override
    {
        if (&source && source.getFlag())
        {
            source.setFlag(false);
            repaint();
        }
    }

protected:
    VolumeMeterSource& source;
private:
    Type type;
    VolumeMeterLookAndFeel lnf;
};