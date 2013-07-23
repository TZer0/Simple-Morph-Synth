#include "PluginProcessor.h"
#include "PluginEditor.h"

AudioProcessor* JUCE_CALLTYPE createPluginFilter();

juce::String convertActionToString(int act)
{
	Action c_act = (Action) act;
	switch(c_act)
	{
		case SinFunc: return juce::String("Sinus");
		case CosFunc: return juce::String("Cosinus");
		case SawFunc: return juce::String("Saw");
		case SquareFunc: return juce::String("Square");
		case TriFunc: return juce::String("Tri");
		case UserFunc: return juce::String("User");
		case Reverse: return juce::String("Reverse");
		case Flip: return juce::String("Flip");
		case MeanFilter: return juce::String("MeanFilter");
	}
	return juce::String("Actionstring not defined");
}

class WaveTableSound : public SynthesiserSound
{
public:
	WaveTableSound()
	{}

	bool appliesToNote (const int /*midiNoteNumber*/)			{ return true; }
	bool appliesToChannel (const int /*midiChannel*/)			{ return true; }
};

class WaveTableVoice  : public SynthesiserVoice
{
public:
	WaveTableVoice(SimpleMorphSynth *synth)
		: mTimeDelta (0.0),
		mCyclesPerSecond (0.0),
		mTailOff (0.0),
		mReleaseTime (0.0),
		mReleased (false)
{
	mSynth = synth;
}

	bool canPlaySound (SynthesiserSound* sound)
	{
		return dynamic_cast <WaveTableSound*> (sound) != 0;
	}

	void startNote (const int midiNoteNumber, const float velocity,
			SynthesiserSound* /*sound*/, const int /*currentPitchWheelPosition*/)
	{
		mReleased = false;
		mCurrentTime = 0.0;
		mLevel = velocity * 0.15;
		mTailOff = 0.0;

		mCyclesPerSecond = MidiMessage::getMidiNoteInHertz (midiNoteNumber);
		mTimeDelta = 1 / getSampleRate();
	}

	void stopNote (const bool allowTailOff)
	{
		if (allowTailOff)
		{
			// start a tail-off by setting this flag. The render callback will pick up on
			// this and do a fade out, calling clearCurrentNote() when it's finished.
			mReleased = true;
			mReleaseTime = 0.0;
		}
		else
		{
			// we're being told to stop playing immediately, so reset everything..

			clearCurrentNote();
			mTimeDelta = 0.0;
		}
	}

	void pitchWheelMoved (const int /*newValue*/)
	{
		// can't be bothered implementing this for the demo!
	}

	void controllerMoved (const int /*controllerNumber*/, const int /*newValue*/)
	{
		// not interested in controllers in this case.
	}

	void renderNextBlock (AudioSampleBuffer& outputBuffer, int startSample, int numSamples)
	{
		if (mTimeDelta != 0.0)
		{
			float attack = std::max(mSynth->mADSRTables[2].mAttack, DECLICK);
			float decay = mSynth->mADSRTables[2].mDecay;
			float sustain = mSynth->mADSRTables[2].mSustain;
			float release = std::max(mSynth->mADSRTables[2].mRelease, DECLICK);

			while (--numSamples >= 0)
			{
				float pos = (float) (mCurrentTime * WAVESIZE * mCyclesPerSecond);

				float currentSample = (float) (mSynth->getWaveValue(pos) * mLevel * (mTailOff > 0 ? mTailOff : 1));
				double modTime = mCurrentTime;

				if (modTime < attack && attack > 0)
				{
					currentSample *= (float) (modTime/attack);
				} else
				{
					modTime -= attack;
					if (modTime < decay && decay > 0)
					{
						double decProg = modTime/decay;
						currentSample *= (float) (sustain * decProg + (1-decProg));
					} else
					{
						currentSample *= (float) sustain;
					}
				}

				if (mReleased && release > 0)
				{
					if (mReleaseTime < release)
					{
						currentSample *= (float) (1 - mReleaseTime/release);
						mReleaseTime += mTimeDelta;
					} else
					{
						currentSample = 0;
						//mReleased = false;
						clearCurrentNote();
					}
				} else if (mReleased)
				{
					currentSample = 0;
					clearCurrentNote();
				}

				for (int i = outputBuffer.getNumChannels(); --i >= 0;)
					*outputBuffer.getSampleData (i, startSample) += currentSample;

				mCurrentTime += mTimeDelta;
				++startSample;
			}
		}
	}

private:
	double mCurrentTime, mTimeDelta, mLevel, mTailOff, mCyclesPerSecond, mReleaseTime;
	bool mReleased;
	SimpleMorphSynth *mSynth;
};


//==============================================================================
	SimpleMorphSynth::SimpleMorphSynth()
