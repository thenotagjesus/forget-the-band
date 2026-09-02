#pragma once

#include <JuceHeader.h>

/** User-app-data and documents paths for Forget The Band. */
namespace SessionSettings
{
    inline constexpr const char* kProductFolder = "ForgetTheBand";
    inline constexpr const char* kDisplayName   = "Forget The Band";

    inline juce::File appDir()
    {
        auto d = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                     .getChildFile ("Centrophy")
                     .getChildFile (kProductFolder);
        d.createDirectory();
        return d;
    }

    inline juce::File productDir()
    {
        auto d = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                     .getChildFile ("Centrophy")
                     .getChildFile (kProductFolder);
        d.createDirectory();
        return d;
    }

    inline juce::File deviceXml() { return appDir().getChildFile ("audio.xml"); }
    inline juce::File uiXml()     { return appDir().getChildFile ("ui.xml"); }

    inline juce::File projectsDir()
    {
        auto d = productDir().getChildFile ("projects");
        d.createDirectory();
        return d;
    }

    inline juce::File stemsDir()
    {
        auto d = productDir().getChildFile ("stems");
        d.createDirectory();
        return d;
    }

    /** Shared landing + jam setup. Combo ids in xml are 1-based (existing ui.xml). */
    struct Setup
    {
        bool drumsIn = true;
        bool bassIn  = true;
        bool keysIn  = true;
        int  style   = 0;   // FollowerBand::Style
        int  drumsKit  = 0; // FollowerBand::DrumKit (Acoustic)
        int  bassVoice = 0; // FollowerBand::BassVoice (Finger)
        int  keysVoice = 0; // FollowerBand::KeysVoice (Piano)
        int  form    = 1;   // Song / Radio
        int  scale   = 2;   // Pentatonic
        int  feel    = 0;   // Grid
        int  keyPc   = 4;   // E
        bool followKey = true;
        float bpm    = 112.0f;
        bool slew    = true; // tempo follows the player; false locks landing BPM
        int  phraseBars = 8;

        void writeTo (juce::XmlElement& xml) const
        {
            xml.setAttribute ("drumsIn", drumsIn ? 1 : 0);
            xml.setAttribute ("bassIn",  bassIn  ? 1 : 0);
            xml.setAttribute ("keysIn",  keysIn  ? 1 : 0);
            xml.setAttribute ("style",   style + 1);
            xml.setAttribute ("drumsKit",  drumsKit);
            xml.setAttribute ("bassVoice", bassVoice);
            xml.setAttribute ("keysVoice", keysVoice);
            xml.setAttribute ("form",    form + 1);
            xml.setAttribute ("scale",   scale + 1);
            xml.setAttribute ("feel",    feel + 1);
            xml.setAttribute ("key",     keyPc + 1);
            xml.setAttribute ("autoKey", followKey ? 1 : 0);
            xml.setAttribute ("bpm",     (double) bpm);
            xml.setAttribute ("slew",    slew ? 1 : 0);
            xml.setAttribute ("autoBpm", slew ? 1 : 0);
            xml.setAttribute ("lockTempo", slew ? 0 : 1);
            xml.setAttribute ("phrase",  phraseBars);
        }

        static Setup fromXml (const juce::XmlElement& xml)
        {
            Setup s;
            s.drumsIn = xml.getIntAttribute ("drumsIn", 1) != 0;
            s.bassIn  = xml.getIntAttribute ("bassIn",  1) != 0;
            s.keysIn  = xml.getIntAttribute ("keysIn",  1) != 0;
            s.style   = juce::jlimit (0, 4, xml.getIntAttribute ("style", 1) - 1);
            s.drumsKit  = juce::jlimit (0, 4, xml.getIntAttribute ("drumsKit",  0));
            s.bassVoice = juce::jlimit (0, 3, xml.getIntAttribute ("bassVoice", 0));
            s.keysVoice = juce::jlimit (0, 4, xml.getIntAttribute ("keysVoice", 0));
            if (! xml.hasAttribute ("drumsKit")
                && ! xml.hasAttribute ("bassVoice")
                && ! xml.hasAttribute ("keysVoice"))
            {
                // Old ui.xml: suggest timbres from Style once, then they stick.
                switch (s.style)
                {
                    case 1: s.drumsKit = 0; s.bassVoice = 0; s.keysVoice = 1; break; // Blues
                    case 2: s.drumsKit = 1; s.bassVoice = 1; s.keysVoice = 2; break; // Metal
                    case 3: s.drumsKit = 3; s.bassVoice = 3; s.keysVoice = 4; break; // Funk
                    case 4: s.drumsKit = 2; s.bassVoice = 0; s.keysVoice = 1; break; // Jazz
                    default: s.drumsKit = 0; s.bassVoice = 0; s.keysVoice = 0; break; // Rock
                }
            }
            s.form    = juce::jlimit (0, 3, xml.getIntAttribute ("form",  2) - 1);
            s.scale   = juce::jlimit (0, 3, xml.getIntAttribute ("scale", 3) - 1);
            s.feel    = juce::jlimit (0, 3, xml.getIntAttribute ("feel",  1) - 1);
            s.keyPc   = juce::jlimit (0, 11, xml.getIntAttribute ("key", 5) - 1);
            s.followKey = xml.getIntAttribute ("autoKey", 1) != 0;
            s.bpm     = (float) xml.getDoubleAttribute ("bpm", 112.0);
            s.phraseBars = xml.getIntAttribute ("phrase", 8);
            if (s.phraseBars <= 4) s.phraseBars = 4;
            else if (s.phraseBars >= 16) s.phraseBars = 16;
            else s.phraseBars = 8;
            if (xml.hasAttribute ("slew"))
                s.slew = xml.getIntAttribute ("slew") != 0;
            else
                s.slew = xml.getIntAttribute ("lockTempo", 0) == 0
                      && xml.getIntAttribute ("autoBpm", 1) != 0;
            return s;
        }
    };

    inline Setup loadSetup()
    {
        auto xml = juce::XmlDocument::parse (uiXml());
        if (xml == nullptr)
            return {};
        return Setup::fromXml (*xml);
    }

    inline void mergeSetup (const Setup& s)
    {
        auto xml = juce::XmlDocument::parse (uiXml());
        if (xml == nullptr)
            xml = std::make_unique<juce::XmlElement> ("SessionUI");
        s.writeTo (*xml);
        xml->writeTo (uiXml());
    }
}
