
#pragma once
#include <JuceHeader.h>
#include <quicfetch.h>

#if JUCE_WINDOWS
    #define OS "windows"
#elif JUCE_MAC
    #define OS "macos"
#elif JUCE_LINUX
    #define OS "linux"
#endif

/** struct to hold data about an update check */
struct UpdateStatus
{
    bool updateAvailable; // is an update avaiable?
    String changes = ""; // comma-separated list of changes
    enum State {
        Checking,
        Finished
    } state;
};

struct DownloadStatus
{
    // Did download succeed
    bool ok;
    // Percentage of total dl size
    int progress;
    // Size in bytes of download
    size_t totalSize;
    // State of manager. NotStarted, Downloading, Finished
    enum State {
        NotStarted,
        Downloading,
        Finished,
    } state = NotStarted;
};

static void onUpdateCheck(Updater *updater, bool checkResult, void *user_data);
static void downloadProgress(Updater *updater, size_t read, size_t total, void *user_data);
static void downloadFinished(Updater *updater, bool ok, size_t size, void *user_data);

/**
 * DownloadManager.h
 * 
 * Depends on abriccio/quicfetch (https://github.com/abriccio/quicfetch.git)
 * You may define a macro ARBOR_DEBUG_DOWNLOADER, which for
 * debug purposes will always return that an update
 * is available
*/
struct DownloadManager : Component, Timer
{
    /**
    * @param _downloadPath location & filename to download to
    */
    DownloadManager(const String &_downloadPath) : downloadPath(_downloadPath)
    {
        addAndMakeVisible(yes);
        addAndMakeVisible(no);

        no.onClick = [&]
        {
            if (dlStatus.state != DownloadStatus::Downloading)
            {
                setVisible(false);
            }
            else
            {
                // download.cancelAllDownloads();
                dlStatus.state = DownloadStatus::Finished;
                no.setButtonText("No");
                yes.setVisible(true);
                yes.setButtonText("Yes");
                repaint();
            }
        };
        yes.onClick = [&]
        {
            dlStatus.state = DownloadStatus::Downloading;
            downloadUpdate();
        };

        startTimerHz(10);
    }

    ~DownloadManager() override
    {
        yes.setLookAndFeel(nullptr);
        no.setLookAndFeel(nullptr);
        if (updater) {
            updater_deinit(updater);
            updater = nullptr;
        }
        stopTimer();
    }

    void timerCallback() override
    {
        if (needsRepaint) {
            repaint();
            needsRepaint = false;
        }
    }

    /** @param force whether to force the check even if checked < 24hrs ago */
    void checkForUpdate(const char *pluginName,
                                       const char *currentVersion,
                                       const char *versionURL,
                                       bool force = false, bool beta = false,
                                       int64 lastCheck = 0)
    {
        updateStatus.state = UpdateStatus::Checking;
        updater = updater_init(versionURL, pluginName,
                     currentVersion, this);
        if (!updater) {
            DBG("Updater is null\n");
            return;
        }
        if (!force)
        {
            auto dayAgo = Time::getCurrentTime() - RelativeTime::hours(24);
            DBG("Last check: " << lastCheck);
            DBG("24hr ago: " << dayAgo.toMilliseconds());
            if (lastCheck > dayAgo.toMilliseconds())
            {
                DBG("Checked within 24 hours");
                updateStatus.state = UpdateStatus::Finished;
                updateStatus.updateAvailable = false;
                updater_deinit(updater);
                return;
            }
        }

        DBG("Checking for update");
        updater_fetch(updater, onUpdateCheck);
    }

    void downloadUpdate()
    {
        // download new binary
        if (updater) {
            updater_download_bin(updater, DownloadOptions{
                                     .progress = downloadProgress,
                                     .finished = downloadFinished,
                                     .dest_file = downloadPath.toRawUTF8(),
                                     .chunk_size = 64 * 1024,
                                 });
        } else {
            DBG("Updater is null\n");
        }
        no.setButtonText("Cancel");
        yes.setVisible(false);
    }

