
#pragma once
#include <JuceHeader.h>

#if JUCE_WINDOWS
static const char *os = "windows";
#elif JUCE_MAC
static const char *os = "macos";
#elif JUCE_LINUX
static const char *os = "linux";
#endif

static String downloadURL = "";

/** struct to hold data about an update check */
struct UpdateResult
{
    bool updateAvailable; // is an update avaiable?
    String changes = ""; // comma-separated list of changes
};

/**
 * DownloadManager.h
 * What it sounds like. Depends on gin's DownloadManager
 * 
 * Expects a macro PRODUCTION_BUILD to be true, or else
 * for debug purposes will always return that an update
 * is available
*/
struct DownloadManager : Component
{
    /**
    * @param _downloadPath location & filename to download to
    */
    DownloadManager(const char *_downloadPath) : downloadPath(_downloadPath)
    {
        addAndMakeVisible(yes);
        addAndMakeVisible(no);

        no.onClick = [&]
        {
            if (!isDownloading)
            {
                setVisible(false);
            }
            else
            {
                download.cancelAllDownloads();
                isDownloading = false;
                downloadFinished = false;
                no.setButtonText("No");
                yes.setVisible(true);
                yes.setButtonText("Yes");
                repaint();
            }
        };
        yes.onClick = [&]
        { downloadFinished = false; downloadUpdate(); };
    }

    ~DownloadManager() override
    {
        yes.setLookAndFeel(nullptr);
        no.setLookAndFeel(nullptr);
    }

    /** @param force whether to force the check even if checked < 24hrs ago */
    static UpdateResult checkForUpdate(const String pluginName, const String &currentVersion, const String &versionURL, bool force = false, int64 lastCheck = 0)
    {
        UpdateResult result;
        if (!force)
        {
            auto dayAgo = Time::getCurrentTime() - RelativeTime::hours(24);
            if (lastCheck > dayAgo.toMilliseconds())
            {
                result.updateAvailable = false;
                return result;
            }
        }

        auto stream = WebInputStream(URL(versionURL), false);
        auto connected = stream.connect(nullptr);
        auto size = stream.getTotalLength();
        if (connected && !stream.isError() && size > 0)
        {
            char *buf = (char*)malloc(sizeof(char) * (size_t)size);
            stream.read(buf, (int)size);
            auto data = JSON::parse(String(CharPointer_UTF8(buf)));
            free(buf);
            auto pluginInfo = data.getProperty("plugins", var()).getProperty(pluginName, var());
            auto changesObj = pluginInfo.getProperty("changes", var());
            if (changesObj.isArray())
            {
                std::vector<String> chVec;
                for (auto i = 0; i < changesObj.size(); ++i)
                {
                    chVec.emplace_back(changesObj[i].toString());
                }

                StringArray changesList{chVec.data(), (int)chVec.size()};
                result.changes = changesList.joinIntoString(", ");
            }
            else
            {
                result.changes = changesObj;
            }

            auto latestVersion = pluginInfo.getProperty("version", var());
            downloadURL = pluginInfo.getProperty("bin", var()).getProperty(os, var()).toString();

            DBG("Current: " << currentVersion);
            DBG("Latest: " << latestVersion.toString());

#if PRODUCTION_BUILD
            result.updateAvailable = currentVersion.removeCharacters(".") < latestVersion.toString().removeCharacters(".");
#else
            DBG("Update result: " << int(currentVersion.removeCharacters(".") < latestVersion.toString().removeCharacters(".")));
            result.updateAvailable = true;
#endif
        }
        else
            result.updateAvailable = false;

        return result;
    }

    void downloadUpdate()
    {
        download.setDownloadBlockSize(64 * 128);
        download.setProgressInterval(100);
        download.setRetryDelay(1);
        download.setRetryLimit(2);
        download.startAsyncDownload(URL(downloadURL), result, progress);
        isDownloading = true;
        no.setButtonText("Cancel");
        yes.setVisible(false);
    }

    void paint(Graphics &g) override
    {
        if (isVisible())
        {
            g.setColour(Colours::grey.darker());
            g.fillRoundedRectangle(getLocalBounds().toFloat(), 15.f);

            g.setColour(Colours::white);

            auto textbounds = Rectangle<int>{getLocalBounds().reduced(10).withTrimmedBottom(70)};

            if (!downloadFinished.load())
            {
                if (!isDownloading.load())
                    g.drawFittedText("A new update is available! Would you like to download?\n\nChanges:\n" + changes, textbounds, Justification::centredTop, 10, 0.f);
                else
                    g.drawFittedText("Downloading... " + String(downloadProgress.load()) + "%",
                                     textbounds, Justification::centred,
                                     1, 1.f);
            }
            else
            {
                if (downloadStatus.load())
                {
                    g.drawFittedText("Download complete.\nThe installer is in your Downloads folder. You must close your DAW to run the installation.",
                                     textbounds, Justification::centred,
                                     7, 1.f);
                    yes.setVisible(false);
                    no.setButtonText("Close");
                }
                else
                {
                    g.drawFittedText("Download failed. Please try again.",
                                     textbounds, Justification::centred,
                                     7, 1.f);
                    yes.setVisible(true);
                    yes.setButtonText("Retry");
                    yes.onClick = [this]
                    { downloadProgress = 0;
                    downloadFinished = false;
                    isDownloading = true;
                    downloadUpdate(); };
                }
            }
        }
        else
            return;
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        bounds.reduce(10, 10);
        auto halfWidth = bounds.getWidth() / 2;
        auto halfHeight = bounds.getHeight() / 2;

        Rectangle<int> yesBounds{bounds.withTrimmedTop(halfHeight).withTrimmedRight(halfWidth).reduced(20, 30)};
        Rectangle<int> noBounds{bounds.withTrimmedTop(halfHeight).withTrimmedLeft(halfWidth).reduced(20, 30)};

        yes.setBounds(yesBounds);
        no.setBounds(noBounds);
    }

    const String url, downloadPath;

    String changes;

private:

    void onUpdateCheck(bool checkResult)
    {
        if (isVisible() != checkResult)
            setVisible(checkResult);
    }

    gin::DownloadManager download;

    TextButton yes{"Yes"}, no{"No"};

    std::atomic<bool> downloadStatus = false;
    std::atomic<bool> isDownloading = false;
    std::atomic<int> downloadProgress = 0;
    std::atomic<bool> downloadFinished = false;

    std::function<void(gin::DownloadManager::DownloadResult)> result =
        [&](gin::DownloadManager::DownloadResult downloadResult)
    {
        downloadStatus = downloadResult.ok;
        isDownloading = false;

        if (!downloadResult.ok || downloadResult.httpCode != 200)
        {
            downloadFinished = true;
            repaint();
            return;
        }

        auto bin = File(downloadPath);
        if (!downloadResult.saveToFile(bin))
            downloadStatus = false;
        else
            downloadFinished = true;

        repaint();
    };

    std::function<void(int64, int64, int64)> progress = [&](int64 downloaded, int64 total, int64)
    {
        downloadProgress = 100 * ((float)downloaded / (float)total);
        repaint();
    };
};
