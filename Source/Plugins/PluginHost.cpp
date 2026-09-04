#include "Plugins/PluginHost.h"
#include "SessionSettings.h"

#if JUCE_WINDOWS
 #include <objbase.h>
#endif

namespace
{
    bool pathLooksLikeDocumentsVst3 (const juce::String& p)
    {
        const auto n = p.replaceCharacter ('\\', '/');
        return n.containsIgnoreCase ("/Documents/VST3")
            || n.containsIgnoreCase ("/Documents\\VST3");
    }

    bool pathLooksLikeProgramFilesVst3 (const juce::String& p)
    {
        const auto n = p.replaceCharacter ('\\', '/');
        return n.containsIgnoreCase ("/Program Files")
            || n.containsIgnoreCase ("/Common Files/VST3");
    }

    juce::String pluginKey (const juce::PluginDescription& t)
    {
        // Prefer uniqueId so Documents vs Program Files copies collapse.
        // Do NOT use createIdentifierString() — it embeds the file path.
        if (t.uniqueId != 0)
            return "uid:" + juce::String (t.uniqueId);
        if (t.deprecatedUid != 0)
            return "duid:" + juce::String (t.deprecatedUid);
        return t.pluginFormatName + "|" + t.manufacturerName + "|" + t.name;
    }

    int pathPreferenceScore (const juce::String& path)
    {
        if (pathLooksLikeDocumentsVst3 (path))
            return 3;
        if (pathLooksLikeProgramFilesVst3 (path))
            return 1;
        return 2;
    }
}

PluginHost::PluginHost()
{
    formatManager.addFormat (new juce::VST3PluginFormat());
    loadPersistedList();
    maybeForceCleanRescan();
    sanitizeKnownList();
}

PluginHost::~PluginHost()
{
    savePersistedList();
}

juce::File PluginHost::settingsFile() const
{
    return SessionSettings::appDir().getChildFile ("plugin-list.xml");
}

juce::File PluginHost::deadMansPedalFile() const
{
    return settingsFile().getSiblingFile ("vst3-crashed.txt");
}


juce::File PluginHost::starterFlagFile() const
{
    return settingsFile().getSiblingFile ("starter-vst.flag");
}

bool PluginHost::isStarterSeeded() const
{
    return starterFlagFile().existsAsFile();
}

void PluginHost::markStarterSeeded()
{
    starterFlagFile().replaceWithText ("1\n");
}

void PluginHost::clearStarterSeeded()
{
    starterFlagFile().deleteFile();
}

