#include <JuceHeader.h>
#include "MainComponent.h"

class SmartArrangerMenuModel final : public juce::MenuBarModel
{
public:
    std::function<void()> openSettings;
    juce::Component::SafePointer<SmartArrangerStandaloneComponent> component;

    juce::StringArray getMenuBarNames() override { return { "Settings", "View" }; }

    juce::PopupMenu getMenuForIndex (int index, const juce::String&) override
    {
        juce::PopupMenu menu;
        if (index == 0)
            menu.addItem (1, "Internal Sounds and SoundFont…");
        else if (index == 1)
        {
            const int mode = component != nullptr ? component->getViewMode() : 100;
            menu.addSectionHeader ("Layouts");
            menu.addItem (100, "All", true, mode == 100);
            menu.addItem (101, "Style Parts (MIDI Channel and Instruments)", true, mode == 101);
            menu.addItem (102, "Instrument List", true, mode == 102);
            menu.addSeparator();
            menu.addSectionHeader ("Show or Hide Sections");
            menu.addItem (110, "MIDI Controls", true,
                          component != nullptr && component->isViewSectionVisible (110));
            menu.addItem (111, "Style Parts", true,
                          component != nullptr && component->isViewSectionVisible (111));
            menu.addItem (112, "MIDI Out Activity", true,
                          component != nullptr && component->isViewSectionVisible (112));
            menu.addItem (113, "Arranger", true,
                          component != nullptr && component->isViewSectionVisible (113));
        }
        return menu;
    }

    void menuItemSelected (int itemID, int) override
    {
        if ((itemID == 1 || itemID == 2) && openSettings)
            openSettings();
        else if (component != nullptr)
        {
            component->handleViewCommand (itemID);
            menuItemsChanged();
        }
    }
};

class SmartArrangerStandaloneApplication final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "Smart Arranger Standalone"; }
    const juce::String getApplicationVersion() override { return "0.1.0"; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise (const juce::String&) override
    {
        window = std::make_unique<MainWindow> (getApplicationName(), [this] (SmartArrangerStandaloneComponent* component)
        {
            auto safeComponent = juce::Component::SafePointer<SmartArrangerStandaloneComponent> (component);
            menuModel.component = safeComponent;
            menuModel.openSettings = [safeComponent]
            {
                if (safeComponent != nullptr)
                    safeComponent->showSettings();
            };
        });
#if JUCE_MAC
        appleMenuItems.addItem (2, "Settings…");
        juce::MenuBarModel::setMacMainMenu (&menuModel, &appleMenuItems);
#endif
    }

    void shutdown() override
    {
#if JUCE_MAC
        juce::MenuBarModel::setMacMainMenu (nullptr);
#endif
        window.reset();
    }
    void systemRequestedQuit() override { quit(); }

private:
    class MainWindow final : public juce::DocumentWindow
    {
    public:
        MainWindow (const juce::String& name,
                    std::function<void(SmartArrangerStandaloneComponent*)> onContentCreated)
            : DocumentWindow (name, juce::Colour (0xff171b20), allButtons)
        {
            setUsingNativeTitleBar (true);
            auto* component = new SmartArrangerStandaloneComponent();
            setContentOwned (component, true);
            component->onPreferredHeightChanged = [this] (int contentHeight)
            {
                if (auto* content = getContentComponent())
                    setContentComponentSize (content->getWidth(), contentHeight);
            };
            if (onContentCreated)
                onContentCreated (component);
            setResizable (true, true);
            // The MIDI control strip has fixed-width selectors and indicators;
            // keep enough width to prevent those controls and their labels
            // from colliding. Height remains content-driven by the component.
            setResizeLimits (1180, 300, 4000, 3000);
            centreWithSize (1180, 980);
            component->refreshContentHeight();
            setVisible (true);
        }

        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
    };

    SmartArrangerMenuModel menuModel;
    juce::PopupMenu appleMenuItems;
    std::unique_ptr<MainWindow> window;
};

START_JUCE_APPLICATION (SmartArrangerStandaloneApplication)
