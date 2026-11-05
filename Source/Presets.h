#pragma once

#include "Parameters.h"

#include <juce_data_structures/juce_data_structures.h>

#include <vector>

namespace vr
{

struct PresetEntry
{
    juce::String id;
    juce::String name;
    juce::String bank;
    juce::String description;
    juce::String useCase;
    juce::String firstAdjustment;
    juce::var parameters;
    bool factory = false;
    juce::File file;
};

/**
    Factory and user preset library.

    The on-disk format is a versioned JSON object:
      { "format": "ademusic-vocal-rack-preset", "version": 1,
        "id", "name", "bank", "description", "useCase", "firstAdjustment",
        "parameters": { "<stable parameter id>": <denormalised value>, ... } }
    Every parameter is present.  Imports are fully validated before any state
    change is offered to the processor.
*/
class PresetManager
{
public:
    static constexpr const char* formatTag = "ademusic-vocal-rack-preset";
    static constexpr int formatVersion = 1;
    static constexpr int maximumImportBytes = 262144;
    static constexpr int numParameters = 82;

    PresetManager();

    void initialise();

    const std::vector<PresetEntry>& getEntries() const noexcept { return entries; }
    const PresetEntry* findById (const juce::String& id) const;
    int indexOfId (const juce::String& id) const;

    std::vector<juce::String> getBanks() const;
    std::vector<int> filteredIndices (const juce::String& bank,
                                      const juce::String& search) const;

    juce::File getUserDirectory() const { return userDirectory; }
    const juce::StringArray& getScanErrors() const noexcept { return scanErrors; }

    juce::var parametersFromState (const juce::ValueTree& state) const;

    juce::ValueTree makeStateFromPreset (const PresetEntry& entry,
                                         const juce::ValueTree& baseState,
                                         bool keepSongKey,
                                         juce::String& error) const;

    bool validatePreset (const juce::var& root, PresetEntry& out, juce::String& error) const;
    bool validatePresetFile (const juce::File& file, PresetEntry& out, juce::String& error) const;

    bool saveUserPreset (const PresetEntry& entry, bool overwrite, juce::String& error);
    bool deleteUserPreset (const juce::String& id, juce::String& error);
    bool renameUserPreset (const juce::String& id, const juce::String& newName, juce::String& error);
    bool duplicateUserPreset (const juce::String& id, const juce::String& newName, juce::String& error);
    bool exportPreset (const juce::String& id, const juce::File& destination, juce::String& error) const;
    bool importPreset (const juce::File& source, PresetEntry& imported, juce::String& error);

    juce::var toJson (const PresetEntry& entry) const;

    static juce::String makeUserId();
    static juce::String slugify (const juce::String& text);

private:
    void scanUserPresets();
    void loadFactoryPresets();
    static bool readNumber (const juce::var& value, double& out);

    std::vector<PresetEntry> entries;
    juce::File userDirectory;
    juce::StringArray scanErrors;
};

} // namespace vr
