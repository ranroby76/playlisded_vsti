/*
  ==============================================================================

    PlaylistComponent.cpp
    Playlisted2

  ==============================================================================
*/

#include "PlaylistComponent.h"
#include "../RegistrationManager.h"

using namespace juce;

PlaylistComponent::PlaylistComponent(AudioEngine& engine, IOSettingsManager& settings)
    : audioEngine(engine), ioSettings(settings), playlist(engine.getPlaylist()) 
{
    addAndMakeVisible(headerLabel);
    headerLabel.setText("PLAYLIST", dontSendNotification);
    headerLabel.setFont(Font(18.0f, Font::bold));
    headerLabel.setColour(Label::textColourId, Colour(0xFFD4AF37));
    headerLabel.setJustificationType(Justification::centredLeft);

    addAndMakeVisible(autoPlayToggle);
    autoPlayToggle.setButtonText("Auto-Play");
    autoPlayToggle.setToggleState(true, dontSendNotification);
    autoPlayToggle.setColour(ToggleButton::textColourId, Colours::white);
    autoPlayToggle.setColour(ToggleButton::tickColourId, Colour(0xFFD4AF37));
    autoPlayToggle.onClick = [this] { autoPlayEnabled = autoPlayToggle.getToggleState(); };

    // --- BUTTON ROW INITIALIZATION (5 Buttons) ---
    
    // 1. Add Files
    addAndMakeVisible(addTrackButton);
    addTrackButton.setButtonText("Add Media Files");
    addTrackButton.setColour(TextButton::buttonColourId, Colour(0xFF404040));
    addTrackButton.onClick = [this] {
        File startDir = File::getSpecialLocation(File::userMusicDirectory);
        String savedPath = ioSettings.getMediaFolder();
        if (savedPath.isNotEmpty()) {
            File f(savedPath);
            if (f.isDirectory()) startDir = f;
        }

        auto fc = std::make_shared<FileChooser>("Select Media Files",
            startDir,
            "*.mp3;*.wav;*.aiff;*.flac;*.ogg;*.m4a;*.mp4;*.avi;*.mov;*.mkv;*.webm;*.mpg;*.mpeg", true);

        fc->launchAsync(FileBrowserComponent::openMode | FileBrowserComponent::canSelectMultipleItems,
            [this, fc](const FileChooser& chooser) {
                auto results = chooser.getResults();
                for (auto& file : results)
                    addTrack(file);
            });
    };

    // 2. Clear
    addAndMakeVisible(clearButton);
    clearButton.setButtonText("Clear Playlist");
    clearButton.setColour(TextButton::buttonColourId, Colour(0xFF8B0000));
    clearButton.onClick = [this] { clearPlaylist(); };

    // 3. Save
    addAndMakeVisible(saveButton);
    saveButton.setButtonText("Save Playlist");
    saveButton.setColour(TextButton::buttonColourId, Colour(0xFF2A2A2A));
    saveButton.onClick = [this] { savePlaylist(); };

    // 4. Load
    addAndMakeVisible(loadButton);
    loadButton.setButtonText("Load Playlist");
    loadButton.setColour(TextButton::buttonColourId, Colour(0xFF2A2A2A));
    loadButton.onClick = [this] { loadPlaylist(); };

    // 5. Set Folder
    addAndMakeVisible(defaultFolderButton);
    defaultFolderButton.setButtonText("Set Playlist's Folder");
    defaultFolderButton.setColour(TextButton::buttonColourId, Colour(0xFF404040));
    defaultFolderButton.onClick = [this] { setDefaultFolder(); };

    addAndMakeVisible(viewport);
    viewport.setScrollBarsShown(true, false);
    viewport.setViewedComponent(&listContainer, false);

    rebuildList();
    
    // [FIX] SMART RESTORE: Check if we have a persisted track index from the engine
    int savedIndex = audioEngine.getActiveTrackIndex();
    if (!playlist.empty())
    {
        if (savedIndex >= 0 && savedIndex < (int)playlist.size())
        {
            // Restore visual selection ONLY (Do NOT call selectTrack(index) because it triggers loadFile)
            currentTrackIndex = savedIndex;
            updateBannerVisuals();
            scrollToBanner(currentTrackIndex);
        }
        else
        {
            // First run or invalid state -> Default to 0 and load it
            currentTrackIndex = 0;
            selectTrack(0);
        }
    }

    startTimerHz(30);
}

