// VolumeMeter.h

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
    enum Type
    {
        Volume,
        Reduction
    };

    enum Layout
    {
        Vertical,
        Horizontal
    };

    bool clipIndicator = false;

    struct VolumeMeterLookAndFeel : LookAndFeel_V4
    {
        VolumeMeterLookAndFeel(VolumeMeterComponent &comp) : owner(comp)
        {
        }

        void setMeterType(Type newType)
        {
            type = newType;
            if (type == Type::Reduction)
                lastPeak = 0.f;
        }
        void setMeterLayout(Layout newLayout) { layout = newLayout; }
        void setMeterColor(Colour newColor) { meterColor = newColor; }

        void drawMeterBar(Graphics &g)
        {
            switch (type)
            {
            case Volume:
            {
                if (owner.isMouseButtonDown() || owner.numTicks >= 150)
                {
                    lastPeak = -90.f;
                    owner.numTicks = 0;
                }

                g.setColour(Colours::white);

                auto dbL = Decibels::gainToDecibels(owner.source.getAvgRMS(0), -100.f);
                auto dbR = Decibels::gainToDecibels(owner.source.getAvgRMS(1), -100.f);

                auto peak = Decibels::gainToDecibels(owner.source.peak, -100.f);

                auto ob = owner.getLocalBounds().withTrimmedTop(owner.getHeight() * 0.1f);

                g.fillRect(ob.getCentreX() - 1, ob.getY(), 2, ob.getHeight());

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
                    if (lastPeak > 0.f && owner.clipIndicator)
                        g.setColour(Colours::red);
                    else
                        g.setColour(Colours::white);

                    g.drawHorizontalLine((int)bounds.getY() + jmax(lastPeak * bounds.getHeight() / -100.f, 0.f), bounds.getX(), bounds.getRight());

                    g.drawFittedText(String(lastPeak, 1) + "dB", owner.getLocalBounds().removeFromTop(owner.getHeight() * 0.1f), Justification::centred, 1);
                }
                else
                {
                    if (peak > 0.f && owner.clipIndicator)
                        g.setColour(Colours::red);
                    else
                        g.setColour(Colours::white);

                    g.drawHorizontalLine((int)bounds.getY() + jmax(peak * bounds.getHeight() / -100.f, 0.f), bounds.getX(), bounds.getRight());

                    g.drawFittedText(String(peak, 1) + "dB", owner.getLocalBounds().removeFromTop(owner.getHeight() * 0.1f), Justification::centred, 1);

                    lastPeak = peak;
                }
                break;
            }
            case Reduction:
            {
                if (owner.isMouseButtonDown() || owner.numTicks >= 300)
                {
                    owner.numTicks = 0;
                    lastPeak = 0.f;
                }

                g.setColour(meterColor);

                auto db = Decibels::gainToDecibels(owner.source.getAvgRMS(0), -60.f);
                auto peak = Decibels::gainToDecibels(owner.source.peak, -60.f);

                auto ob = owner.getLocalBounds();
                auto bounds = Rectangle<float>{ceilf(ob.getX()), ceilf(ob.getY()) + 1.f,
                                               floorf(ob.getRight()) - ceilf(ob.getX()) + 2.f, floorf(ob.getBottom()) - ceilf(ob.getY()) + 2.f};

                if (layout == Vertical)
                {
                    Rectangle<float> rect = bounds.withBottom(bounds.getY() - db * bounds.getHeight() / 36.f);

                    g.fillRect(rect.translated(0, 20));
                    g.drawFittedText("GR", Rectangle<int>(0, 0, ob.getWidth(), 20), Justification::centred, 1);
                }
                else if (layout == Horizontal)
                {
                    db = jmax(db, -21.f);
                    Rectangle<float> rect = bounds.withWidth(bounds.getX() - db * bounds.getWidth() / 24.f).withTrimmedTop(10.f);

                    g.fillRect(rect.translated(20, 0));

                    g.drawFittedText("GR", Rectangle<int>(0, 0, 15, ob.getHeight()), Justification::centred, 1);

                    if (peak < lastPeak)
                    {
                        peak = jmax(peak, -21.f);
                        g.fillRect((bounds.getX() - peak * bounds.getWidth() / 24.f) + 20, 10.f, 2.f, (float)ob.getHeight() - 10.f);
                        lastPeak = peak;
                    }
                    else
                        g.fillRect((bounds.getX() - lastPeak * bounds.getWidth() / 24.f) + 20, 10.f, 2.f, (float)ob.getHeight() - 10.f);

                    for (float i = 0; i <= bounds.getWidth(); i += bounds.getWidth() / 6.f)
                    {
                        if (i > 0)
                            g.fillRect(i + 19.f, 0.f, 2.f, 10.f);
                        g.setFont(8.f);
                        g.drawFittedText(String((i / bounds.getWidth()) * 24.f), Rectangle<int>(i + 10, 0, 10, 10), Justification::centred, 1);
                    }

                    if (owner.getState() && !*owner.getState()) /*reset peak if comp is turned off*/
                        lastPeak = 0.f;
                }
                break;
            }
            default:
                return;
            }
        }

        Type type;

    private:
        Layout layout;
        Colour meterColor;
        VolumeMeterComponent &owner;

        float lastPeak = -90.f;
    };

    void setMeterType(Type newType) { lnf.setMeterType(newType); }
    void setMeterLayout(Layout newLayout) { lnf.setMeterLayout(newLayout); }
    void setMeterColor(Colour newColor) { lnf.setMeterColor(newColor); }

    /**
     * @param v audio source for the meter
     * @param s parameter the meter may be attached to (like a compression param, for instance). Used for turning the display on/off
     */
    VolumeMeterComponent(VolumeMeterSource &v, std::atomic<float> *s = nullptr) : lnf(*this), source(v), state(s)
    {
        setLookAndFeel(&lnf);
        startTimerHz(45);
    }

    ~VolumeMeterComponent() override
    {
        setLookAndFeel(nullptr);
        stopTimer();
    }

    void paint(Graphics &g) override
    {
        lnf.drawMeterBar(g);
    }

    void timerCallback() override
    {
        if (source.newBuf)
        {
            source.newBuf = false;
            repaint(getLocalBounds());
            ++numTicks;
        }

        if (lnf.type == Type::Reduction)
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
    VolumeMeterLookAndFeel lnf;
    std::atomic<float> *state;
    bool lastState = false;

    ComponentAnimator anim;
};