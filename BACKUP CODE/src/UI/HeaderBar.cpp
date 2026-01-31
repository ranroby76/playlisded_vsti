/*
  ==============================================================================

    HeaderBar.cpp
    Playlisted2

    Fix: Dynamic scaling for Playlisted2 logo (634x109) to fit header height.
  ==============================================================================
*/

#include "HeaderBar.h"
#include <juce_graphics/juce_graphics.h>
#include "BinaryData.h"
#include "RegistrationComponent.h"
#include "ManualComponent.h" 

using namespace juce;

HeaderBar::HeaderBar(AudioEngine& engine) : audioEngine(engine)
{
    // Load Fanan Logo
    fananLogo = ImageFileFormat::loadFrom(BinaryData::logo_png, BinaryData::logo_pngSize);
    
    // Load Playlisted2 Logo
    onStageLogo = ImageFileFormat::loadFrom(BinaryData::playlisted2_png, BinaryData::playlisted2_pngSize);

    addAndMakeVisible(manualButton);
    manualButton.setButtonText("Manual");
    manualButton.setColour(TextButton::buttonColourId, Colour(0xFF2A2A2A));
    manualButton.setColour(TextButton::textColourOnId, Colour(0xFFD4AF37));
    manualButton.setColour(TextButton::textColourOffId, Colour(0xFFD4AF37));
    manualButton.onClick = [this]() {
        DialogWindow::LaunchOptions opt;
        opt.content.setOwned(new ManualComponent());
        opt.dialogTitle = "Playlisted User Manual";
        opt.componentToCentreAround = this;
        opt.dialogBackgroundColour = Colour(0xFF202020);
        opt.useNativeTitleBar = true;
        opt.resizable = false;
        opt.launchAsync();
    };

    addAndMakeVisible(registerButton);
    registerButton.setButtonText("REGISTER");
    registerButton.setColour(TextButton::buttonColourId, Colour(0xFF8B0000)); 
    registerButton.setColour(TextButton::textColourOffId, Colours::white);
    registerButton.onClick = [this]() { 
        DialogWindow::LaunchOptions opt;
        opt.content.setOwned(new RegistrationComponent());
        opt.dialogTitle = "Registration";
        opt.componentToCentreAround = this;
        opt.dialogBackgroundColour = Colour(0xFFE08020);
        opt.useNativeTitleBar = true;
        opt.resizable = false;
        opt.launchAsync();
    };
    
    addAndMakeVisible(modeLabel);
    modeLabel.setFont(Font(14.0f, Font::bold));
    modeLabel.setJustificationType(Justification::centredLeft);
    
    startTimer(1000);
    timerCallback();
}

void HeaderBar::timerCallback()
{
    bool isPro = RegistrationManager::getInstance().isProMode();
    if (isPro) {
        modeLabel.setText("PRO", dontSendNotification);
        modeLabel.setColour(Label::textColourId, Colours::lightgreen);
    } else {
        modeLabel.setText("FREE", dontSendNotification);
        modeLabel.setColour(Label::textColourId, Colours::red);
    }
}

void HeaderBar::paint(Graphics& g)
{
    g.fillAll(Colour(0xFF2D2D2D));

    auto area = getLocalBounds();
    int height = area.getHeight();
    
    // Draw Fanan Logo (Left)
    if (fananLogo.isValid())
    {
        int logoHeight = height - 20;
        int logoWidth = (int)(logoHeight * 2.303f);
        Rectangle<int> fananArea(55, (height - logoHeight) / 2, logoWidth, logoHeight);
        g.drawImageWithin(fananLogo, fananArea.getX(), fananArea.getY(), 
                         fananArea.getWidth(), fananArea.getHeight(),
                         RectanglePlacement::centred);
    }

    // Draw Playlisted2 Logo (Right)
    if (onStageLogo.isValid())
    {
        // FIX: Calculate aspect ratio dynamically from the 634x109 image
        float aspectRatio = (float)onStageLogo.getWidth() / (float)onStageLogo.getHeight();
        
        // Target height is 70% of the bar height (approx 42px)
        int logoHeight = (int)(height * 0.7f); 
        int logoWidth = (int)(logoHeight * aspectRatio);
        
        // Position on the right side
        int xPos = getWidth() - logoWidth - 15;
        
        Rectangle<int> logoArea(xPos, (height - logoHeight) / 2, logoWidth, logoHeight);
        g.drawImageWithin(onStageLogo, logoArea.getX(), logoArea.getY(),
                         logoArea.getWidth(), logoArea.getHeight(),
                         RectanglePlacement::centred);
    }
    
    g.setColour(Colours::black);
    g.fillRect(0, height - 1, getWidth(), 1);
}

void HeaderBar::resized()
{
    auto area = getLocalBounds();
    int height = area.getHeight();
    int buttonHeight = 30;
    int spacing = 10;
    int buttonY = (height - buttonHeight) / 2;

    int manualWidth = 80;
    int registerWidth = 80;
    int modeLabelWidth = 50;
    
    int totalCenterGroupWidth = manualWidth + registerWidth + modeLabelWidth + (spacing * 2);
    int startX = (getWidth() - totalCenterGroupWidth) / 2;

    manualButton.setBounds(startX, buttonY, manualWidth, buttonHeight);
    registerButton.setBounds(manualButton.getRight() + spacing, buttonY, registerWidth, buttonHeight);
    modeLabel.setBounds(registerButton.getRight() + spacing, buttonY, modeLabelWidth, buttonHeight);
}