: mDelayBuffer (2, 12000)
{
	mUpdateEditor = true;
	// Set up some default values..
	mSmoothStrengthFactor = mSmoothRangeFactor = 0;


	mLastUIWidth = WINDOWWIDTH;
	mLastUIHeight = WINDOWHEIGHT;

	mLastPosInfo.resetToDefault();
	mDelayPosition = 0;
	mWaveTables[0] = WaveTable();
	mWaveTables[1] = WaveTable();
	mWaveTables[0].executeAction(SawFunc);
	mWaveTables[0].executeAction(Reverse);
	mWaveTables[1].executeAction(SinFunc);
	mWaveTables[1].executeAction(Flip);

	for (int i = 0; i < 3; i++)
	{
		mADSRTables[i] = ADSRTable();
	}

	// Initialise the synth...
	for (int i = 4; --i >= 0;)
		mSynth.addVoice (new WaveTableVoice(this));		// These voices will play our custom sine-wave sounds..

	mSynth.addSound (new WaveTableSound());

}

float SimpleMorphSynth::getWaveValue(float pos)
{
	int range = (int) (mSmoothRangeFactor*16);
	if (range < 1)
	{
		range = 1;
	}
	int curSampPos = (int) pos;
	float posOffset = (float) pos-curSampPos;
	float outVal = 0.f;
	float regTotal = 0.0;
	for (size_t i = 0; i < NUMOSC; i++)
	{
		for (int j = -range; j <= range; j++)
		{
			int aPos = (curSampPos+j)%WAVESIZE;
			if (aPos < 0)
			{
				aPos += WAVESIZE;
			}
			float from = mWaveTables[i].getWaveValue(aPos);
			float to = mWaveTables[i].getWaveValue(aPos+1);
			float smooth = (range-std::abs(j)*mSmoothStrengthFactor);
			regTotal += smooth;
			outVal += mAmplifiers[i].mAmp *(from*(1-posOffset) + to*posOffset) * smooth * 2;
		}
	}
	if (regTotal > 0.1f)
	{
		outVal /= regTotal;
	}

	return outVal;
}

SimpleMorphSynth::~SimpleMorphSynth()
{
}

float SimpleMorphSynth::getWaveTableValue(size_t table, int pos)
{
	if (table < NUMOSC && 0 <= pos && pos < WAVESIZE)
	{
		return mWaveTables[table].getWaveValue(pos);
	}
	return 0.;
}

void SimpleMorphSynth::setWaveTableValue(size_t table, int pos, float value)
{
	if (table < NUMOSC)
	{
		mWaveTables[table].setWaveSection(pos, value);
	}
}

WaveTable *SimpleMorphSynth::getWaveTable(size_t table)
{
	if (table < NUMOSC)
	{
		return &mWaveTables[table];
	}
	return NULL;
}

//==============================================================================
int SimpleMorphSynth::getNumParameters()
{
	return TOTALNUMPARAMS;
}

float SimpleMorphSynth::getParameter (int index)
{
	int target = 0;
	if (index >= LastParam)
	{
		index = index - SYNTHPARAMS;
		target++;
	}

	switch (index)
	{
		case SmoothStrengthParam:		return mSmoothStrengthFactor;
		case SmoothRangeParam:			return mSmoothRangeFactor;
		case AmpAttackParam:			return mADSRTables[2].mAttack;
		case AmpSustainParam:			return mADSRTables[2].mSustain;
		case AmpDecayParam:				return mADSRTables[2].mDecay;
		case AmpReleaseParam:			return mADSRTables[2].mRelease;
		case SynthAmpParam:				return mAmplifiers[target].mAmp;
		case SynthAttackParam:			return mADSRTables[target].mAttack;
		case SynthSustainParam:			return mADSRTables[target].mSustain;
		case SynthDecayParam:			return mADSRTables[target].mDecay;
		case SynthReleaseParam:			return mADSRTables[target].mRelease;
		default:						return 0.0f;
	}
}

const String SimpleMorphSynth::getParameterName (int index)
{
	int target = 0;
	if (index >= LastParam)
	{
		index = index - SYNTHPARAMS;
		target++;
	}
	juce::String tOsc =  juce::String(target+1);
	switch (index)
	{
		case SmoothStrengthParam:		return "SmoothStrength";
		case SmoothRangeParam:			return "SmoothRange";
		case AmpAttackParam:			return "AmpAttackParam";
		case AmpDecayParam:				return "AmpDecayParam";
		case AmpReleaseParam:			return "AmpReleaseParam";
		case AmpSustainParam:			return "AmpSustainParam";
		case AdjustPhaseParam:			return juce::String("AdjustOSCPhase") + tOsc;
		case SynthAmpParam:				return juce::String("SynthAmpParam") + tOsc;
		case SynthAttackParam:			return juce::String("SynthAttackParam") + tOsc;
		case SynthDecayParam:			return juce::String("SyntDecayParam") + tOsc;
		case SynthSustainParam:			return juce::String("SynthSustainParam") + tOsc;
		case SynthReleaseParam:			return juce::String("SynthReleaseParam") + tOsc;

		default:				break;
	}

	return "Undefined parameter name.";
}


