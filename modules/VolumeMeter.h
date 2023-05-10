// VolumeMeter.h
#pragma once
#include <JuceHeader.h>

struct VolumeMeterSource : Timer
{
    VolumeMeterSource()
    {
        startTimerHz(60);
    }

    ~VolumeMeterSource() override { stopTimer(); }

    /**
     * @param rmsWindow window in ms to measure RMS
     */
    void prepare(const dsp::ProcessSpec &spec, const float rmsWindow)
    {
        numSamplesToRead = spec.maximumBlockSize;
        mainBuf.setSize(spec.numChannels, jmax(44100, (int)spec.maximumBlockSize), false, true, false);
        rmsBuf.setSize(spec.numChannels, numSamplesToRead, false, true, false);
        fifo.setTotalSize(numSamplesToRead);

        rmsSize = (rmsWindow * spec.sampleRate) / (float)spec.maximumBlockSize;
        rmsvec[0].assign((size_t)rmsSize, 0.f);
        rmsvec[1].assign((size_t)rmsSize, 0.f);

        sum[0] = sum[1] = 0.f;

        if (rmsSize > 1)
        {
            lPtr %= rmsvec[0].size();
            rPtr %= rmsvec[1].size();
        }
        else
        {
            lPtr = 0;
            rPtr = 0;
        }
    }

    void reset()
    {
        mainBuf.clear();
        rmsBuf.clear();
        fifo.reset();
        sum[0] = sum[1] = 0.f;
        rmsvec[0].assign(rmsSize, 0.f);
        rmsvec[1].assign(rmsSize, 0.f);
    }

    // lock-free method for copying data to the meter's buffer
    template <typename T>
    void copyBuffer(const T *const *buffer, size_t numChannels, size_t numSamples)
    {
        const auto scope = fifo.write(jmin((int)numSamples, fifo.getFreeSpace()));
        if (scope.blockSize1 > 0)
        {
            mainBuf.copyFrom(0, scope.startIndex1, buffer[0], scope.blockSize1);
            if (numChannels > 1)
                mainBuf.copyFrom(1, scope.startIndex1, buffer[1], scope.blockSize1);
        }
        if (scope.blockSize2 > 0)
        {
            mainBuf.copyFrom(0, scope.startIndex2, buffer[0], scope.blockSize2);
            if (numChannels > 1)
                mainBuf.copyFrom(1, scope.startIndex2, buffer[1], scope.blockSize2);
        }
        bufCopied = true;
    }

    // lock-free method for copying data to the meter's buffer
    void copyBuffer(const AudioBuffer<float> &_buffer)
    {
        const auto numSamples = _buffer.getNumSamples();
        const auto scope = fifo.write(jmin(numSamples, fifo.getFreeSpace()));
        if (scope.blockSize1 > 0)
        {
            mainBuf.copyFrom(0, scope.startIndex1, _buffer, 0, 0, scope.blockSize1);
            if (_buffer.getNumChannels() > 1)
                mainBuf.copyFrom(1, scope.startIndex1, _buffer, 1, 0, scope.blockSize1);
        }
        if (scope.blockSize2 > 0)
        {
            mainBuf.copyFrom(0, scope.startIndex2, _buffer, 0, 0, scope.blockSize2);
            if (_buffer.getNumChannels() > 1)
                mainBuf.copyFrom(1, scope.startIndex2, _buffer, 1, 0, scope.blockSize2);
        }
        bufCopied = true;
    }

