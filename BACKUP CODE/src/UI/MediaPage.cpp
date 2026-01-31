#include "MediaPage.h"

// NEW: Subclass for Show Video Button to add Tooltip and Black Text
class ShowVideoButton : public juce::TextButton, public LongPressDetector
{
public:
    ShowVideoButton() : juce::TextButton("SHOW VIDEO") 
    {
        setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF4A90E2)); 
        setColour(juce::TextButton::textColourOffId, juce::Colours::black); 
        setTooltip("Opens the video window if closed");
    }

    void onLongPress() override 
    {
        showMidiTooltip(this, "Show Video Window\nMIDI: Note 17");
    }

    void mouseDown(const juce::MouseEvent& e) override 
    { 
        if (e.mods.isRightButtonDown()) { onLongPress(); return; }
        handleMouseDown(e); 
        juce::TextButton::mouseDown(e); 
    }
    void mouseUp(const juce::MouseEvent& e) override { handleMouseUp(e); if (e.mods.isRightButtonDown() || isLongPressTriggered) return; juce::TextButton::mouseUp(e); }
    void mouseDrag(const juce::MouseEvent& e) override { handleMouseDrag(e); if (!isLongPressTriggered) juce::TextButton::mouseDrag(e); }
};

MediaPage::MediaPage(AudioEngine& engine, IOSettingsManager& settings) : audioEngine(engine),
    progressSlider(juce::Slider::LinearBar, juce::Slider::NoTextBox) 
{
    playlistComponent = std::make_unique<PlaylistComponent>(engine, settings);
    addAndMakeVisible(playlistComponent.get());

    addAndMakeVisible(playPauseBtn);
    playPauseBtn.setButtonText("PLAY");
    playPauseBtn.setMidiInfo("MIDI: Note 15");
    playPauseBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF2A2A2A));
    playPauseBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    playPauseBtn.onClick = [this] { 
        auto& player = audioEngine.getMediaPlayer();
        if (player.isPlaying()) player.pause();
        else player.play();
    };

    addAndMakeVisible(stopBtn);
    stopBtn.setButtonText("STOP");
    stopBtn.setMidiInfo("MIDI: Note 16");
    stopBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF2A2A2A));
    stopBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    stopBtn.onClick = [this] { 
        audioEngine.stopAllPlayback();
        playPauseBtn.setButtonText("PLAY");
        progressSlider.setValue(0.0, juce::dontSendNotification);
    };

    // Use new button class
    showVideoBtn = std::make_unique<ShowVideoButton>();
    addAndMakeVisible(showVideoBtn.get());
    showVideoBtn->onClick = [this] { 
        audioEngine.showVideoWindow();
    };

    addAndMakeVisible(progressSlider);
    progressSlider.setRange(0.0, 1.0, 0.001);
    progressSlider.setColour(juce::Slider::trackColourId, juce::Colour(0xFFD4AF37));
    progressSlider.setColour(juce::Slider::backgroundColourId, juce::Colour(0xFF404040));
    progressSlider.onDragStart = [this] { isUserDraggingSlider = true; };
    progressSlider.onDragEnd = [this] { 
        isUserDraggingSlider = false;
        audioEngine.getMediaPlayer().setPosition((float)progressSlider.getValue());
    };
    progressSlider.onValueChange = [this] {
        if (isUserDraggingSlider) audioEngine.getMediaPlayer().setPosition((float)progressSlider.getValue());
    };

    addAndMakeVisible(currentTimeLabel);
    currentTimeLabel.setText("00:00", juce::dontSendNotification);
    currentTimeLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFD4AF37));
    currentTimeLabel.setJustificationType(juce::Justification::centredRight);
    currentTimeLabel.setFont(juce::Font(16.0f, juce::Font::bold));

    addAndMakeVisible(totalTimeLabel);
    totalTimeLabel.setText("00:00", juce::dontSendNotification);
    totalTimeLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    totalTimeLabel.setJustificationType(juce::Justification::centredLeft);
    totalTimeLabel.setFont(juce::Font(16.0f, juce::Font::bold));

    addAndMakeVisible(countdownLabel);
    countdownLabel.setText("", juce::dontSendNotification);
    countdownLabel.setColour(juce::Label::textColourId, juce::Colours::red);
    countdownLabel.setJustificationType(juce::Justification::centred); 
    countdownLabel.setFont(juce::Font(18.0f, juce::Font::bold));

    startTimerHz(30);
}

MediaPage::~MediaPage() { stopTimer(); }

juce::String MediaPage::formatTime(double seconds) const
{
    if (seconds < 0) seconds = 0;
    int totalSeconds = (int)seconds;
    int m = totalSeconds / 60;
    int s = totalSeconds % 60;
    return juce::String::formatted("%02d:%02d", m, s);
}

void MediaPage::timerCallback()
{
    auto& player = audioEngine.getMediaPlayer();
    bool isPlaying = player.isPlaying();
    playPauseBtn.setButtonText(isPlaying ? "PAUSE" : "PLAY");
    playPauseBtn.setColour(juce::TextButton::textColourOffId, isPlaying ? juce::Colour(0xFFD4AF37) : juce::Colours::white);

    // NEW: Visibility Check for Video Button
    if (showVideoBtn)
        showVideoBtn->setVisible(!player.isWindowOpen());

    if (!isUserDraggingSlider && isPlaying)
    {
        progressSlider.setValue(player.getPosition(), juce::dontSendNotification);
    }

    double lenMs = (double)player.getLengthMs();
    double posRatio = player.getPosition();
    double currentMs = lenMs * posRatio;

    totalTimeLabel.setText(formatTime(lenMs / 1000.0), juce::dontSendNotification);
    currentTimeLabel.setText(formatTime(currentMs / 1000.0), juce::dontSendNotification);

    int remaining = playlistComponent->getWaitSecondsRemaining();
    if (remaining > 0)
    {
        countdownLabel.setText("Next track in: " + juce::String(remaining), juce::dontSendNotification);
        countdownLabel.setVisible(true);
    }
    else
    {
        countdownLabel.setVisible(false);
    }
}

void MediaPage::paint(juce::Graphics& g) 
{ 
    g.fillAll(juce::Colour(0xFF202020));
}

void MediaPage::resized()
{
    auto area = getLocalBounds();
    auto transportArea = area.removeFromBottom(60); 
    transportArea.removeFromTop(5); 

    auto countdownArea = area.removeFromBottom(25);
    countdownLabel.setBounds(countdownArea);

    int btnHeight = 40;
    int btnY = (transportArea.getHeight() - btnHeight) / 2;
    int startX = 5;
    int spacing = 10;
    int btnW = 80;
    int videoBtnW = 100;

    playPauseBtn.setBounds(startX, transportArea.getY() + btnY, btnW, btnHeight);
    stopBtn.setBounds(playPauseBtn.getRight() + spacing, transportArea.getY() + btnY, btnW, btnHeight);
    
    // Position show video button
    if (showVideoBtn)
        showVideoBtn->setBounds(stopBtn.getRight() + spacing, transportArea.getY() + btnY, videoBtnW, btnHeight);

    int clockW = 60;
    totalTimeLabel.setBounds(transportArea.removeFromRight(clockW));
    
    auto sliderArea = transportArea;
    sliderArea.removeFromLeft(startX + btnW*2 + videoBtnW + spacing*2 + 10);
    
    currentTimeLabel.setBounds(sliderArea.removeFromLeft(clockW));
    progressSlider.setBounds(sliderArea.reduced(5, 15)); 

    playlistComponent->setBounds(area);
}