void SimpleMorphSynth::setParameter (int index, float newValue)
{
	mUpdateEditor = true;
	int target = 0;
	if (index >= LastParam)
	{
		index = index - SYNTHPARAMS;
		target++;
	}
	// This method will be called by the host, probably on the audio thread, so
	// it's absolutely time-critical. Don't use critical sections or anything
	// UI-related, or anything at all that may block in any way!
	switch (index)
	{
		case SmoothStrengthParam:		mSmoothStrengthFactor = newValue; break;
		case SmoothRangeParam:			mSmoothRangeFactor = newValue; break;
		case AmpAttackParam:			mADSRTables[2].mAttack = newValue; break;
		case AmpSustainParam:			mADSRTables[2].mSustain = newValue; break;
		case AmpDecayParam:				mADSRTables[2].mDecay = newValue; break;
		case AmpReleaseParam:			mADSRTables[2].mRelease = newValue; break;
		case SynthAmpParam:				mAmplifiers[target].mAmp = newValue; break;
		case AdjustPhaseParam:			mWaveTables[target].executeAction(AdjustPhase, newValue); break;
		case SynthAttackParam:			mADSRTables[target].mAttack = newValue; break;
		case SynthSustainParam:			mADSRTables[target].mSustain = newValue; break;
		case SynthDecayParam:			mADSRTables[target].mDecay = newValue; break;
		case SynthReleaseParam:			mADSRTables[target].mRelease = newValue; break;
		default:   break;

	}
}


const String SimpleMorphSynth::getParameterText (int index)
{
	return String (getParameter (index), 2);
}

//==============================================================================
void SimpleMorphSynth::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
	// Use this method as the place to do any pre-playback
	// initialisation that you need..
	mSynth.setCurrentPlaybackSampleRate (sampleRate);
	mKeyboardState.reset();
	mDelayBuffer.clear();
}

void SimpleMorphSynth::releaseResources()
{
	// When playback stops, you can use this as an opportunity to free up any
	// spare memory, etc.
	mKeyboardState.reset();
}

void SimpleMorphSynth::reset()
{
	// Use this method as the place to clear any delay lines, buffers, etc, as it
	// means there's been a break in the audio's continuity.
	mDelayBuffer.clear();
}

void SimpleMorphSynth::processBlock (AudioSampleBuffer& buffer, MidiBuffer& midiMessages)
{
	const int numSamples = buffer.getNumSamples();
	int channel, dp = 0;

	// Go through the incoming data, and apply our gain to it...
	for (channel = 0; channel < getNumInputChannels(); ++channel)
		buffer.applyGain (channel, 0, buffer.getNumSamples(), 1);

	// Now pass any incoming midi messages to our keyboard state object, and let it
	// add messages to the buffer if the user is clicking on the on-screen keys
	mKeyboardState.processNextMidiBuffer (midiMessages, 0, numSamples, true);

	// and now get the synth to process these midi events and generate its output.
	mSynth.renderNextBlock (buffer, midiMessages, 0, numSamples);

	// Apply our delay effect to the new output..
	/*for (channel = 0; channel < getNumInputChannels(); ++channel)
	  {
	  float* channelData = buffer.getSampleData (channel);
	  float* delayData = mDelayBuffer.getSampleData (jmin (channel, mDelayBuffer.getNumChannels() - 1));
	  dp = mDelayPosition;

	  for (int i = 0; i < numSamples; ++i)
	  {
	  const float in = channelData[i];
	  channelData[i] += delayData[dp];
	  delayData[dp] = (delayData[dp] + in);
	  if (++dp >= mDelayBuffer.getNumSamples())
	  dp = 0;
	  }
	  }*/

	mDelayPosition = dp;

	// In case we have more outputs than inputs, we'll clear any output
	// channels that didn't contain input data, (because these aren't
	// guaranteed to be empty - they may contain garbage).
	for (int i = getNumInputChannels(); i < getNumOutputChannels(); ++i)
		buffer.clear (i, 0, buffer.getNumSamples());

	// ask the host for the current time so we can display it...
	AudioPlayHead::CurrentPositionInfo newTime;

	if (getPlayHead() != nullptr && getPlayHead()->getCurrentPosition (newTime))
	{
		// Successfully got the current time from the host..
		mLastPosInfo = newTime;
	}
	else
	{
		// If the host fails to fill-in the current time, we'll just clear it to a default..
		mLastPosInfo.resetToDefault();
	}
}