    void measureBlock()
    {
        const auto numRead = jmin(numSamplesToRead, fifo.getNumReady());
        if (numRead <= 0)
            return;
        const auto scope = fifo.read(numRead);
        if (scope.blockSize1 > 0)
        {
            rmsBuf.copyFrom(0, 0, mainBuf, 0, scope.startIndex1, scope.blockSize1);
            if (mainBuf.getNumChannels() > 1)
                rmsBuf.copyFrom(1, 0, mainBuf, 1, scope.startIndex1, scope.blockSize1);
        }
        if (scope.blockSize2 > 0)
        {
            rmsBuf.copyFrom(0, scope.blockSize1, mainBuf, 0, scope.startIndex2, scope.blockSize2);
            if (mainBuf.getNumChannels() > 1)
                rmsBuf.copyFrom(1, scope.blockSize1, mainBuf, 1, scope.startIndex2, scope.blockSize2);
        }

        peak = rmsBuf.getMagnitude(0, numRead);

        rmsL = rmsBuf.getRMSLevel(0, 0, numRead);
        rmsR = rmsL;
        if (rmsBuf.getNumChannels() > 1)
            rmsR = rmsBuf.getRMSLevel(1, 0, numRead);

        if (rmsvec[0].size() > 0)
        {
            rmsvec[0][lPtr] = rmsL * rmsL;
            rmsvec[1][rPtr] = rmsR * rmsR;
            lPtr = (lPtr + 1) % rmsvec[0].size();
            rPtr = (rPtr + 1) % rmsvec[1].size();
        }
        else
        {
            sum[0] = rmsL * rmsL;
            sum[1] = rmsR * rmsR;
        }

        newBuf = true;
    }

    void timerCallback() override
    {
        // std::unique_lock<std::mutex> lock (mutex);
        juce::ScopedTryLock lock(mutex);
        if (bufCopied && lock.isLocked())
        {
            measureBlock();
            bufCopied = false;
        }
    }

    void measureGR(float newGR)
    {
        peak = newGR;

        if (rmsvec[0].size() > 0)
        {
            rmsvec[0][lPtr] = newGR * newGR;
            lPtr = (lPtr + 1) % rmsvec[0].size();
        }
        else
            sum[0] = newGR * newGR;

        newBuf = true;
    }

    inline float getAvgRMS() const
    {
        if (rmsvec[0].size() > 0 && rmsvec[1].size() > 0)
        {
            float rmsL, rmsR;
            rmsL = std::sqrt(std::accumulate(rmsvec[0].begin(), rmsvec[0].end(), 0.f) / (float)rmsvec[0].size());
            rmsR = std::sqrt(std::accumulate(rmsvec[1].begin(), rmsvec[1].end(), 0.f) / (float)rmsvec[1].size());
            return (rmsL + rmsR) / 2.f;
        }

        return (std::sqrt(sum[0]) + std::sqrt(sum[1])) / 2.f;
    }

    inline float getAvgRMS(int ch) const
    {
        if (rmsvec[ch].size() > 0)
            return std::sqrt(std::accumulate(rmsvec[ch].begin(), rmsvec[ch].end(), 0.f) / (float)rmsvec[ch].size());

        return std::sqrt(sum[ch]);
    }

    float peak = 0.f;
    std::atomic<bool> newBuf = false, bufCopied = false;

private:
    AbstractFifo fifo{1024};
    AudioBuffer<float> mainBuf, rmsBuf;
    int numSamplesToRead = 0;

    juce::CriticalSection mutex;

    float rmsSize = 0.f;
    float rmsL = 0.f, rmsR = 0.f;
    std::vector<float> rmsvec[2];
    size_t lPtr = 0, rPtr = 0;
    std::atomic<float> sum[2];
};

struct VolumeMeterComponent : Component, Timer
{
    enum
    {
        Reduction = 1,       // 0 - Volume | 1 - Reduction
        Horizontal = 1 << 1, // 0 - Vertical | 1 - Horizontal
        ClipIndicator = 1 << 2,
        Background = 1 << 3 // 0 - No background | 1 - draw a background
    };
    typedef uint8_t Flags;

    Colour meterColor, backgroundColor;

    /**
     * @param v audio source for the meter
     * @param s parameter the meter may be attached to (like a compression param, for instance). Used for turning the display on/off
     */
    VolumeMeterComponent(VolumeMeterSource &v, Flags f, std::atomic<float> *s = nullptr) : source(v), state(s), flags(f)
    {
        startTimerHz(45);
    }

    ~VolumeMeterComponent() override
    {
        stopTimer();
    }