PlaylistComponent::~PlaylistComponent()
{
    stopTimer();
    banners.clear();
}

void PlaylistComponent::paint(Graphics& g)
{
    g.fillAll(Colour(0xFF222222));
}

void PlaylistComponent::resized()
{
    auto area = getLocalBounds().reduced(12);
    auto row1 = area.removeFromTop(35);
    headerLabel.setBounds(row1.removeFromLeft(120).reduced(5, 0));
    autoPlayToggle.setBounds(row1.removeFromRight(100).reduced(5, 0));

    // Button Row: 5 buttons evenly spaced
    auto row2 = area.removeFromTop(40);
    int numButtons = 5;
    int spacing = 4;
    int totalSpacing = (numButtons - 1) * spacing;
    int btnWidth = (row2.getWidth() - totalSpacing) / numButtons;

    addTrackButton.setBounds(row2.removeFromLeft(btnWidth));
    row2.removeFromLeft(spacing);
    clearButton.setBounds(row2.removeFromLeft(btnWidth));
    row2.removeFromLeft(spacing);
    saveButton.setBounds(row2.removeFromLeft(btnWidth));
    row2.removeFromLeft(spacing);
    loadButton.setBounds(row2.removeFromLeft(btnWidth));
    row2.removeFromLeft(spacing);
    defaultFolderButton.setBounds(row2.removeFromLeft(btnWidth));
    
    viewport.setBounds(area);
    rebuildList();
}

void PlaylistComponent::setDefaultFolder()
{
    auto fc = std::make_shared<FileChooser>("Choose Default Media Folder",
        File::getSpecialLocation(File::userMusicDirectory));

    fc->launchAsync(FileBrowserComponent::openMode | FileBrowserComponent::canSelectDirectories,
        [this, fc](const FileChooser& chooser) {
            auto result = chooser.getResult();
            if (result.isDirectory()) {
                ioSettings.saveMediaFolder(result.getFullPathName());
                NativeMessageBox::showMessageBoxAsync(AlertWindow::InfoIcon, "Success", 
                    "Default media folder set to:\n" + result.getFileName());
            }
        });
}

void PlaylistComponent::addTrack(const File& file)
{
    if (!RegistrationManager::getInstance().isProMode() && playlist.size() >= 3)
    {
        NativeMessageBox::showMessageBoxAsync(AlertWindow::InfoIcon, 
            "Free Mode", 
            "Free Mode is limited to 3 tracks maximum.\n\nPlease click 'REGISTER' to unlock Pro Mode and unlimited tracks.");
        return;
    }

    PlaylistItem item;
    item.filePath = file.getFullPathName();
    item.title = file.getFileNameWithoutExtension();
    item.volume = 1.0f;
    item.playbackSpeed = 1.0f;
    item.pitchSemitones = 0;
    item.transitionDelaySec = 0;
    item.isCrossfade = false; 
    
    playlist.push_back(item);
    
    // Default Selection Logic: If first track, select it.
    if (playlist.size() == 1)
    {
        currentTrackIndex = 0;
        selectTrack(0);
    }
    
    rebuildList();
}

void PlaylistComponent::clearPlaylist()
{
    playlist.clear();
    banners.clear();
    currentTrackIndex = -1;
    audioEngine.setActiveTrackIndex(-1);
    // [FIX] Clear engine state too
    waitingForTransition = false;
    audioEngine.stopAllPlayback();
    rebuildList();
}

void PlaylistComponent::removeTrack(int index)
{
    if (index >= 0 && index < (int)playlist.size())
    {
        playlist.erase(playlist.begin() + index);

        if (currentTrackIndex == index) 
        {
            currentTrackIndex = -1;
            audioEngine.setActiveTrackIndex(-1);
            waitingForTransition = false;
            audioEngine.stopAllPlayback();
        }
        else if (currentTrackIndex > index)
        {
            currentTrackIndex--;
            audioEngine.setActiveTrackIndex(currentTrackIndex); // Sync decrement
        }
        rebuildList();
    }
}