void PluginHost::maybeForceCleanRescan()
{
    // One-shot: clear dead-man + starter + slot state so amp VSTs re-seed into AmpReplace
    // (old seed stacked NAM/VoLum in Pre while SAWVI still ran).
    auto marker = settingsFile().getSiblingFile ("vst-amp-slot-fix-20260904.flag");
    if (marker.existsAsFile())
        return;
    deadMansPedalFile().deleteFile();
    clearStarterSeeded();
    settingsFile().getSiblingFile ("plugin-slots.xml").deleteFile();
    marker.replaceWithText ("1
");
}

bool PluginHost::shouldAutoScan() const
{
    return knownList.getNumTypes() == 0 || ! isStarterSeeded();
}

bool PluginHost::findTypeMatching (const juce::StringArray& needles, juce::PluginDescription& out) const
{
    for (const auto& t : knownList.getTypes())
    {
        const auto hay = t.name + " " + t.descriptiveName + " " + t.fileOrIdentifier;
        for (const auto& n : needles)
        {
            if (n.isNotEmpty() && hay.containsIgnoreCase (n))
            {
                out = t;
                return true;
            }
        }
    }
    return false;
}

juce::String PluginHost::normalizeVst3Identifier (const juce::String& fileOrId)
{
    if (fileOrId.isEmpty())
        return fileOrId;

    juce::File cur (fileOrId);
    // Walk up looking for a .vst3 directory that contains Contents/.
    for (int i = 0; i < 6 && cur != juce::File(); ++i)
    {
        if (cur.hasFileExtension ("vst3")
            && cur.isDirectory()
            && cur.getChildFile ("Contents").isDirectory())
            return cur.getFullPathName();
        const auto parent = cur.getParentDirectory();
        if (parent == cur)
            break;
        cur = parent;
    }

    // Already a bare bundle path that exists as a directory.
    juce::File f (fileOrId);
    if (f.hasFileExtension ("vst3") && f.isDirectory())
        return f.getFullPathName();

    return fileOrId;
}

juce::String PluginHost::resolveVst3BundlePath (const juce::PluginDescription& desc)
{
    auto path = normalizeVst3Identifier (desc.fileOrIdentifier);
    juce::File f (path);
    if (f.exists())
        return f.getFullPathName();

    // Derive a bundle name from the leaf or plugin name.
    juce::String leaf = f.getFileName();
    if (! leaf.endsWithIgnoreCase (".vst3"))
        leaf = desc.name + ".vst3";
    // If nested module was Foo.vst3 inside Contents, parent walk may have failed —
    // try desc.name.
    const juce::StringArray candidates { leaf, desc.name + ".vst3",
                                         juce::File (desc.fileOrIdentifier).getFileName() };

#if JUCE_WINDOWS
    const auto docs = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                          .getChildFile ("VST3");
    const juce::File commons[] = {
        juce::File ("C:/Program Files/Common Files/VST3"),
        juce::File ("C:/Program Files (x86)/Common Files/VST3"),
        juce::File::getSpecialLocation (juce::File::commonApplicationDataDirectory)
            .getChildFile ("VST3")
    };
    for (const auto& name : candidates)
    {
        if (name.isEmpty())
            continue;
        auto tryDocs = docs.getChildFile (name);
        if (tryDocs.exists())
            return normalizeVst3Identifier (tryDocs.getFullPathName());
        for (const auto& root : commons)
        {
            auto t = root.getChildFile (name);
            if (t.exists())
                return normalizeVst3Identifier (t.getFullPathName());
        }
    }
#else
    juce::ignoreUnused (candidates);
#endif
    return path;
}

juce::String PluginHost::pathFolderHint (const juce::String& fileOrId)
{
    if (pathLooksLikeDocumentsVst3 (fileOrId))
        return "Documents";
    if (pathLooksLikeProgramFilesVst3 (fileOrId))
        return "Program Files";
    const auto n = fileOrId.replaceCharacter ('\\', '/');
    if (n.containsIgnoreCase ("/VST3/"))
        return "VST3";
    return {};
}

void PluginHost::normalizeKnownPaths()
{
    auto types = knownList.getTypes();
    bool changed = false;
    juce::Array<juce::PluginDescription> rewritten;
    rewritten.ensureStorageAllocated (types.size());
    for (auto t : types)
    {
        const auto norm = normalizeVst3Identifier (t.fileOrIdentifier);
        if (norm != t.fileOrIdentifier && juce::File (norm).exists())
        {
            t.fileOrIdentifier = norm;
            changed = true;
        }
        rewritten.add (t);
    }
    if (! changed)
        return;
    knownList.clear();
    for (const auto& t : rewritten)
        knownList.addType (t);
}

void PluginHost::pruneDeadTypes()
{
    auto types = knownList.getTypes();
    juce::Array<juce::PluginDescription> keep;
    keep.ensureStorageAllocated (types.size());
    bool changed = false;
    for (auto t : types)
    {
        const auto resolved = resolveVst3BundlePath (t);
        juce::File f (resolved);
        if (! f.exists())
        {
            changed = true;
            continue;
        }
        if (resolved != t.fileOrIdentifier)
        {
            t.fileOrIdentifier = resolved;
            changed = true;
        }
        keep.add (t);
    }
    if (! changed && keep.size() == types.size())
        return;
    knownList.clear();
    for (const auto& t : keep)
        knownList.addType (t);
}

void PluginHost::dedupeTypes()
{
    auto types = knownList.getTypes();
    if (types.size() <= 1)
        return;

    juce::StringArray keys;
    juce::Array<juce::PluginDescription> best;
    best.ensureStorageAllocated (types.size());

    for (const auto& t : types)
    {
        const auto key = pluginKey (t);
        const int idx = keys.indexOf (key);
        if (idx < 0)
        {
            keys.add (key);
            best.add (t);
            continue;
        }
        const auto& cur = best.getReference (idx);
        const int sNew = pathPreferenceScore (t.fileOrIdentifier);
        const int sOld = pathPreferenceScore (cur.fileOrIdentifier);
        bool take = sNew > sOld;
        if (! take && sNew == sOld
            && t.fileOrIdentifier.length() < cur.fileOrIdentifier.length())
            take = true;
        if (take)
            best.set (idx, t);
    }

    if (best.size() == types.size())
        return;

    knownList.clear();
    for (const auto& t : best)
        knownList.addType (t);
}

void PluginHost::sanitizeKnownList()
{
    normalizeKnownPaths();
    pruneDeadTypes();
    dedupeTypes();
    savePersistedList();
}

juce::FileSearchPath PluginHost::defaultVST3Paths() const
{
    juce::FileSearchPath path;
#if JUCE_WINDOWS
    // Prefer user Documents first so scanners / dedupe lean that way.
    const auto docs = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
    auto userVst = docs.getChildFile ("VST3");
    userVst.createDirectory();
    path.add (userVst);
    path.add (juce::File::getSpecialLocation (juce::File::commonApplicationDataDirectory)
                  .getChildFile ("VST3"));
    path.add (juce::File ("C:/Program Files/Common Files/VST3"));
    path.add (juce::File ("C:/Program Files (x86)/Common Files/VST3"));
#elif JUCE_MAC
    path.add (juce::File ("/Library/Audio/Plug-Ins/VST3"));
    path.add (juce::File::getSpecialLocation (juce::File::userHomeDirectory)
                  .getChildFile ("Library/Audio/Plug-Ins/VST3"));
#else
    path.add (juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile (".vst3"));
    path.add (juce::File ("/usr/lib/vst3"));
    path.add (juce::File ("/usr/local/lib/vst3"));
    path.add (juce::File ("/usr/lib/x86_64-linux-gnu/vst3"));
#endif
    return path;
}

void PluginHost::loadPersistedList()
{
    auto f = settingsFile();
    if (! f.existsAsFile())
        return;
    if (auto xml = juce::parseXML (f))
    {
        if (auto* list = xml->getChildByName ("KNOWNPLUGINS"))
            knownList.recreateFromXml (*list);
        else
            knownList.recreateFromXml (*xml);
    }
}

void PluginHost::savePersistedList()
{
    auto xml = knownList.createXml();
    if (xml == nullptr)
        xml = std::make_unique<juce::XmlElement> ("KNOWNPLUGINS");
    xml->writeTo (settingsFile());
}

void PluginHost::scanDefaultVST3Paths (std::function<void()> onFinished)
{
    if (scanning.exchange (1) != 0)
        return;

    lastScanStatus = "Scanning VST3…";
    const auto paths = defaultVST3Paths();
    // Skip dead-man for this scan pass (Documents plugins were blacklisted).
    const auto pedal = juce::File(); // empty = no dead-man's pedal

    juce::Thread::launch ([this, onFinished, paths, pedal]
    {
#if JUCE_WINDOWS
        const HRESULT coHr = CoInitializeEx (nullptr, COINIT_MULTITHREADED);
#endif
        for (int i = 0; i < formatManager.getNumFormats(); ++i)
        {
            auto* fmt = formatManager.getFormat (i);
            if (fmt == nullptr)
                continue;
            juce::PluginDirectoryScanner scanner (knownList, *fmt, paths, true, pedal, false);
            juce::String name;
            while (scanner.scanNextFile (true, name))
            {}
        }

        // Prefer bundle paths on anything the scanner stored nested.
        normalizeKnownPaths();
        pruneDeadTypes();
        dedupeTypes();
        savePersistedList();
        lastScanStatus = "VST3 scan complete (" + juce::String (knownList.getNumTypes()) + " plugins)";
        scanning.store (0);
#if JUCE_WINDOWS
        if (SUCCEEDED (coHr))
            CoUninitialize();
#endif
        if (onFinished)
            juce::MessageManager::callAsync (onFinished);
    });
}

void PluginHost::importVst3Files (const juce::Array<juce::File>& files, std::function<void(int added)> onDone)
{
    if (scanning.exchange (1) != 0)
    {
        if (onDone)
            juce::MessageManager::callAsync ([onDone] { onDone (0); });
        return;
    }

    lastScanStatus = "Loading VST3…";
    const auto picked = files;

    juce::Thread::launch ([this, onDone, picked]
    {
#if JUCE_WINDOWS
        const HRESULT coHr = CoInitializeEx (nullptr, COINIT_MULTITHREADED);
#endif
        int added = 0;
        juce::Array<juce::File> targets;

        for (const auto& f : picked)
        {
            if (! f.exists())
                continue;
            if (f.isDirectory() && ! f.hasFileExtension ("vst3"))
            {
                juce::Array<juce::File> kids;
                f.findChildFiles (kids, juce::File::findFilesAndDirectories, false, "*.vst3");
                targets.addArray (kids);
            }
            else
            {
                targets.add (f);
            }
        }

        for (const auto& t : targets)
        {
            // Always scan the bundle folder, not a nested module path.
            const auto path = normalizeVst3Identifier (t.getFullPathName());
            for (int i = 0; i < formatManager.getNumFormats(); ++i)
            {
                auto* fmt = formatManager.getFormat (i);
                if (fmt == nullptr)
                    continue;
                juce::OwnedArray<juce::PluginDescription> types;
                fmt->findAllTypesForFile (types, path);
                for (auto* d : types)
                {
                    if (d == nullptr)
                        continue;
                    d->fileOrIdentifier = normalizeVst3Identifier (d->fileOrIdentifier);
                    if (knownList.addType (*d))
                        ++added;
                }
            }
        }

        normalizeKnownPaths();
        pruneDeadTypes();
        dedupeTypes();
        savePersistedList();
        lastScanStatus = added > 0
            ? ("Loaded " + juce::String (added) + " VST3")
            : "No VST3 found";
        scanning.store (0);
#if JUCE_WINDOWS
        if (SUCCEEDED (coHr))
            CoUninitialize();
#endif
        if (onDone)
            juce::MessageManager::callAsync ([onDone, added] { onDone (added); });
    });
}