    void paint(Graphics &g) override
    {
        // NOTE: IS this really necessary? Why did I put this here?
        if (isVisible())
        {
            g.setColour(Colours::grey.darker());
            g.fillRoundedRectangle(getLocalBounds().toFloat(), 15.f);

            g.setColour(Colours::white);

            auto textbounds = Rectangle<int>{
                getLocalBounds().reduced(10).withTrimmedBottom(70)};

            if (!updater) {
                DBG("Updater is null\n");
                return;
            }
            if (dlStatus.state == DownloadStatus::NotStarted) {
                if (updateStatus.updateAvailable) {
                    g.drawFittedText("A new update is available!\n" +
                                     String(updater_get_message(updater)),
                                     textbounds, Justification::centredTop, 10,
                                     0.f);
                }
            } else {
                if (dlStatus.state == DownloadStatus::Downloading) {
                    g.drawFittedText("Downloading... " +
                                         String(dlStatus.progress) + "%",
                                     textbounds, Justification::centred, 1,
                                     1.f);
                }
                if (dlStatus.state == DownloadStatus::Finished) {
                    if (dlStatus.ok) {
                        g.drawFittedText("Download complete.\nThe installer is in "
                                         "your Downloads folder. You must close "
                                         "your DAW to run the installation.",
                                         textbounds, Justification::centred, 7,
                                         1.f);
                        yes.setVisible(false);
                        no.setButtonText("Close");
                    }
                    else {
                        g.drawFittedText("Download failed\n" +
                                         String(updater_get_message(updater)),
                                         textbounds, Justification::centred,
                                         7, 1.f);
                        yes.setVisible(true);
                        yes.setButtonText("Retry");
                        yes.onClick = [this]
                        {
                            dlStatus = DownloadStatus{
                                .state = DownloadStatus::Downloading
                            };
                            downloadUpdate();
                        };
                    }
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

        Rectangle<int> yesBounds{bounds.withTrimmedTop(halfHeight)
                                     .withTrimmedRight(halfWidth)
                                     .reduced(20, 30)};
        Rectangle<int> noBounds{bounds.withTrimmedTop(halfHeight)
                                    .withTrimmedLeft(halfWidth)
                                    .reduced(20, 30)};

        yes.setBounds(yesBounds);
        no.setBounds(noBounds);
    }

    const String url, downloadPath;

    UpdateStatus updateStatus;
    DownloadStatus dlStatus;

    std::atomic<bool> needsRepaint = false;

   // bool shouldBeHidden = false; // set to true when "No" is clicked

private:
    Updater *updater = nullptr;

    TextButton yes{"Yes"}, no{"No"};
};

static void downloadFinished(Updater *updater, bool ok, size_t size, void *user_data)
{
    DownloadManager *dl = (DownloadManager*)user_data;
    dl->dlStatus.ok = ok;
    dl->dlStatus.state = DownloadStatus::Finished;
    dl->dlStatus.totalSize = size;
    
    dl->needsRepaint = true;
};

static void downloadProgress(Updater *updater, size_t read, size_t total, void *user_data)
{
    DownloadManager *dl = (DownloadManager*)user_data;
    dl->dlStatus.progress = ((float)read / (float)total) * 100.f;
    dl->needsRepaint = true;
};

static void onUpdateCheck(Updater *updater, bool checkResult, void *user_data)
{
    DownloadManager *dl = (DownloadManager*)user_data;
    dl->updateStatus.updateAvailable = checkResult;
    if (updater) {
        dl->updateStatus.changes.fromUTF8(updater_get_message(updater));
    } else {
        DBG("Updater is null\n");
        dl->updateStatus.changes = "Updater is null";
    }
    dl->updateStatus.state = UpdateStatus::Finished;

    if (checkResult) {
        DBG("Update available");
        dl->setVisible(true);
    } else {
        DBG("No update");
        dl->setVisible(false);
    }
    dl->needsRepaint = true;
}
