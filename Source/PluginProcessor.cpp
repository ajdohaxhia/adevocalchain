#include "PluginProcessor.h"

#include "PluginEditor.h"

#include <cmath>

namespace vr
{

VocalRackAudioProcessor::VocalRackAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "VocalRackState", createParameterLayout()),
      paramCache (apvts)
{
    presetManager.initialise();

    for (const auto& spec : getParamSpecs())
        apvts.addParameterListener (spec.id, this);

    startTimer (300);
}

VocalRackAudioProcessor::~VocalRackAudioProcessor()
{
    stopTimer();

    for (const auto& spec : getParamSpecs())
        apvts.removeParameterListener (spec.id, this);
}

void VocalRackAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate > 1000.0 ? sampleRate : 48000.0;
    currentBlockSize = juce::jlimit (32, 8192, samplesPerBlock);
    currentInputChannels = juce::jlimit (1, 2, juce::jmax (1, getTotalNumInputChannels()));
    currentOutputChannels = juce::jlimit (1, 2, juce::jmax (1, getTotalNumOutputChannels()));

    engine.prepare (currentSampleRate, currentBlockSize, currentInputChannels,
                    currentOutputChannels, qualityAtomic.load());
    updateHostLatency();
    processing.store (false);
}

void VocalRackAudioProcessor::releaseResources()
{
    engine.releaseResources();
}

bool VocalRackAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& mainIn = layouts.getMainInputChannelSet();
    const auto& mainOut = layouts.getMainOutputChannelSet();

    if (mainIn.isDisabled() || mainOut.isDisabled())
        return false;

    if (mainIn.size() > 2 || mainOut.size() > 2)
        return false;

    // Matching mono/stereo and mono-to-stereo are supported. The engine keeps
    // an internal stereo path so a mono insert still gets usable returns.
    return mainOut.size() >= mainIn.size();
}

void VocalRackAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                            juce::MidiBuffer& midi)
{
    juce::ignoreUnused (midi);
    juce::ScopedNoDenormals noDenormals;

    if (processing.exchange (true))
    {
        buffer.clear();
        return;
    }

    if (! engine.isPrepared())
    {
        buffer.clear();
        processing.store (false);
        return;
    }

    RackParams params;
    paramCache.snapshot (params);

    TransportInfo transport;

    if (auto* playHead = getPlayHead())
    {
        if (auto position = playHead->getPosition())
        {
            transport.playing = position->getIsPlaying();

            if (auto bpm = position->getBpm())
            {
                if (*bpm > 20.0 && *bpm < 400.0)
                {
                    transport.valid = true;
                    transport.bpm = *bpm;
                }
            }
        }
    }

    engine.process (buffer, params, transport,
                    juce::jmax (1, getTotalNumInputChannels()),
                    juce::jmax (1, getTotalNumOutputChannels()));

    // Measure the processed signal before comparison gain. This keeps the
    // loudness detector independent from the gain it controls.
    const int channels = juce::jmax (1, buffer.getNumChannels());
    const int samples = buffer.getNumSamples();
    double sum = 0.0;

    for (int ch = 0; ch < channels; ++ch)
        for (int i = 0; i < samples; ++i)
        {
            const double sample = buffer.getReadPointer (ch)[i];
            sum += sample * sample;
        }

    const float rmsDb = sum > 1.0e-12
                            ? (float) (10.0 * std::log10 (sum / (double) juce::jmax (1, samples * channels)))
                            : -60.0f;
    const int slot = juce::jlimit (0, 1, activeABAtomic.load (std::memory_order_relaxed));
    auto& measured = abMeasuredRmsDb[slot];
    const float stored = measured.load (std::memory_order_relaxed);
    measured.store (stored < -59.0f ? rmsDb : stored + 0.06f * (rmsDb - stored),
                    std::memory_order_relaxed);
    currentOutputRmsDb.store (rmsDb, std::memory_order_relaxed);

    const float targetGain = isLoudnessMatch()
                                 ? comparisonTargetGain.load (std::memory_order_relaxed)
                                 : 1.0f;

    if (isLoudnessMatch() || std::abs (comparisonGain - 1.0f) > 0.0001f)
    {
        const float coefficient = 1.0f - std::exp (-1.0f / (0.05f * (float) currentSampleRate));

        for (int i = 0; i < samples; ++i)
        {
            comparisonGain += coefficient * (targetGain - comparisonGain);

            for (int ch = 0; ch < channels; ++ch)
                buffer.getWritePointer (ch)[i] *= comparisonGain;
        }
    }

    comparisonGainDbg.store (comparisonGain, std::memory_order_relaxed);
    processing.store (false);
}

