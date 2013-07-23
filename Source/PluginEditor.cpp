/*
   ==============================================================================

   This file was auto-generated by the Jucer!

   It contains the basic startup code for a Juce application.

   ==============================================================================
 */

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
	SimpleMorphSynthProcessorEditor::SimpleMorphSynthProcessorEditor (SimpleMorphSynth* ownerFilter)
: AudioProcessorEditor (ownerFilter),
	mMidiKeyboard (ownerFilter->mKeyboardState, MidiKeyboardComponent::horizontalKeyboard)
{
	addKeyListener(this);

	// add the midi keyboard component..
	//addAndMakeVisible (&mMidiKeyboard);

	// set our component's initial size to be the last one that was stored in the filter's settings
	setSize (ownerFilter->mLastUIWidth,
			ownerFilter->mLastUIHeight);
	addSlider(SmoothStrengthParam, juce::Point<int>(380, 424), juce::Point<int>(16, 72));
	addSlider(SmoothRangeParam, juce::Point<int>(400, 424), juce::Point<int>(16, 72));
	addSlider(SynthAmpParam, juce::Point<int>(350, 20), juce::Point<int>(20, (int) WAVEHEIGHT-40),0, 1, 0);
	addSlider(SynthAmpParam, juce::Point<int>(370, 20), juce::Point<int>(20, (int) WAVEHEIGHT-40),0, 1, 1);

	addSlider(AdjustPhaseParam, OscPoints[0].toInt() + juce::Point<int>((int) (OSCBOXWIDTH/2), 
				(int) OSCBOXHEIGHT), juce::Point<int>(PHASESLIDERWIDTH, 20), 0, 1, 0, juce::Slider::SliderStyle::LinearHorizontal);
	addSlider(AdjustPhaseParam, OscPoints[1].toInt() + juce::Point<int>((int) (OSCBOXWIDTH/2-PHASESLIDERWIDTH/2), 
				(int) OSCBOXHEIGHT), juce::Point<int>(PHASESLIDERWIDTH, 20), 0, 1, 1, juce::Slider::SliderStyle::LinearHorizontal);

	for (int i = AmpAttackParam; i <= AmpReleaseParam; i++)
	{
		addSlider((Parameter) i, ADSRPoints[2]+juce::Point<int>((i-AmpAttackParam)*ADSRSPACING, 0), 
				juce::Point<int>(ADSRWIDTH, ADSRHEIGHT));
	}

	TabbedComponent *tabbedComponent;
	addAndMakeVisible (tabbedComponent = new TabbedComponent (TabbedButtonBar::TabsAtTop));
	tabbedComponent->setTabBarDepth (30);
	tabbedComponent->addTab ("Tab 0", Colour (0xff3e5659), 0, false);
	tabbedComponent->addTab ("Tab 1", Colour (0xff314548), 0, false);
	tabbedComponent->addTab ("Tab 2", Colour (0xff314548), 0, false);
	tabbedComponent->setCurrentTabIndex (0);
	tabbedComponent->setBounds (TabbedComponentPoints[0].getX(), TabbedComponentPoints[0].getY(), TABBOXWIDTH, TABOXHEIGHT);



	for (int i = 0; i < 2; i++)
	{
		for (int j = 0; j <= LASTRADIOACTION; j++)
		{
			juce::ToggleButton *button = new juce::ToggleButton(convertActionToString(j));
			button->setRadioGroupId(i + 1);
			button->addListener(this);
			addAndMakeVisible(button);
			mButtons.push_back(button);
		}
	}

	mWaveClicked = -1;
	startTimer (10);
	resized();

	mOscBoxPath1.clear();
    mOscBoxPath1.startNewSubPath (152.0f, 208.0f);
    mOscBoxPath1.lineTo (328.0f, 208.0f);
    mOscBoxPath1.lineTo (328.0f, 232.0f);
    mOscBoxPath1.lineTo (176.0f, 232.0f);
    mOscBoxPath1.closeSubPath();

	mOscBoxPath2.clear();
    mOscBoxPath2.startNewSubPath (392.0f, 208.0f);
    mOscBoxPath2.lineTo (576.0f, 208.0f);
    mOscBoxPath2.lineTo (544.0f, 232.0f);
    mOscBoxPath2.lineTo (392.0f, 232.0f);
    mOscBoxPath2.closeSubPath();

	updateAllComponents();
}