void PlaylistComponent::selectTrack(int index)
{
    if (index < 0 || index >= (int)playlist.size()) return;
    
    currentTrackIndex = index;
    audioEngine.setActiveTrackIndex(index);
    // [FIX] Persist selection to Engine
    
    waitingForTransition = false;
    
    auto& item = playlist[index];
    if (audioEngine.getMediaPlayer().loadFile(item.filePath))
    {
        audioEngine.getMediaPlayer().setVolume(item.volume);
        audioEngine.getMediaPlayer().setRate(item.playbackSpeed);
        audioEngine.setPitchSemitones(item.pitchSemitones);
    }
    
    updateBannerVisuals();
}

void PlaylistComponent::playTrack(int index)
{
    selectTrack(index); 
    audioEngine.getMediaPlayer().play();
}

void PlaylistComponent::scrollToBanner(int index)
{
    if (index < 0 || index >= banners.size()) return;
    if (auto* banner = banners[index])
    {
        viewport.setViewPosition(0, banner->getY());
    }
}

void PlaylistComponent::rebuildList()
{
    banners.clear();
    listContainer.removeAllChildren();
    
    int y = 0;
    for (size_t i = 0; i < playlist.size(); ++i)
    {
        auto& item = playlist[i];
        int currentH = item.isExpanded ? 170 : 44; 
        
        auto* banner = new TrackBannerComponent((int)i, item, 
            [this, i] { removeTrack((int)i); }, 
            [this, i] { 
                playlist[i].isExpanded = !playlist[i].isExpanded; 
                rebuildList(); 
            }, 
            // FIX: TRIANGLE CLICK (LOAD ONLY)
            // The green triangle now STRICTLY selects/loads the track.
            // It will NEVER start playback, ensuring Play/Stop buttons have exclusive transport control.
            [this, i] { 
                selectTrack((int)i);
            }, 
            [this, i](float vol) { 
                if (currentTrackIndex == (int)i) 
                    audioEngine.getMediaPlayer().setVolume(vol);
            },
            [this, i](int semitones) {
                if (currentTrackIndex == (int)i)
                    audioEngine.setPitchSemitones(semitones);
            },
            [this, i](float speed) {
                if (currentTrackIndex == (int)i) 
                    audioEngine.getMediaPlayer().setRate(speed);
            }
        );

        banner->setBounds(0, y, viewport.getWidth(), currentH);
        listContainer.addAndMakeVisible(banner);
        banners.add(banner);
        
        y += currentH + 2;
    }
    
    listContainer.setSize(viewport.getWidth(), y + 50);
    updateBannerVisuals();
}

void PlaylistComponent::updateBannerVisuals()
{
    for (int i = 0; i < banners.size(); ++i)
    {
        bool isCurrent = (i == currentTrackIndex);
        banners[i]->setPlaybackState(isCurrent, audioEngine.getMediaPlayer().isPlaying());
    }
}

void PlaylistComponent::timerCallback()
{
    if ((int)playlist.size() != banners.size())
    {
        rebuildList();
        // ENSURE DEFAULT SELECTION
        if (currentTrackIndex == -1 && !playlist.empty())
        {
            currentTrackIndex = 0;
            selectTrack(0);
        }
    }

    if (autoPlayEnabled && currentTrackIndex >= 0 && currentTrackIndex < (int)playlist.size())
    {
        auto& player = audioEngine.getMediaPlayer();
        bool hasFinished = player.hasFinished();
        
        if (waitingForTransition)
        {
            if (transitionCountdown > 0)
            {
                transitionCountdown--;
            }
            else
            {
                waitingForTransition = false;
                int nextIndex = currentTrackIndex + 1;
                if (nextIndex < (int)playlist.size())
                {
                    playTrack(nextIndex);
                    if (autoPlayEnabled) scrollToBanner(nextIndex);
                }
                else
                {
                    audioEngine.stopAllPlayback();
                }
            }
            finishDebounceCounter = 0;
        }
        else if (hasFinished) 
        {
            finishDebounceCounter++;
            if (finishDebounceCounter > 6)
            {
                auto& currentItem = playlist[currentTrackIndex];
                int nextIndex = currentTrackIndex + 1;
                
                if (nextIndex < (int)playlist.size())
                {
                    waitingForTransition = true;
                    player.pause(); 

                    int sec = currentItem.transitionDelaySec;
                    transitionCountdown = (sec > 0) ? (sec * 30) : 15;
                }
            }
        }
        else
        {
            finishDebounceCounter = 0;
        }
    }
    updateBannerVisuals();
}