    void paint(Graphics &g) override
    {
        switch (flags & Reduction)
        {
        case 0: // Volume
        {
            if (isMouseButtonDown() || numTicks >= 150)
            {
                lastPeak = -90.f;
                numTicks = 0;
            }

            auto dbL = Decibels::gainToDecibels(source.getAvgRMS(0), -100.f);
            auto dbR = Decibels::gainToDecibels(source.getAvgRMS(1), -100.f);

            auto peak = Decibels::gainToDecibels(source.peak, -100.f);

            auto ob = getLocalBounds().withTrimmedTop(getHeight() * 0.1f);

            g.setColour(Colours::white);
            g.fillRoundedRectangle((float)(ob.getCentreX() - 1), (float)ob.getY(), 2.f, (float)ob.getHeight(), 2.5f);

            auto bounds = Rectangle<float>{(float)ob.getX(), (float)ob.getY() + 4.f,
                                           (float)ob.getRight() - ob.getX(),
                                           (float)ob.getBottom() - ob.getY() - 2.f};

            bounds.reduce(4.f, 4.f);

            /*RMS meter*/

            Rectangle<float> rectL = bounds.withTop(bounds.getY() + jmax(dbL * bounds.getHeight() / -100.f, 0.f)).removeFromLeft(bounds.getWidth() / 2.f - 3.f);
            Rectangle<float> rectR = bounds.withTop(bounds.getY() + jmax(dbR * bounds.getHeight() / -100.f, 0.f)).removeFromRight(bounds.getWidth() / 2.f - 3.f);

            g.setColour(meterColor);

            g.fillRect(rectL);
            g.fillRect(rectR);

            /*peak ticks*/

            if (lastPeak > peak)
            {
                if (lastPeak > 0.f && (flags & ClipIndicator))
                    g.setColour(Colours::red);
                else
                    g.setColour(Colours::white);

                g.drawHorizontalLine((int)bounds.getY() + jmax(lastPeak * bounds.getHeight() / -100.f, 0.f), bounds.getX(), bounds.getRight());

                g.drawFittedText(String(lastPeak, 1) + "dB", getLocalBounds().removeFromTop(getHeight() * 0.1f), Justification::centred, 1);
            }
            else
            {
                if (peak > 0.f && (flags & ClipIndicator))
                    g.setColour(Colours::red);
                else
                    g.setColour(Colours::white);

                g.drawHorizontalLine((int)bounds.getY() + jmax(peak * bounds.getHeight() / -100.f, 0.f), bounds.getX(), bounds.getRight());

                g.drawFittedText(String(peak, 1) + "dB", getLocalBounds().removeFromTop(getHeight() * 0.1f), Justification::centred, 1);

                lastPeak = peak;
            }
            break;
        }
        case 1: // Reduction
        {
            float maxDb = 24.f;  // max dB the meter can display
            float padding = 0.f; // padding for the "GR" label
            if (isMouseButtonDown() || numTicks >= 225)
            {
                numTicks = 0;
                lastPeak = 0.f;
            }

            auto db = Decibels::gainToDecibels(source.getAvgRMS(), -60.f);
            auto peak = Decibels::gainToDecibels(source.peak, -60.f);

            auto ob = getLocalBounds();
            auto bounds = Rectangle<float>{ceilf(ob.getX()), ceilf(ob.getY()) + 1.f,
                                           floorf(ob.getRight()) - ceilf(ob.getX()) + 2.f, floorf(ob.getBottom()) - ceilf(ob.getY()) + 2.f};

            if (flags & Background)
            {
                g.setColour(backgroundColor);
                g.fillRoundedRectangle(bounds.reduced(2.f), 5.f);
            }

            bounds.reduce(4.f, 4.f);

            g.setColour(meterColor);
            if (!(flags & Horizontal)) // INCOMPLETE: Vertical
            {
                maxDb = 36.f;
                padding = 15.f;
                db = jmax(db, -maxDb + 3.f);
                Rectangle<float> rect = bounds.withBottom(bounds.getY() - db * bounds.getHeight() / maxDb);

                g.fillRect(rect.translated(0, padding));
                g.drawFittedText("GR", Rectangle<int>(0, 0, ob.getWidth(), padding * 0.75f), Justification::centred, 1);

                /* peak tick */
                if (peak < lastPeak)
                {
                    peak = jmax(peak, -maxDb + 3.f);
                    g.fillRect(bounds.getX(), (bounds.getY() - peak * bounds.getHeight() / maxDb) + padding, bounds.getWidth(), 2.f);
                    lastPeak = peak;
                }
                else
                    g.fillRect(bounds.getX(), (bounds.getY() - lastPeak * bounds.getHeight() / maxDb) + padding, bounds.getWidth(), 2.f);

                /* meter scale ticks */
                // for (float i = 0; i <= bounds.getHeight(); i += bounds.getHeight() / 6.f)
                // {
                //     if (i > 0)
                //         g.fillRect(0.f, i + padding, 4.f, 2.f);
                //     g.setFont(8.f);
                //     g.drawFittedText(String((i / bounds.getHeight()) * maxDb), Rectangle<int>(0, i, 10, 10), Justification::centred, 1);
                // }
            }
            else // Horizontal
            {
                maxDb = 24.f;
                padding = 30.f; // space for meter values (if printing them. Must uncomment uses below!)
                float topTrim = 10.f;
                db = jmax(db, -maxDb + 3.f);
                Rectangle<float> rect = bounds.withRight(bounds.getX() - db * bounds.getWidth() / maxDb).withTrimmedTop(topTrim);

                g.fillRect(rect);

#ifdef TEST_METER_VALUES
                g.setColour(Colours::red);
                g.drawFittedText(String(String((int)std::abs(lastPeak)) + "/" + String((int)std::abs(db))), Rectangle<int>(0, 0, padding, ob.getHeight()), Justification::centred, 1);
                g.setColour(meterColor);
#endif

                if (peak < lastPeak && peak != 0.f)
                {
                    peak = jmax(peak, -maxDb + 3.f);
                    g.fillRect((bounds.getX() - peak * bounds.getWidth() / maxDb) /*  + padding */, rect.getY(), 2.f, rect.getHeight());
                    lastPeak = peak;
                }
                else if (lastPeak != 0.f)
                    g.fillRect((bounds.getX() - lastPeak * bounds.getWidth() / maxDb) /*  + padding */, rect.getY(), 2.f, rect.getHeight());

                /* ticks & numbers */
                const float nWidth = bounds.getWidth() /*  - padding */;
                for (float i = 0; i + topTrim + 5 <= bounds.getRight(); i += nWidth / 6.f)
                {
                    g.setFont(topTrim);
                    String str = "| ";
                    str.append(String(static_cast<int>((i / nWidth) * maxDb)), 2);
                    g.drawText(str, Rectangle<int>(bounds.getX() + i - 1, bounds.getY(), (int)topTrim + 5, (int)topTrim), Justification::centred);
                }
            }
            if (getState() && !*getState()) /*reset peak if comp is turned off*/
                lastPeak = 0.f;
            break;
        }
        default:
            return;
        }
    }

    void timerCallback() override
    {
        if (source.newBuf)
        {
            source.newBuf = false;
            repaint(getLocalBounds());
            ++numTicks;
        }

        if (flags & Reduction)
        {
            if (!*state && !anim.isAnimating(this))
            {
                anim.fadeOut(this, 500);
                lastState = false;
                setVisible(false);
            }
            else if (*state && !anim.isAnimating(this) && !lastState)
            {
                anim.fadeIn(this, 500);
                lastState = true;
            }
        }
    }

    std::atomic<float> *getState() { return state; }
    void setState(std::atomic<float> *newState) { state = newState; }

private:
    VolumeMeterSource &source;
    int numTicks = 0;
    Flags flags;
    std::atomic<float> *state;
    bool lastState = false;
    float lastPeak = 0.f;

    ComponentAnimator anim;
};