void SimpleMorphSynthProcessorEditor::addSlider(Parameter param, juce::Point<int> point, 
		juce::Point<int> size, double minVal, double maxVal, int target, juce::Slider::SliderStyle style)
{
	ComponentContainer<Slider> cont;
	cont.mParam = param;
	cont.mLabel = new Label(getProcessor()->getParameterName(param));
	cont.mTarget = target;
	Slider *sl = new Slider();
	sl->setBounds(point.getX(), point.getY(), size.getX(), size.getY());

	addAndMakeVisible(sl);
	sl->setSliderStyle(style);
	sl->setTextBoxStyle(juce::Slider::TextEntryBoxPosition::NoTextBox, false, 0, 0);
	sl->addListener(this);
	sl->setRange(minVal, maxVal, 0.01);

	cont.mComponent = sl;

	
	cont.mLabel->attachToComponent (sl, false);
	cont.mLabel->setFont (Font (11.0f));
	addAndMakeVisible(cont.mLabel);

	mSliders.push_back(cont);
}

void SimpleMorphSynthProcessorEditor::mouseDown(const MouseEvent &event)
{
	mLastDrag = event.getPosition().toFloat();
	mWaveClicked = checkIfInWavetable((int)mLastDrag.getX(), (int)mLastDrag.getY());
	if (mWaveClicked != -1)
	{
		checkUserWaveBox(mWaveClicked);
	}
}

void SimpleMorphSynthProcessorEditor::mouseUp(const MouseEvent &)
{
	mWaveClicked = -1;
}

void SimpleMorphSynthProcessorEditor::mouseDrag(const MouseEvent &event)
{
	if (mWaveClicked == -1)
	{
		return;
	}
	auto pos = event.getPosition().toFloat();
	auto dragVec = pos - mLastDrag;
	auto dragDistLeft = dragVec;
	int i = 0;
	while (dragDistLeft.getDistanceFromOrigin() > 0.2f && i < 10000)
	{
		checkIfInWavetable((int)mLastDrag.getX(), (int)mLastDrag.getY(), mWaveClicked);
		auto dPos = (dragVec/dragVec.getDistanceFromOrigin())*0.01f;
		dragDistLeft -= dPos;
		mLastDrag += dPos;
		i++;
	}
	mLastDrag = pos;
	checkIfInWavetable((int)mLastDrag.getX(), (int)mLastDrag.getY(), mWaveClicked );
	repaint();
}

int SimpleMorphSynthProcessorEditor::checkIfInWavetable(int x, int y, int forceTable)
{
	for (size_t i = 0; i < NUMOSC; i++)
	{
		juce::Point<float> table = OscPoints[i];
		int tx = (int) (x - table.getX()); int ty = (int) (y - table.getY());
		if ((0 <= tx && tx < WAVESIZE && 0 <= ty && ty < WAVEHEIGHT && forceTable == -1 ) 
				|| (forceTable != -1 && ((size_t) forceTable) == i))
		{
			getProcessor()->setWaveTableValue(i, tx, ((float) -ty+(WAVEHEIGHT/2))/(WAVEHEIGHT/2));
			return i;
		}
	}

	return -1;
}

SimpleMorphSynthProcessorEditor::~SimpleMorphSynthProcessorEditor()
{
	for (size_t i = 0; i < mButtons.size(); i++)
	{
		delete mButtons[i];
	}

	for (size_t i =0; i < mSliders.size(); i++)
	{
		delete mSliders[i].mComponent;
		delete mSliders[i].mLabel;
	}
}

//==============================================================================
void SimpleMorphSynthProcessorEditor::paint (Graphics& g)
{
	SimpleMorphSynth *synth = getProcessor();
	g.setColour(BACKGROUND);
	g.fillAll();

	for (size_t t = 0; t < NUMOSC; t++)
	{
		juce::Point<float> oscPoint = OscPoints[t];
		float x = oscPoint.getX(); float y = oscPoint.getY();
		g.setColour(COMPBACKGROUND);
		g.fillRect(x-OSCBOXOFFSET, y-OSCBOXOFFSET, OSCBOXWIDTH, OSCBOXHEIGHT);
		g.setColour(TABLEBACKGROUND);

		g.fillRect(x, y, (float) WAVESIZE-1, WAVEHEIGHT);
		g.setGradientFill(juce::ColourGradient(OSCOUTPUT1, x, y, OSCOUTPUT2, x, y+WAVEHEIGHT, false));
		y += (WAVEHEIGHT/2);
		juce::Path pth;
		pth.startNewSubPath(x, y);
		for (int i = 0; i < WAVESIZE; i++)
		{
			float waveVal = getProcessor()->getWaveTableValue(t, i);
			pth.lineTo(x+i, y-waveVal*(WAVEHEIGHT/2));
		}
		pth.lineTo(x+WAVESIZE-1, y);
		g.fillPath(pth);
	}


	juce::Point<float> oscPoint = OscPoints[2];
	float x = oscPoint.getX(); float y = oscPoint.getY();
	g.setColour(TABLEBACKGROUND);
	g.fillRect(x, y, (float) OUTPUTWIDTH-1, (float) OUTPUTHEIGHT);
	g.setGradientFill(juce::ColourGradient(MAINOUTPUT1, x, y, MAINOUTPUT2, x, y+OUTPUTHEIGHT, false));
	y += OUTPUTHEIGHT/2;
	juce::Path pth;
	pth.startNewSubPath(x, y);
	for (int i = 0; i < OUTPUTWIDTH; i++)
	{
		pth.lineTo(x+i, y - synth->getWaveValue(i*(WAVESIZE/((float)OUTPUTWIDTH))) * (OUTPUTHEIGHT/4));
	}
	pth.lineTo(x+OUTPUTWIDTH-1, y);
	g.fillPath(pth);

	g.setColour(COMPBACKGROUND);
	g.fillRect(WINDOWOFFSET, BUTTOMBOXY, WINDOWWIDTH-WINDOWOFFSET*2, WINDOWHEIGHT-BUTTOMBOXY-WINDOWOFFSET);

	g.fillPath(mOscBoxPath1);
	g.fillPath(mOscBoxPath2);
}