void PlaylistComponent::savePlaylist()
{
    auto fc = std::make_shared<FileChooser>("Save Playlist",
        File::getSpecialLocation(File::userDocumentsDirectory), "*.json");

    fc->launchAsync(FileBrowserComponent::saveMode | FileBrowserComponent::canSelectFiles,
        [this, fc](const FileChooser& chooser) {
            auto file = chooser.getResult();
            if (file == File{}) return; 

            if (!file.hasFileExtension("json"))
                file = file.withFileExtension("json");

            DynamicObject::Ptr root = new DynamicObject();
            Array<var> tracks;
            
            for (const auto& item : playlist)
            {
                DynamicObject::Ptr obj = new DynamicObject();
                obj->setProperty("path", item.filePath);
                obj->setProperty("title", item.title);
                obj->setProperty("vol", item.volume);
                obj->setProperty("pitch", item.pitchSemitones);
                obj->setProperty("speed", item.playbackSpeed);
                obj->setProperty("delay", item.transitionDelaySec);
                obj->setProperty("xfade", item.isCrossfade);
                tracks.add(obj.get());
            }

            root->setProperty("tracks", tracks);
            if (file.replaceWithText(JSON::toString(var(root.get()))))
            {
                NativeMessageBox::showMessageBoxAsync(AlertWindow::InfoIcon, "Success", 
                    "Playlist saved successfully!");
            }
            else
            {
                NativeMessageBox::showMessageBoxAsync(AlertWindow::WarningIcon, "Error", 
                    "Could not write to file.");
            }
        });
}

void PlaylistComponent::loadPlaylist()
{
    auto fc = std::make_shared<FileChooser>("Load Playlist",
        File::getSpecialLocation(File::userDocumentsDirectory), "*.json");

    fc->launchAsync(FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
        [this, fc](const FileChooser& chooser) {
            auto file = chooser.getResult();
            if (!file.existsAsFile()) return;
            
            var json = JSON::parse(file);
            if (auto* root = json.getDynamicObject())
            {
                if (auto* tracks = root->getProperty("tracks").getArray())
                {
                    clearPlaylist();
                    
                    bool isPro = RegistrationManager::getInstance().isProMode();
                    int max = isPro ? 99999 : 3;
                    int count = 0;
                    
                    for (auto& t : *tracks)
                    {
                        if (count >= max) break;
                        
                        if (auto* obj = t.getDynamicObject())
                        {
                            PlaylistItem item;
                            item.filePath = obj->getProperty("path").toString();
                            if (obj->hasProperty("title"))
                                item.title = obj->getProperty("title").toString();
                            else
                                item.ensureTitle();
                            
                            if (File(item.filePath).existsAsFile())
                            {
                                item.volume = (float)obj->getProperty("vol");
                                if (obj->hasProperty("pitch")) item.pitchSemitones = (int)obj->getProperty("pitch");
                                item.playbackSpeed = (float)obj->getProperty("speed");
                                item.transitionDelaySec = (int)obj->getProperty("delay");
                                item.isCrossfade = (bool)obj->getProperty("xfade");
                                playlist.push_back(item);
                                count++;
                            }
                        }
                    }
                    
                    rebuildList();
                    // Explicitly select first track after load
                    if (!playlist.empty()) selectTrack(0);
                }
            }
            else
            {
                NativeMessageBox::showMessageBoxAsync(AlertWindow::WarningIcon, "Error", 
                    "Failed to parse playlist file.");
            }
        });
}