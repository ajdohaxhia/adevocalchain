#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Engine.h"
#include "Parameters.h"
#include "Presets.h"

namespace vr
{

class VocalRackAudioProcessor final : public juce::AudioProcessor,
                                      private juce::AudioProcessorValueTreeState::Listener,
                                      private juce::Timer
{
public:
    VocalRackAudioProcessor();
    ~VocalRackAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "ademusic Vocal Rack"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts; }
    Engine& getEngine() noexcept { return engine; }
    PresetManager& getPresets() noexcept { return presetManager; }

    // UI / preset orchestration (message thread).
    void requestPresetLoad (const juce::String& presetJsonOrId, bool keepSongKey);
    void requestStateApply (const juce::ValueTree& state, bool markEdited);
    void requestQuality (QualityMode mode);
    QualityMode getQuality() const noexcept { return qualityAtomic.load(); }
    int getReportedLatencySamples() const noexcept { return reportedLatency.load(); }

    bool saveCurrentAsUserPreset (const juce::String& name, const juce::String& bank,
                                  const juce::String& description, const juce::String& useCase,
                                  const juce::String& firstAdjustment, juce::String& error);
    bool importPresetFile (const juce::File& file, juce::String& error);
    bool exportPresetFile (const juce::String& id, const juce::File& file, juce::String& error);
    bool deleteUserPreset (const juce::String& id, juce::String& error);
    bool renameUserPreset (const juce::String& id, const juce::String& newName, juce::String& error);
    bool duplicateUserPreset (const juce::String& id, const juce::String& newName, juce::String& error);

    void switchAB (int slot);
    void copyAB (int from, int to);
    int getActiveAB() const noexcept { return activeAB; }
    void setKeepSongKey (bool shouldKeep) noexcept { keepSongKey = shouldKeep; notifyStateChanged(); }
    bool getKeepSongKey() const noexcept { return keepSongKey; }
    bool isEdited() const noexcept { return edited; }
    juce::String getPresetName() const { return currentPresetName; }
    juce::String getPresetId() const { return currentPresetId; }
    juce::String getPresetBank() const { return currentPresetBank; }
    bool isFactoryPreset() const noexcept { return currentPresetFactory; }
    void notifyStateChanged();

    std::function<void()> onStateChanged;
    std::function<void (const juce::String&)> onNotice;

    // Host bypass parameter is the same as the UI bypass parameter.
    juce::AudioProcessorParameter* getBypassParameter() const override;

private:
    void parameterChanged (const juce::String&, float) override;
    void timerCallback() override;
    void applyQualityOnMessageThread (QualityMode mode);
    void updateHostLatency();
    void publishMeters();
    void applyStateInternal (const juce::ValueTree& state, bool markEdited);
    void syncMaskFromKey();

    juce::AudioProcessorValueTreeState apvts;
    ParamCache paramCache;
    Engine engine;
    PresetManager presetManager;

    std::atomic<QualityMode> qualityAtomic { QualityMode::track };
    std::atomic<int> reportedLatency { 0 };
    std::atomic<bool> processing { false };

    double currentSampleRate = 48000.0;
    int currentBlockSize = 512;
    int currentInputChannels = 2;
    int currentOutputChannels = 2;

    juce::String currentPresetId { "init" };
    juce::String currentPresetName { "Init" };
    juce::String currentPresetBank { "Init" };
    bool currentPresetFactory = false;
    bool edited = false;
    bool keepSongKey = true;
    bool applyingState = false;
    bool updatingMask = false;

    juce::ValueTree abStates[2];
    bool abValid[2] { false, false };
    int activeAB = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalRackAudioProcessor)
};

} // namespace vr
