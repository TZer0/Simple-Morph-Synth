#ifndef __PLUGINEDITOR_H_4ACCBAA__
#define __PLUGINEDITOR_H_4ACCBAA__

#include "../JuceLibraryCode/JuceHeader.h"
#include "PluginProcessor.h"

#define PRESETACTIONBUTTONSIZE 25

static juce::Point<float> OscPoints[3] = { juce::Point<float>(0,0), juce::Point<float>(500, 0), juce::Point<float>(250, 250) };
static juce::Point<int> PresetFuncButtonPoints[2] = { OscPoints[0].toInt() + juce::Point<int>(WAVESIZE, 0), OscPoints[1].toInt() + juce::Point<int>(-60,0) };



class ComponentContainer
{
public:
	ComponentContainer() { mLabel = nullptr; mComponent = nullptr; mParam = NoneParam; mTarget = 0;}
	ComponentContainer(Label *label, Component *comp, Parameter p, int t) {
		mLabel = label;
		mComponent = comp;
		mParam = p;
		mTarget = t;
	}
	Label *mLabel;
	Component *mComponent;
	Parameter mParam;
	size_t mTarget;
};

class SimpleMorphSynthProcessorEditor  : public AudioProcessorEditor,
                                            public SliderListener,
                                            public Timer,
											public Button::Listener
{
public:
    SimpleMorphSynthProcessorEditor (SimpleMorphSynth* ownerFilter);
    ~SimpleMorphSynthProcessorEditor();

    //==============================================================================
    void timerCallback();
    void paint (Graphics& g);
    void resized();
    void sliderValueChanged (Slider*);
	void mouseUp(const MouseEvent &event);
	void mouseDown(const MouseEvent &event);
	void mouseDrag(const MouseEvent &event);

private:
    MidiKeyboardComponent mMidiKeyboard;

	int mWaveClicked;
	juce::Slider *mDraggingSlider;
	juce::Point<float> mLastDrag;
	bool mDragging;
	int checkIfInWavetable(int x, int y, int forceTable = -1);
	AudioPlayHead::CurrentPositionInfo mLastDisplayedPosition;
	std::vector<juce::ToggleButton *> mButtons;
	std::vector<ComponentContainer> mSliders;
	void buttonClicked(Button *button);
	void buttonStateChanged(Button *);
	void addSlider(Parameter param, juce::Point<int> point, juce::Point<int> size, double minVal = 0.0, double maxVal = 1.0, int target = 0,
		juce::Slider::SliderStyle style = juce::Slider::SliderStyle::LinearVertical);
	
    SimpleMorphSynth* getProcessor() const
    {
        return static_cast <SimpleMorphSynth*> (getAudioProcessor());
    }

};


#endif  // __PLUGINEDITOR_H_4ACCBAA__