void SimpleMorphSynthProcessorEditor::resized()
{

	const int keyboardHeight = 70;
	mMidiKeyboard.setBounds (4, getHeight() - keyboardHeight - 4, 700-WAVESIZE-8, keyboardHeight);

	size_t k = 0;
	for (int i = 0; i < 2; i++)
	{
		juce::Point<int> buttonPoint = PresetFuncButtonPoints[i];
		int x = buttonPoint.getX(); int y = buttonPoint.getY();

		for (int j = 0; j <= LASTRADIOACTION; j++)
		{
			if (mButtons.size() <= k)
			{
				break;
			}
			mButtons[k]->setBounds(x, y+j*PRESETACTIONBUTTONSIZE, PRESETACTIONBUTTONSIZE+60, PRESETACTIONBUTTONSIZE);
			k++;
		}
	}

	getProcessor()->mLastUIWidth = getWidth();
	getProcessor()->mLastUIHeight = getHeight();
}

//==============================================================================
// This timer periodically checks whether any of the filter's parameters have changed...
void SimpleMorphSynthProcessorEditor::timerCallback()
{
	SimpleMorphSynth* synth = getProcessor();

	AudioPlayHead::CurrentPositionInfo newPos (synth->mLastPosInfo);

	if (synth->mUpdateEditor)
	{
		updateAllComponents();
		synth->mUpdateEditor = false;
		repaint();
	}
}

void SimpleMorphSynthProcessorEditor::updateAllComponents()
{
	SimpleMorphSynth* synth = getProcessor();

	for (size_t i = 0; i < mSliders.size(); i++)
	{
		mSliders[i].mComponent->setValue(synth->getParameter(mSliders[i].mParam + ((int)mSliders[i].mTarget)*SYNTHPARAMS), dontSendNotification);
	}
}

// This is our Slider::Listener callback, when the user drags a slider.
void SimpleMorphSynthProcessorEditor::sliderValueChanged (Slider* slider)
{
	for (size_t i = 0; i < mSliders.size(); i++)
	{
		if (mSliders[i].mComponent == slider)
		{

			if (mSliders[i].mParam == AdjustPhaseParam)
			{
				checkUserWaveBox(mSliders[i].mTarget);
			}
			
			updateParam(mSliders[i].mParam + ((int)mSliders[i].mTarget)*SYNTHPARAMS,
					(float) slider->getValue());
			repaint();
		}
	}
}

void SimpleMorphSynthProcessorEditor::updateParam(int param, float val)
{
	auto *synth = getProcessor();
	bool pre = synth->mUpdateEditor;
	synth->setParameterNotifyingHost(param, val);
	synth->mUpdateEditor = pre;
}

void SimpleMorphSynthProcessorEditor::checkUserWaveBox(int target)
{
	if (target < 0 || 1 < target)
	{
		return;
	}
	size_t i = LASTRADIOACTION + target * (LASTRADIOACTION + 1);
	mButtons[i]->triggerClick();
}

void SimpleMorphSynthProcessorEditor::sliderDragEnded (Slider *)
{
}


void SimpleMorphSynthProcessorEditor::buttonClicked(Button *button)
{
	size_t k = 0;
	for (size_t i = 0; i < 2; i++)
	{
		for (size_t j = 0; j <= LASTRADIOACTION; j++)
		{
			if (mButtons.size() <= k)
			{
				break;
			}
			if (mButtons[k] == button)
			{
				getProcessor()->mWaveTables[i].executeAction(j);
				repaint();
				return;
			}
			k++;
		}
	}
}

void SimpleMorphSynthProcessorEditor::buttonStateChanged(Button *button)
{
}
