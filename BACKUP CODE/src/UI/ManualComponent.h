#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class ManualComponent : public juce::Component
{
public:
    ManualComponent()
    {
        // Define Pages
        pageTitles = { 
            "1. What is Playlisted?", 
            "2. Plugin Operation", 
            "3. MIDI Support", 
            "4. Tooltips & Help", 
            "5. Registration" 
        };

        pageContents = {
            // 1. What is Playlisted?
            "WHAT IS PLAYLISTED?\n\n"
            "Welcome to Playlisted! This plugin is a unique media engine designed to run inside your DAW (Digital Audio Workstation). "
            "It bridges the gap between standard music production tools and live performance media players.\n\n"
            "What does it do?\n"
            "Playlisted allows you to load lists of audio or video files (MP3, WAV, MP4, AVI, etc.) and play them back directly "
            "through your DAW's audio engine. It handles video decoding in a separate high-performance window while keeping audio "
            "perfectly synced.\n\n"
            "Key Features:\n"
            "- Real-time Pitch Shifting: Change the key of a song (+/- 12 semitones) without affecting the speed.\n"
            "- Speed Control: Slow down or speed up practice tracks (0.1x to 2.1x).\n"
            "- Auto-Wait Logic: Set specific countdown times between tracks for seamless live sets.",

            // 2. Plugin Operation
            "PLUGIN OPERATION\n\n"
            "The Interface is divided into the Playlist (top) and the Player (bottom).\n\n"
            "1. Managing Files:\n"
            "   Use the buttons to 'Add Media Files', 'Clear', 'Save', or 'Load' playlists. You can also set a 'Default Folder' "
            "to open your favorite directory quickly.\n\n"
            "2. Track Controls (The Banner):\n"
            "   Each track has an expansion arrow ('v'). Click it to reveal advanced controls:\n"
            "   - Vol: Individual track volume.\n"
            "   - Pitch: Shift the key up or down by 12 semitones.\n"
            "   - Speed: Change playback rate.\n"
            "   - Wait: Set a delay (in seconds). When the track ends, Playlisted will count down this duration before automatically "
            "starting the next track.\n\n"
            "3. Playback:\n"
            "   Click the Green Triangle on any track to load and select it. Use the main PLAY/STOP buttons at the bottom to control playback.\n\n"
            "4. Video:\n"
            "   If you load a video file, click 'SHOW VIDEO' to open the projection window.",

            // 3. MIDI Support
            "MIDI SUPPORT\n\n"
            "Playlisted is designed for hands-free control using any MIDI keyboard or controller.\n\n"
            "Fixed Mappings:\n"
            "- Note 15: Play / Pause toggle.\n"
            "- Note 16: Stop (and return to zero).\n"
            "- Note 17: Show / Hide the Video Window.\n\n"
            "Setup:\n"
            "Simply route a MIDI track in your DAW to the Playlisted plugin. Ensure your controller is sending on MIDI Channel 1 (or Omni).",

            // 4. Tooltips & Help
            "TOOLTIPS & HELP\n\n"
            "Unsure what a specific button or slider does?\n\n"
            "Just RIGHT-CLICK on it!\n\n"
            "Almost every control in Playlisted has a built-in help bubble. Right-clicking will show you the control's name, "
            "its current value, and any MIDI notes assigned to it.\n\n"
            "This works on the main buttons, the sliders inside the playlist, and even the header buttons.",

            // 5. Registration
            "REGISTRATION\n\n"
            "Playlisted operates in two modes:\n\n"
            "1. FREE Mode:\n"
            "   Fully functional, but limited to a maximum of 3 tracks per playlist. You can use all features (Pitch, Video, Speed), "
            "but you cannot add a 4th file.\n\n"
            "2. PRO Mode:\n"
            "   Unlimited tracks and playlists.\n\n"
            "How to Upgrade:\n"
            "1. Click the 'REGISTER' button in the top header.\n"
            "2. Copy your 'User ID' and send it to us / enter it on the website.\n"
            "3. Paste the 'Serial Number' you receive back into the box.\n"
            "4. Click 'Save License File'.\n\n"
            "Thank you for supporting independent audio development!"
        };

        // Create Navigation Buttons
        for (int i = 0; i < pageTitles.size(); ++i)
        {
            auto* btn = new juce::TextButton();
            btn->setButtonText(juce::String(i + 1));
            btn->setTooltip(pageTitles[i]); // Hover to see page name
            btn->setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF2A2A2A));
            btn->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
            btn->onClick = [this, i] { setPage(i); };
            addAndMakeVisible(btn);
            navButtons.add(btn);
        }

        // Title Header
        addAndMakeVisible(pageHeaderLabel);
        pageHeaderLabel.setFont(juce::Font(22.0f, juce::Font::bold));
        pageHeaderLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFD4AF37)); // Gold
        pageHeaderLabel.setJustificationType(juce::Justification::centred);

        // Content Area
        addAndMakeVisible(contentView);
        contentView.setMultiLine(true);
        contentView.setReadOnly(true);
        contentView.setCaretVisible(false);
        contentView.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xFF151515));
        contentView.setColour(juce::TextEditor::textColourId, juce::Colours::white);
        contentView.setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        contentView.setFont(juce::Font(16.0f));

        setSize(600, 500);
        setPage(0); // Load Page 1
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xFF202020)); // Background
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(15);
        // Navigation Bar
        auto navArea = area.removeFromTop(30);
        int btnWidth = navArea.getWidth() / navButtons.size();
        for (auto* btn : navButtons)
        {
            btn->setBounds(navArea.removeFromLeft(btnWidth).reduced(2, 0));
        }

        area.removeFromTop(10);
        
        // Page Title
        pageHeaderLabel.setBounds(area.removeFromTop(30));
        // Content
        area.removeFromTop(10);
        contentView.setBounds(area);
    }

private:
    void setPage(int index)
    {
        if (index < 0 || index >= pageTitles.size()) return;
        // Update Buttons Visuals
        for (int i = 0; i < navButtons.size(); ++i)
        {
            if (i == index) {
                navButtons[i]->setColour(juce::TextButton::buttonColourId, juce::Colour(0xFFD4AF37)); // Gold
                navButtons[i]->setColour(juce::TextButton::textColourOffId, juce::Colours::black);
            } else {
                navButtons[i]->setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF2A2A2A)); // Dark
                navButtons[i]->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
            }
        }

        // Update Text
        pageHeaderLabel.setText(pageTitles[index], juce::dontSendNotification);
        contentView.setText(pageContents[index]);
        contentView.moveCaretToTop(false);
    }

    juce::StringArray pageTitles;
    juce::StringArray pageContents;
    juce::OwnedArray<juce::TextButton> navButtons;
    juce::Label pageHeaderLabel;
    juce::TextEditor contentView;
};