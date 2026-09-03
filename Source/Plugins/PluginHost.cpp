#include "Plugins/PluginHost.h"
#include "SessionSettings.h"

PluginHost::PluginHost()
{
    formatManager.addFormat (new juce::VST3PluginFormat());
    loadPersistedList();
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

juce::FileSearchPath PluginHost::defaultVST3Paths() const
{
    juce::FileSearchPath path;
#if JUCE_WINDOWS
    path.add (juce::File::getSpecialLocation (juce::File::commonApplicationDataDirectory)
                  .getChildFile ("VST3"));
    path.add (juce::File ("C:/Program Files/Common Files/VST3"));
    path.add (juce::File ("C:/Program Files (x86)/Common Files/VST3"));
    const auto docs = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
    auto userVst = docs.getChildFile ("VST3");
    userVst.createDirectory();
    path.add (userVst);
    if (docs.isDirectory())
        path.add (docs);
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
    const auto pedal = deadMansPedalFile();

    juce::Thread::launch ([this, onFinished, paths, pedal]
    {
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
        savePersistedList();
        lastScanStatus = "VST3 scan complete (" + juce::String (knownList.getNumTypes()) + " plugins)";
        scanning.store (0);
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
            const auto path = t.getFullPathName();
            for (int i = 0; i < formatManager.getNumFormats(); ++i)
            {
                auto* fmt = formatManager.getFormat (i);
                if (fmt == nullptr)
                    continue;
                juce::OwnedArray<juce::PluginDescription> types;
                fmt->findAllTypesForFile (types, path);
                for (auto* d : types)
                {
                    if (d != nullptr && knownList.addType (*d))
                        ++added;
                }
            }
        }

        savePersistedList();
        lastScanStatus = added > 0
            ? ("Loaded " + juce::String (added) + " VST3")
            : "No VST3 found";
        scanning.store (0);
        if (onDone)
            juce::MessageManager::callAsync ([onDone, added] { onDone (added); });
    });
}