juce::AudioProcessorEditor* VocalRackAudioProcessor::createEditor()
{
    return new VocalRackEditor (*this);
}

void VocalRackAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("stateVersion", 1, nullptr);
    state.setProperty ("quality", (int) qualityAtomic.load(), nullptr);
    state.setProperty ("presetId", currentPresetId, nullptr);
    state.setProperty ("presetName", currentPresetName, nullptr);
    state.setProperty ("presetBank", currentPresetBank, nullptr);
    state.setProperty ("presetFactory", currentPresetFactory, nullptr);
    state.setProperty ("edited", edited, nullptr);
    state.setProperty ("keepSongKey", keepSongKey, nullptr);
    state.setProperty ("abSlot", activeAB, nullptr);
    state.setProperty ("loudnessMatch", isLoudnessMatch(), nullptr);
    state.setProperty ("abA", abValid[0] ? abStates[0].toXmlString() : juce::String(), nullptr);
    state.setProperty ("abB", abValid[1] ? abStates[1].toXmlString() : juce::String(), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void VocalRackAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);

    if (xml == nullptr)
        return;

    auto state = juce::ValueTree::fromXml (*xml);

    if (! state.isValid() || state.getType() != juce::Identifier ("VocalRackState"))
        return;

    const int version = (int) state.getProperty ("stateVersion", 0);

    if (version > 1)
        return;

    const auto qualityValue = (int) state.getProperty ("quality", (int) QualityMode::track);
    applyQualityOnMessageThread (qualityValue == (int) QualityMode::mix ? QualityMode::mix : QualityMode::track);

    applyingState = true;
    engine.triggerStateFade();
    apvts.replaceState (state);

    currentPresetId = state.getProperty ("presetId", "custom").toString();
    currentPresetName = state.getProperty ("presetName", "Session").toString();
    currentPresetBank = state.getProperty ("presetBank", "Session").toString();
    currentPresetFactory = (bool) state.getProperty ("presetFactory", false);
    edited = (bool) state.getProperty ("edited", false);
    keepSongKey = (bool) state.getProperty ("keepSongKey", true);
    activeAB = juce::jlimit (0, 1, (int) state.getProperty ("abSlot", 0));
    activeABAtomic.store (activeAB, std::memory_order_relaxed);
    setLoudnessMatch ((bool) state.getProperty ("loudnessMatch", false));

    for (int slot = 0; slot < 2; ++slot)
    {
        abValid[slot] = false;
        abStates[slot] = juce::ValueTree();
        const auto xmlString = state.getProperty (slot == 0 ? "abA" : "abB").toString();

        if (xmlString.isNotEmpty())
        {
            if (auto storedXml = juce::XmlDocument::parse (xmlString))
            {
                auto stored = juce::ValueTree::fromXml (*storedXml);

                if (stored.isValid())
                {
                    abStates[slot] = stored;
                    abValid[slot] = true;
                }
            }
        }
    }

    applyingState = false;
    updateHostLatency();
    notifyStateChanged();
}

juce::AudioProcessorParameter* VocalRackAudioProcessor::getBypassParameter() const
{
    return apvts.getParameter ("global.bypass");
}

void VocalRackAudioProcessor::parameterChanged (const juce::String& id, float)
{
    if (applyingState)
        return;

    if (id == "pitch.tonic" || id == "pitch.scale")
        syncMaskFromKey();

    edited = true;
    apvts.state.setProperty ("edited", true, nullptr);

    if (onStateChanged)
        onStateChanged();
}

void VocalRackAudioProcessor::syncMaskFromKey()
{
    if (updatingMask)
        return;

    const int scale = (int) std::lround (apvts.getRawParameterValue ("pitch.scale")->load());
    const int tonic = (int) std::lround (apvts.getRawParameterValue ("pitch.tonic")->load());

    if (scale == 3)
        return;

    const int desired = maskForScale (scale, tonic);
    const int current = (int) std::lround (apvts.getRawParameterValue ("pitch.mask")->load());

    if (desired == current)
        return;

    if (auto* parameter = apvts.getParameter ("pitch.mask"))
    {
        updatingMask = true;
        parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) desired));
        updatingMask = false;
    }
}

