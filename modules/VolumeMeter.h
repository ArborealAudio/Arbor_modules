// VolumeMeter.h
#pragma once

struct VolumeMeterSource : Timer
{
    VolumeMeterSource()
    {
        startTimerHz(60);
    }

    ~VolumeMeterSource() { stopTimer(); }

    void prepare(const dsp::ProcessSpec &spec)
    {
        numSamplesToRead = spec.maximumBlockSize;
        mainBuf.setSize(spec.numChannels, 44100, false, true, false);
        rmsBuf.setSize(spec.numChannels, numSamplesToRead, false, true, false);
        fifo.setTotalSize(numSamplesToRead);

        rmsSize = (0.1f * spec.sampleRate) / (float)spec.maximumBlockSize;
        rmsvec[0].assign(rmsSize, 0.f);
        rmsvec[1].assign(rmsSize, 0.f);

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

    void copyBuffer(const AudioBuffer<float> &_buffer)
    {
        const auto numSamples = _buffer.getNumSamples();
        const auto scope = fifo.write(numSamples);
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
        const auto scope = fifo.read(numSamplesToRead);
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

        peak = rmsBuf.getMagnitude(0, rmsBuf.getNumSamples());

        rmsL = rmsBuf.getRMSLevel(0, 0, rmsBuf.getNumSamples());
        rmsR = rmsL;
        if (rmsBuf.getNumChannels() > 1)
            rmsR = rmsBuf.getRMSLevel(1, 0, rmsBuf.getNumSamples());

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
        if (bufCopied)
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
        Reduction = 1, // 0 - Volume | 1 - Reduction
        Horizontal = 1 << 1, // 0 - Vertical | 1 - Horizontal
        ClipIndicator = 1 << 2
    };
    typedef uint8_t flags_t;

    Colour meterColor;

    /**
     * @param v audio source for the meter
     * @param s parameter the meter may be attached to (like a compression param, for instance). Used for turning the display on/off
     */
    VolumeMeterComponent(VolumeMeterSource &v, flags_t f, std::atomic<float> *s = nullptr) : source(v), state(s), flags(f)
    {
        // setLookAndFeel(&lnf);
        startTimerHz(45);
    }

    ~VolumeMeterComponent() override
    {
        // setLookAndFeel(nullptr);
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
            float maxDb = 24.f; // max dB the meter can display
            float padding = 0.f; // padding for the "GR" label
            if (isMouseButtonDown() || numTicks >= 150)
            {
                numTicks = 0;
                lastPeak = 0.f;
            }

            g.setColour(meterColor);

            auto db = Decibels::gainToDecibels(source.getAvgRMS(0), -60.f);
            auto peak = Decibels::gainToDecibels(source.peak, -60.f);

            auto ob = getLocalBounds();
            auto bounds = Rectangle<float>{ceilf(ob.getX()), ceilf(ob.getY()) + 1.f,
                                            floorf(ob.getRight()) - ceilf(ob.getX()) + 2.f, floorf(ob.getBottom()) - ceilf(ob.getY()) + 2.f};

            if (!(flags & Horizontal)) // Vertical
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
                padding = 20.f; // space for GR label
                float topTrim = 10.f;
                db = jmax(db, -maxDb + 3.f);
                Rectangle<float> rect = bounds.withWidth(bounds.getX() - db * bounds.getWidth() / maxDb).withTrimmedTop(topTrim);

                g.fillRect(rect);

                // g.setColour(Colours::red);
                // g.drawFittedText(String((int)std::abs(lastPeak)), Rectangle<int>(0, 0, padding * 0.75f, ob.getHeight()), Justification::centred, 1);
                // g.setColour(Colours::antiquewhite);
                // g.drawFittedText("GR", Rectangle<int>(0, 0, padding * 0.75f, ob.getHeight()), Justification::centred, 1);

                if (peak < lastPeak)
                {
                    peak = jmax(peak, -maxDb + 3.f);
                    g.fillRect((bounds.getX() - peak * bounds.getWidth() / maxDb)/*  + padding */, topTrim, 2.f, (float)ob.getHeight() - topTrim);
                    lastPeak = peak;
                }
                else
                    g.fillRect((bounds.getX() - lastPeak * bounds.getWidth() / maxDb)/*  + padding */, topTrim, 2.f, (float)ob.getHeight() - topTrim);

                /* ticks & numbers */
                const float nWidth = bounds.getWidth()/*  - padding */;
                for (float i = 0; i <= bounds.getRight(); i += nWidth / 6.f)
                {
                    g.setFont(topTrim);
                    String str = "| ";
                    str.append(String(static_cast<int>((i / nWidth) * maxDb)), 2);
                    g.drawText(str, Rectangle<int>(i - 1, 0, (int)topTrim + 5, (int)topTrim), Justification::centred);
                }
            }
            break;
            if (getState() && !*getState()) /*reset peak if comp is turned off*/
                lastPeak = 0.f;
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

protected:
    VolumeMeterSource &source;
    int numTicks = 0;

private:
    // VolumeMeterLookAndFeel lnf;
    flags_t flags;
    std::atomic<float> *state;
    bool lastState = false;
    float lastPeak = 0.f;

    ComponentAnimator anim;
};