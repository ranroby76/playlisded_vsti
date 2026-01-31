/*
  ==============================================================================

    MainComponent.cpp
    Playlisted2

    VSTi Fix: Removed global LookAndFeel setting to avoid interfering with DAW
    or other plugin instances. Applied LookAndFeel locally instead.

  ==============================================================================
*/

#include "MainComponent.h"
#include "../AppLogger.h"
#include "../RegistrationManager.h"

using namespace juce;

MainComponent::MainComponent(AudioEngine& engine, IOSettingsManager& settings)
    : audioEngine(engine),
      ioSettingsManager(settings),
      header(engine)
{
    LOG_INFO("=== MainComponent UI Constructed (Linked to Processor) ===");
    
    // 1. License Check
    RegistrationManager::getInstance().checkRegistration();

    // 2. Load Settings 
    ioSettingsManager.loadSettings();

    // 3. L&F - FIX: Apply Locally for VST Safety
    goldenLookAndFeel = std::make_unique<GoldenSliderLookAndFeel>();
    setLookAndFeel(goldenLookAndFeel.get()); // Apply only to this component and children

    // 4. Add Components
    addAndMakeVisible(header);

    // 5. Create Media Page
    mediaPage = std::make_unique<MediaPage>(audioEngine, ioSettingsManager);
    addAndMakeVisible(mediaPage.get());

    setSize(1000, 700);
}

MainComponent::~MainComponent()
{
    setLookAndFeel(nullptr);
}

void MainComponent::paint(Graphics& g)
{
    g.fillAll(Colour(0xFF202020));
}

void MainComponent::resized()
{
    auto area = getLocalBounds();
    header.setBounds(area.removeFromTop(60));

    if (mediaPage)
        mediaPage->setBounds(area);
}