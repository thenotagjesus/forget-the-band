#include "Plugins/PluginHost.h"

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
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("Centrophy").getChildFile ("FtheBand");
    dir.createDirectory();
    return dir.getChildFile ("plugin-list.xml");
}

juce::File PluginHost::deadMansPedalFile() const
{
    return settingsFile().getSiblingFile ("vst3-crashed.txt");
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
    if (docs.getChildFile ("VST3").isDirectory())
        path.add (docs.getChildFile ("VST3"));
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
