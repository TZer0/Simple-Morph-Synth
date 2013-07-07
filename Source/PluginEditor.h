#ifndef __PLUGINEDITOR_H_4ACCBAA__
#define __PLUGINEDITOR_H_4ACCBAA__

#include "../JuceLibraryCode/JuceHeader.h"
#include "PluginProcessor.h"

#define PRESETACTIONBUTTONSIZE 25
#define OSCBOXWIDTH 300.f
#define OSCBOXOFFSET 8
#define OSCBOXHEIGHT (WAVEHEIGHT+OSCBOXOFFSET*2)
#define OUTPUTWIDTH 224
#define OUTPUTHEIGHT 144
#define TABBOXWIDTH 208
#define TABOXHEIGHT 192
#define TABBOXPOSY 216
#define PHASESLIDERWIDTH 100
#define ADSRHEIGHT 72
#define ADSRWIDTH 16
#define ADSRSPACING 16
#define BUTTOMBOXY 416

#define WINDOWOFFSET 16

#define TABLEBACKGROUND juce::Colour(0xFF1F1F1F)
#define BACKGROUND juce::Colour(0xFF1C2C2D)
#define COMPBACKGROUND juce::Colour(0xFF3E5659)
#define OSCOUTPUT1 juce::Colour(0xFFFFAC07)
#define OSCOUTPUT2 juce::Colour(0xFF3D9499)
#define HIGHLIGHT juce::Colour(0xFFFF4057)
#define MAINOUTPUT1 juce::Colour(0xFF5B3110)
#define MAINOUTPUT2 juce::Colour(0xFF656565)



static juce::Point<float> OscPoints[3] = { juce::Point<float>(WINDOWOFFSET+OSCBOXOFFSET,WINDOWOFFSET+OSCBOXOFFSET), 
	juce::Point<float>((int) (WINDOWWIDTH-OSCBOXWIDTH-OSCBOXOFFSET), WINDOWOFFSET+OSCBOXOFFSET), juce::Point<float>(248, 248) };
static juce::Point<int> PresetFuncButtonPoints[2] = { OscPoints[0].toInt() + juce::Point<int>(WAVESIZE, 0), OscPoints[1].toInt() + juce::Point<int>((int)WAVESIZE,0) };
static juce::Point<int> TabbedComponentPoints[2] = {juce::Point<int>(WINDOWOFFSET, TABBOXPOSY), juce::Point<int>(WINDOWWIDTH-WINDOWOFFSET-TABBOXWIDTH, TABBOXPOSY)};
static juce::Point<int> ADSRPoints[3] = { juce::Point<int>(0,0), juce::Point<int>(0,0), juce::Point<int>(576,424) };

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