void VocalRackAudioProcessor::timerCallback()
{
    // Reserved for future asynchronous notifications.  All UI values are read
    // directly from lock-free atomics in the engine.
}

void VocalRackAudioProcessor::updateHostLatency()
{
    const int latency = engine.getLatencySamples();
    reportedLatency.store (latency, std::memory_order_relaxed);
    setLatencySamples (latency);
}

void VocalRackAudioProcessor::publishMeters()
{
}

void VocalRackAudioProcessor::notifyStateChanged()
{
    if (onStateChanged)
        onStateChanged();
}

void VocalRackAudioProcessor::requestQuality (QualityMode mode)
{
    applyQualityOnMessageThread (mode);
}

void VocalRackAudioProcessor::applyQualityOnMessageThread (QualityMode mode)
{
    if (mode == qualityAtomic.load() && engine.isPrepared())
        return;

    if (! engine.isPrepared())
    {
        qualityAtomic.store (mode);
        return;
    }

    suspendProcessing (true);
    const double deadline = juce::Time::getMillisecondCounterHiRes() + 500.0;

    while (processing.load() && juce::Time::getMillisecondCounterHiRes() < deadline)
        juce::Thread::sleep (1);

    currentInputChannels = juce::jlimit (1, 2, juce::jmax (1, getTotalNumInputChannels()));
    currentOutputChannels = juce::jlimit (1, 2, juce::jmax (1, getTotalNumOutputChannels()));

    qualityAtomic.store (mode);
    engine.prepare (currentSampleRate, currentBlockSize, currentInputChannels,
                    currentOutputChannels, mode);
    updateHostLatency();
    suspendProcessing (false);

    if (onNotice)
        onNotice ("Quality changed. The host latency was updated; stop transport if the host does not re-align live.");
}

void VocalRackAudioProcessor::requestPresetLoad (const juce::String& presetJsonOrId, bool keepKey)
{
    PresetEntry entry;
    juce::String error;

    const auto trimmed = presetJsonOrId.trim();

    if (trimmed.startsWith ("{"))
    {
        const auto parsed = juce::JSON::parse (trimmed);

        if (! presetManager.validatePreset (parsed, entry, error))
        {
            if (onNotice)
                onNotice ("Preset rejected: " + error);

            return;
        }

        entry.factory = false;
    }
    else
    {
        const auto* found = presetManager.findById (trimmed);

        if (found == nullptr)
        {
            if (onNotice)
                onNotice ("Preset not found.");

            return;
        }

        entry = *found;
    }

    const bool preserveKey = keepKey && entry.factory;
    auto state = presetManager.makeStateFromPreset (entry, apvts.copyState(), preserveKey, error);

    if (! state.isValid())
    {
        if (onNotice)
            onNotice ("Preset could not be applied: " + error);

        return;
    }

    applyStateInternal (state, false);
    currentPresetId = entry.id;
    currentPresetName = entry.name;
    currentPresetBank = entry.bank;
    currentPresetFactory = entry.factory;
    edited = false;

    abStates[0] = state;
    abValid[0] = true;
    abStates[1] = juce::ValueTree();
    abValid[1] = false;
    activeAB = 0;

    notifyStateChanged();

    if (onNotice)
    {
        double amount = 0.0;

        if (auto* params = entry.parameters.getDynamicObject())
            amount = (double) params->getProperty ("dn.amount");

        if (qualityAtomic.load() == QualityMode::track && amount > 0.0)
            onNotice ("This preset uses spectral cleanup, which is unavailable in Track. Switch Quality to Mix to hear it.");
    }
}

void VocalRackAudioProcessor::applyStateInternal (const juce::ValueTree& state, bool markEdited)
{
    if (! state.isValid())
        return;

    applyingState = true;
    engine.triggerStateFade();
    apvts.replaceState (state);
    applyingState = false;
    edited = markEdited;

    currentPresetId = state.getProperty ("presetId", currentPresetId).toString();
    currentPresetName = state.getProperty ("presetName", currentPresetName).toString();
    currentPresetBank = state.getProperty ("presetBank", currentPresetBank).toString();
    currentPresetFactory = (bool) state.getProperty ("presetFactory", currentPresetFactory);

    notifyStateChanged();
}

void VocalRackAudioProcessor::requestStateApply (const juce::ValueTree& state, bool markEdited)
{
    applyStateInternal (state, markEdited);
}

