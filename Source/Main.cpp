#include <JuceHeader.h>
#include "MainComponent.h"

class SessionApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return "F#$*ktheband"; }
    const juce::String getApplicationVersion() override { return "2.0.0"; }
    bool moreThanOneInstanceAllowed() override          { return true; }

    void initialise (const juce::String&) override
    {
        mainWindow = std::make_unique<MainWindow> (getApplicationName());
    }

    void shutdown() override { mainWindow.reset(); }
    void systemRequestedQuit() override { quit(); }

    class MainWindow : public juce::DocumentWindow
    {
    public:
        explicit MainWindow (juce::String name)
            : DocumentWindow (name,
                              juce::Colour (0xff101216),
                              DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new MainComponent(), true);
            setResizable (true, true);
            setResizeLimits (1200, 760, 2560, 1800);

            const auto bounds = defaultWindowBounds();
            setBounds (bounds);
            setVisible (true);
        }

        void closeButtonPressed() override
        {
            JUCEApplication::getInstance()->systemRequestedQuit();
        }

    private:
        static juce::Rectangle<int> defaultWindowBounds()
        {
            auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();
            juce::Rectangle<int> area (1280, 800);
            if (display != nullptr)
                area = display->userArea;

            const int marginX = 32;
            const int marginY = 48;
            int w = juce::jmin (1540, juce::jmax (1200, area.getWidth()  - marginX));
            int h = juce::jmin (960,  juce::jmax (760,  area.getHeight() - marginY));

            if (area.getWidth() < 1100)
                w = juce::jmax (900, area.getWidth() - 16);
            if (area.getHeight() < 680)
                h = juce::jmax (560, area.getHeight() - 32);

            const int x = area.getX() + (area.getWidth()  - w) / 2;
            const int y = area.getY() + (area.getHeight() - h) / 2;
            return { x, y, w, h };
        }

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    };

private:
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION (SessionApplication)
