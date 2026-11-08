#include "Presets.h"

#include "presets/FactoryPresetsData.h"

#include <algorithm>
#include <cmath>

namespace vr
{

namespace
{
juce::DynamicObject* asObject (const juce::var& value)
{
    return value.getDynamicObject();
}

bool isNumber (const juce::var& value)
{
    return value.isDouble() || value.isInt() || value.isInt64() || value.isBool();
}

juce::String normaliseExtension (juce::String name)
{
    name = name.trim();

    if (name.isEmpty())
        name = "Untitled";

    return name;
}
} // namespace

PresetManager::PresetManager()
{
    const auto overrideDirectory = juce::SystemStats::getEnvironmentVariable ("VR_USER_PRESET_DIR", {});

    userDirectory = overrideDirectory.isNotEmpty()
                        ? juce::File (overrideDirectory)
                        : juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                              .getChildFile ("ademusic")
                              .getChildFile ("Vocal Rack")
                              .getChildFile ("Presets");
}

juce::String PresetManager::makeUserId()
{
    return "user." + juce::Uuid().toDashedString();
}

juce::String PresetManager::slugify (const juce::String& text)
{
    juce::String result;
    bool lastWasDash = false;

    for (auto character : text.toLowerCase())
    {
        if (juce::CharacterFunctions::isLetterOrDigit (character))
        {
            result << character;
            lastWasDash = false;
        }
        else if (! lastWasDash)
        {
            result << '-';
            lastWasDash = true;
        }
    }

    result = result.trimCharactersAtStart ("-").trimCharactersAtEnd ("-");

    if (result.isEmpty())
        result = "preset";

    return result.substring (0, 48);
}

void PresetManager::initialise()
{
    entries.clear();
    scanErrors.clear();

    loadFactoryPresets();
    scanUserPresets();
}

void PresetManager::loadFactoryPresets()
{
    const auto parsed = juce::JSON::parse (juce::String (kFactoryPresetJson));

    if (auto* array = parsed.getArray())
    {
        for (const auto& item : *array)
        {
            PresetEntry entry;
            juce::String error;

            if (validatePreset (item, entry, error))
            {
                entry.factory = true;
                entry.file = juce::File();
                entries.push_back (entry);
            }
            else
            {
                scanErrors.add ("Embedded factory preset rejected: " + error);
            }
        }
    }
    else
    {
        scanErrors.add ("Embedded factory preset bank is not a JSON array.");
    }
}

void PresetManager::scanUserPresets()
{
    userDirectory.createDirectory();

    if (! userDirectory.isDirectory())
        return;

    juce::Array<juce::File> files;
    userDirectory.findChildFiles (files, juce::File::findFiles, false, "*.vocalrack.json");
    files.sort();

    std::vector<PresetEntry> userEntries;

    for (const auto& file : files)
    {
        if (file.getSize() > (juce::int64) maximumImportBytes)
        {
            scanErrors.add (file.getFileName() + ": larger than 256 KiB, skipped.");
            continue;
        }

        PresetEntry entry;
        juce::String error;

        if (validatePresetFile (file, entry, error))
        {
            entry.factory = false;
            entry.file = file;

            const bool duplicate =
                std::any_of (entries.begin(), entries.end(),
                             [&] (const PresetEntry& existing) { return existing.id == entry.id; })
                || std::any_of (userEntries.begin(), userEntries.end(),
                                [&] (const PresetEntry& existing) { return existing.id == entry.id; });

            if (duplicate)
                scanErrors.add (file.getFileName() + ": duplicate preset id, skipped.");
            else
                userEntries.push_back (entry);
        }
        else
        {
            scanErrors.add (file.getFileName() + ": " + error);
        }
    }

    std::sort (userEntries.begin(), userEntries.end(),
               [] (const PresetEntry& a, const PresetEntry& b)
               {
                   return a.name.compareIgnoreCase (b.name) < 0;
               });

    entries.insert (entries.end(), userEntries.begin(), userEntries.end());
}

const PresetEntry* PresetManager::findById (const juce::String& id) const
{
    for (const auto& entry : entries)
        if (entry.id == id)
            return &entry;

    return nullptr;
}

int PresetManager::indexOfId (const juce::String& id) const
{
    for (size_t i = 0; i < entries.size(); ++i)
        if (entries[i].id == id)
            return (int) i;

    return -1;
}

std::vector<juce::String> PresetManager::getBanks() const
{
    std::vector<juce::String> banks;

    for (const auto& entry : entries)
    {
        if (std::find (banks.begin(), banks.end(), entry.bank) == banks.end())
            banks.push_back (entry.bank);
    }

    return banks;
}

std::vector<int> PresetManager::filteredIndices (const juce::String& bank,
                                                 const juce::String& search) const
{
    std::vector<int> result;
    const auto query = search.trim().toLowerCase();

    for (size_t i = 0; i < entries.size(); ++i)
    {
        const auto& entry = entries[i];

        if (bank.isNotEmpty() && bank != "All banks" && entry.bank != bank)
            continue;

        if (query.isNotEmpty())
        {
            const auto haystack = (entry.name + " " + entry.bank + " " + entry.description
                                   + " " + entry.useCase).toLowerCase();

            if (! haystack.contains (query))
                continue;
        }

        result.push_back ((int) i);
    }

    return result;
}

juce::var PresetManager::parametersFromState (const juce::ValueTree& state) const
{
    juce::DynamicObject::Ptr object = new juce::DynamicObject();

    for (const auto& spec : getParamSpecs())
    {
        const auto child = state.getChildWithProperty ("id", juce::String (spec.id));
        const double value = child.isValid()
                                 ? (double) child.getProperty ("value", spec.defaultValue)
                                 : (double) spec.defaultValue;
        object->setProperty (juce::Identifier (spec.id), value);
    }

    return juce::var (object.get());
}

juce::ValueTree PresetManager::makeStateFromPreset (const PresetEntry& entry,
                                                    const juce::ValueTree& baseState,
                                                    bool keepSongKey,
                                                    juce::String& error) const
{
    if (! baseState.isValid())
    {
        error = "No base state available.";
        return {};
    }

    auto* params = asObject (entry.parameters);

    if (params == nullptr)
    {
        error = "Preset has no parameter object.";
        return {};
    }

    juce::ValueTree state = baseState.createCopy();

    for (const auto& spec : getParamSpecs())
    {
        const juce::String id (spec.id);
        const bool preserveKey = keepSongKey
                                 && (id == "pitch.tonic" || id == "pitch.scale" || id == "pitch.mask");

        double value = spec.defaultValue;

        if (preserveKey)
        {
            const auto existing = baseState.getChildWithProperty ("id", id);
            value = existing.isValid()
                        ? (double) existing.getProperty ("value", spec.defaultValue)
                        : (double) spec.defaultValue;
        }
        else
        {
            const auto parameterValue = params->getProperty (juce::Identifier (spec.id));

            if (! readNumber (parameterValue, value))
            {
                error = "Missing or invalid parameter: " + id;
                return {};
            }
        }

        auto child = state.getChildWithProperty ("id", id);

        if (! child.isValid())
        {
            child = juce::ValueTree ("PARAM");
            child.setProperty ("id", id, nullptr);
            state.appendChild (child, nullptr);
        }

        child.setProperty ("value", value, nullptr);
    }

    state.setProperty ("stateVersion", 1, nullptr);
    state.setProperty ("presetId", entry.id, nullptr);
    state.setProperty ("presetName", entry.name, nullptr);
    state.setProperty ("presetBank", entry.bank, nullptr);
    state.setProperty ("presetDescription", entry.description, nullptr);
    state.setProperty ("presetFactory", entry.factory, nullptr);
    state.setProperty ("presetUseCase", entry.useCase, nullptr);
    state.setProperty ("presetFirstAdjustment", entry.firstAdjustment, nullptr);
    state.setProperty ("edited", false, nullptr);
    return state;
}

bool PresetManager::readNumber (const juce::var& value, double& out)
{
    if (! isNumber (value))
        return false;

    out = (double) value;
    return std::isfinite (out);
}

bool PresetManager::validatePreset (const juce::var& root, PresetEntry& out,
                                    juce::String& error) const
{
    auto* object = asObject (root);

    if (object == nullptr)
    {
        error = "Preset root is not a JSON object.";
        return false;
    }

    const auto format = object->getProperty ("format").toString();

    if (format != formatTag)
    {
        error = "Wrong format tag (expected " + juce::String (formatTag) + ").";
        return false;
    }

    const int version = (int) object->getProperty ("version");

    if (version != formatVersion)
    {
        error = "Unsupported version " + juce::String (version) + ".";
        return false;
    }

    out.id = object->getProperty ("id").toString().trim();
    out.name = object->getProperty ("name").toString().trim();
    out.bank = object->getProperty ("bank").toString().trim();
    out.description = object->getProperty ("description").toString();
    out.useCase = object->getProperty ("useCase").toString();
    out.firstAdjustment = object->getProperty ("firstAdjustment").toString();

    if (out.id.isEmpty() || out.name.isEmpty() || out.bank.isEmpty())
    {
        error = "Preset id, name and bank are required.";
        return false;
    }

    auto* params = object->getProperty ("parameters").getDynamicObject();

    if (params == nullptr)
    {
        error = "Preset has no parameters object.";
        return false;
    }

    const auto& specs = getParamSpecs();
    const auto& properties = params->getProperties();

    for (const auto& property : properties)
    {
        if (findParamSpec (property.name.toString()) == nullptr)
        {
            error = "Unknown parameter: " + property.name.toString();
            return false;
        }
    }

    juce::DynamicObject::Ptr clean = new juce::DynamicObject();

    for (const auto& spec : specs)
    {
        const juce::String id (spec.id);
        const auto value = params->getProperty (juce::Identifier (spec.id));
        double number = 0.0;

        if (! readNumber (value, number))
        {
            // Parameters introduced after the format's first public version
            // are optional in old user presets; use their documented default
            // instead of rejecting the preset.
            const bool introducedIn2 = id == "pitch.formant" || id == "pitch.formantShift"
                                      || id.startsWith ("deq");

            if (introducedIn2)
                number = (double) spec.defaultValue;
            else
            {
                error = "Missing or non-numeric parameter: " + id;
                return false;
            }
        }

        if (spec.type == ParamType::boolean)
        {
            if (number != 0.0 && number != 1.0)
            {
                error = "Boolean parameter out of range: " + id;
                return false;
            }
        }
        else if (spec.type == ParamType::choice)
        {
            if (std::abs (number - std::round (number)) > 1.0e-6
                || number < spec.minValue || number > spec.maxValue)
            {
                error = "Choice parameter out of range: " + id;
                return false;
            }
        }
        else if (spec.type == ParamType::intValue)
        {
            if (std::abs (number - std::round (number)) > 1.0e-6
                || number < spec.minValue || number > spec.maxValue)
            {
                error = "Integer parameter out of range: " + id;
                return false;
            }
        }
        else
        {
            if (number < (double) spec.minValue - 1.0e-4
                || number > (double) spec.maxValue + 1.0e-4)
            {
                error = "Parameter out of range: " + id;
                return false;
            }
        }

        clean->setProperty (juce::Identifier (spec.id), number);
    }

    out.parameters = juce::var (clean.get());
    return true;
}

bool PresetManager::validatePresetFile (const juce::File& file, PresetEntry& out,
                                        juce::String& error) const
{
    if (! file.existsAsFile())
    {
        error = "File does not exist.";
        return false;
    }

    if (file.getSize() <= 0 || file.getSize() > (juce::int64) maximumImportBytes)
    {
        error = "File size is outside the 1..262144 byte limit.";
        return false;
    }

    const auto parsed = juce::JSON::parse (file.loadFileAsString());

    if (parsed.isVoid())
    {
        error = "File is not valid JSON.";
        return false;
    }

    return validatePreset (parsed, out, error);
}

juce::var PresetManager::toJson (const PresetEntry& entry) const
{
    juce::DynamicObject::Ptr object = new juce::DynamicObject();
    object->setProperty ("format", formatTag);
    object->setProperty ("version", formatVersion);
    object->setProperty ("id", entry.id);
    object->setProperty ("name", entry.name);
    object->setProperty ("bank", entry.bank);
    object->setProperty ("description", entry.description);
    object->setProperty ("useCase", entry.useCase);
    object->setProperty ("firstAdjustment", entry.firstAdjustment);
    object->setProperty ("parameters", entry.parameters);
    return juce::var (object.get());
}

bool PresetManager::saveUserPreset (const PresetEntry& input, bool overwrite, juce::String& error)
{
    PresetEntry entry = input;
    entry.factory = false;
    entry.name = normaliseExtension (entry.name);
    entry.bank = entry.bank.trim().isEmpty() ? "User" : entry.bank.trim();

    if (entry.id.isEmpty() || entry.id.startsWith ("factory."))
        entry.id = makeUserId();

    PresetEntry validated;
    const auto json = toJson (entry);

    if (! validatePreset (json, validated, error))
        return false;

    validated.factory = false;
    validated.file = juce::File();

    userDirectory.createDirectory();

    if (! userDirectory.isDirectory())
    {
        error = "Cannot create the user preset directory.";
        return false;
    }

    const auto idSuffix = entry.id.fromLastOccurrenceOf (".", false, false);
    const auto fileName = slugify (entry.name) + "-" + idSuffix + ".vocalrack.json";
    const auto target = userDirectory.getChildFile (fileName);

    if (target.existsAsFile() && ! overwrite)
    {
        error = "A user preset file with this name already exists.";
        return false;
    }

    const auto temp = userDirectory.getChildFile (fileName + ".tmp-" + juce::Uuid().toDashedString());

    if (! temp.replaceWithText (juce::JSON::toString (json, true)))
    {
        error = "Could not write the temporary preset file.";
        return false;
    }

    if (! temp.replaceFileIn (target))
    {
        temp.deleteFile();
        error = "Could not atomically replace the preset file.";
        return false;
    }

    // Remove an older file if this was a rename/overwrite under a new name.
    const auto* existing = findById (entry.id);

    if (existing != nullptr && existing->file != juce::File() && existing->file != target)
        existing->file.deleteFile();

    initialise();
    return true;
}

bool PresetManager::deleteUserPreset (const juce::String& id, juce::String& error)
{
    const auto* entry = findById (id);

    if (entry == nullptr)
    {
        error = "Preset not found.";
        return false;
    }

    if (entry->factory)
    {
        error = "Factory presets are immutable.";
        return false;
    }

    if (entry->file.existsAsFile() && ! entry->file.deleteFile())
    {
        error = "Could not delete the preset file.";
        return false;
    }

    initialise();
    return true;
}

bool PresetManager::renameUserPreset (const juce::String& id, const juce::String& newName,
                                      juce::String& error)
{
    const auto* entry = findById (id);

    if (entry == nullptr)
    {
        error = "Preset not found.";
        return false;
    }

    if (entry->factory)
    {
        error = "Factory presets are immutable.";
        return false;
    }

    PresetEntry copy = *entry;
    const auto oldFile = entry->file;
    copy.name = normaliseExtension (newName);

    if (! saveUserPreset (copy, true, error))
        return false;

    const auto* updated = findById (id);

    if (updated != nullptr && oldFile != juce::File() && oldFile != updated->file)
        oldFile.deleteFile();

    return true;
}

bool PresetManager::duplicateUserPreset (const juce::String& id, const juce::String& newName,
                                         juce::String& error)
{
    const auto* entry = findById (id);

    if (entry == nullptr)
    {
        error = "Preset not found.";
        return false;
    }

    PresetEntry copy = *entry;
    copy.factory = false;
    copy.id = makeUserId();
    copy.name = normaliseExtension (newName.isNotEmpty() ? newName : entry->name + " Copy");
    copy.file = juce::File();

    return saveUserPreset (copy, false, error);
}

bool PresetManager::exportPreset (const juce::String& id, const juce::File& destination,
                                  juce::String& error) const
{
    const auto* entry = findById (id);

    if (entry == nullptr)
    {
        error = "Preset not found.";
        return false;
    }

    if (! destination.replaceWithText (juce::JSON::toString (toJson (*entry), true)))
    {
        error = "Could not write the export file.";
        return false;
    }

    return true;
}

bool PresetManager::importPreset (const juce::File& source, PresetEntry& imported,
                                  juce::String& error)
{
    PresetEntry entry;

    if (! validatePresetFile (source, entry, error))
        return false;

    entry.factory = false;
    entry.id = makeUserId();
    entry.file = juce::File();

    if (! saveUserPreset (entry, false, error))
        return false;

    const auto* saved = findById (entry.id);

    if (saved == nullptr)
    {
        error = "Imported preset could not be added to the user library.";
        return false;
    }

    imported = *saved;
    return true;
}

} // namespace vr