//==============================================================================
AudioProcessorEditor* SimpleMorphSynth::createEditor()
{
	return new SimpleMorphSynthProcessorEditor (this);
}

//==============================================================================
void SimpleMorphSynth::getStateInformation (MemoryBlock& destData)
{
	// You should use this method to store your parameters in the memory block.
	// Here's an example of how you can use XML to make it easy and more robust:

	XmlElement xml ("SIMPLEMORPHSSYNTH");

	xml.setAttribute ("mSmoothStrengthFactor", mSmoothStrengthFactor);
	xml.setAttribute ("mSmoothRangeFactor", mSmoothRangeFactor);

	for (size_t w = 0; w < NUMOSC; w++) 
	{
		for (size_t i = 0; i < WAVESIZE; i++)
		{
			xml.setAttribute(juce::String("wave") + juce::String(w) + juce::String("-") + juce::String(i), mWaveTables[w].mWave[i]);
		}
		xml.setAttribute(juce::String("oscoffset") + juce::String(w), mWaveTables[w].mOscOffset);
	}

	for (size_t i = 0; i < NUMDEVINST; i++)
	{
		juce::String table = juce::String(i);
		xml.setAttribute(juce::String("attack") + table, mADSRTables[i].mAttack);
		xml.setAttribute(juce::String("decay") + table, mADSRTables[i].mDecay);
		xml.setAttribute(juce::String("sustain") + table, mADSRTables[i].mSustain);
		xml.setAttribute(juce::String("release") + table, mADSRTables[i].mRelease);
		xml.setAttribute(juce::String("amp") + table, mAmplifiers[i].mAmp);
	}

	// then use this helper function to stuff it into the binary blob and return it..
	copyXmlToBinary (xml, destData);
}

void SimpleMorphSynth::setStateInformation (const void* data, int sizeInBytes)
{
	// You should use this method to restore your parameters from this memory block,
	// whose contents will have been created by the getStateInformation() call.

	// This getXmlFromBinary() helper function retrieves our XML from the binary blob..
	ScopedPointer<XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
	mUpdateEditor = true;

	if (xmlState != nullptr)
	{
		if (xmlState->hasTagName ("SIMPLEMORPHSSYNTH"))
		{
			mSmoothStrengthFactor  = (float) xmlState->getDoubleAttribute ("mSmoothStrengthFactor", 0.0);
			mSmoothRangeFactor  = (float) xmlState->getDoubleAttribute ("mSmoothRangeFactor", 1.0);
			for (size_t w = 0; w < NUMOSC; w++) 
			{
				for (size_t i = 0; i < WAVESIZE; i++)
				{
					mWaveTables[w].mWave[i] = (float) xmlState->getDoubleAttribute(juce::String("wave") + juce::String(w) + juce::String("-") + juce::String(i));
				}
				mWaveTables[w].mOscOffset = (float) xmlState->getDoubleAttribute(juce::String("oscoffset") + juce::String(w), 0.0);
			}

			for (size_t i = 0; i < NUMDEVINST; i++)
			{
				juce::String table = juce::String(i);
				mADSRTables[i].mAttack = (float) xmlState->getDoubleAttribute(juce::String("attack") + table);
				mADSRTables[i].mDecay = (float) xmlState->getDoubleAttribute(juce::String("decay") + table);
				mADSRTables[i].mSustain = (float) xmlState->getDoubleAttribute(juce::String("sustain") + table, 1.0);
				mADSRTables[i].mRelease = (float) xmlState->getDoubleAttribute(juce::String("release") + table);
				mAmplifiers[i].mAmp = (float) xmlState->getDoubleAttribute(juce::String("amp") + table, 1.0);
			}
		}
	}
}

const String SimpleMorphSynth::getInputChannelName (const int channelIndex) const
{
	return String (channelIndex + 1);
}

const String SimpleMorphSynth::getOutputChannelName (const int channelIndex) const
{
	return String (channelIndex + 1);
}

bool SimpleMorphSynth::isInputChannelStereoPair (int /*index*/) const
{
	return true;
}

bool SimpleMorphSynth::isOutputChannelStereoPair (int /*index*/) const
{
	return true;
}

bool SimpleMorphSynth::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
	return true;
#else
	return false;
#endif
}

bool SimpleMorphSynth::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
	return true;
#else
	return false;
#endif
}

bool SimpleMorphSynth::silenceInProducesSilenceOut() const
{
	return false;
}

double SimpleMorphSynth::getTailLengthSeconds() const
{
	return 0.0;
}

//==============================================================================
// This creates new instances of the plugin..
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
	return new SimpleMorphSynth();
}