bool VocalRackAudioProcessor::saveCurrentAsUserPreset (const juce::String& name,
                                                       const juce::String& bank,
                                                       const juce::String& description,
                                                       const juce::String& useCase,
                                                       const juce::String& firstAdjustment,
                                                       juce::String& error)
{
    PresetEntry entry;
    entry.id = PresetManager::makeUserId();
    entry.name = name;
    entry.bank = bank;
    entry.description = description;
    entry.useCase = useCase;
    entry.firstAdjustment = firstAdjustment;
    entry.parameters = presetManager.parametersFromState (apvts.copyState());
    entry.factory = false;

    if (! presetManager.saveUserPreset (entry, false, error))
        return false;

    currentPresetId = entry.id;
    currentPresetName = name.trim().isEmpty() ? "User Preset" : name.trim();
    currentPresetBank = bank.trim().isEmpty() ? "User" : bank.trim();
    currentPresetFactory = false;
    edited = false;
    notifyStateChanged();
    return true;
}

bool VocalRackAudioProcessor::importPresetFile (const juce::File& file, juce::String& error)
{
    PresetEntry imported;

    if (! presetManager.importPreset (file, imported, error))
        return false;

    requestPresetLoad (imported.id, false);
    return true;
}

bool VocalRackAudioProcessor::exportPresetFile (const juce::String& id, const juce::File& file,
                                                juce::String& error)
{
    return presetManager.exportPreset (id, file, error);
}

bool VocalRackAudioProcessor::deleteUserPreset (const juce::String& id, juce::String& error)
{
    if (! presetManager.deleteUserPreset (id, error))
        return false;

    if (currentPresetId == id)
    {
        currentPresetId = "custom";
        currentPresetName = "Session";
        currentPresetBank = "Session";
        currentPresetFactory = false;
        edited = true;
    }

    notifyStateChanged();
    return true;
}

bool VocalRackAudioProcessor::renameUserPreset (const juce::String& id,
                                                const juce::String& newName,
                                                juce::String& error)
{
    if (! presetManager.renameUserPreset (id, newName, error))
        return false;

    if (currentPresetId == id)
        currentPresetName = newName.trim();

    notifyStateChanged();
    return true;
}

bool VocalRackAudioProcessor::duplicateUserPreset (const juce::String& id,
                                                   const juce::String& newName,
                                                   juce::String& error)
{
    if (! presetManager.duplicateUserPreset (id, newName, error))
        return false;

    notifyStateChanged();
    return true;
}

void VocalRackAudioProcessor::switchAB (int slot)
{
    if (slot < 0 || slot > 1 || slot == activeAB)
        return;

    const float referenceRms = abMeasuredRmsDb[activeAB].load (std::memory_order_relaxed);
    const float targetRms = abMeasuredRmsDb[slot].load (std::memory_order_relaxed);

    abStates[activeAB] = apvts.copyState();
    abStates[activeAB].setProperty ("edited", edited, nullptr);
    abValid[activeAB] = true;

    if (abValid[slot])
    {
        applyingState = true;
        engine.triggerStateFade();
        apvts.replaceState (abStates[slot]);
        applyingState = false;
        edited = (bool) abStates[slot].getProperty ("edited", edited);
    }
    else
    {
        abStates[slot] = abStates[activeAB];
        abValid[slot] = true;
    }

    activeAB = slot;
    activeABAtomic.store (slot, std::memory_order_relaxed);

    if (isLoudnessMatch() && referenceRms > -59.0f && targetRms > -59.0f)
        comparisonTargetGain.store (juce::jlimit (0.25f, 4.0f,
                                                 dbToGain (referenceRms - targetRms)),
                                    std::memory_order_relaxed);
    else
        comparisonTargetGain.store (1.0f, std::memory_order_relaxed);

    notifyStateChanged();
}

void VocalRackAudioProcessor::setLoudnessMatch (bool shouldMatch) noexcept
{
    loudnessMatch.store (shouldMatch, std::memory_order_relaxed);

    if (! shouldMatch)
        comparisonTargetGain.store (1.0f, std::memory_order_relaxed);
}

void VocalRackAudioProcessor::copyAB (int from, int to)
{
    if (from < 0 || from > 1 || to < 0 || to > 1 || from == to)
        return;

    abStates[to] = from == activeAB ? apvts.copyState() : abStates[from];
    abValid[to] = abStates[to].isValid();
    notifyStateChanged();
}

} // namespace vr

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new vr::VocalRackAudioProcessor();
}
