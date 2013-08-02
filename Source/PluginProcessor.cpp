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
	WaveTableVoice(SimpleMorphSynth *proc)
		: mTimeDelta (0.0),
		mCyclesPerSecond (0.0),
		mReleaseTime (0.0),
		mResampleTime(0.0),
		mReleased (false),
		mDone(true),
		mDeclickPreviousNote(false),
		mProc(proc)
{

}

	bool canPlaySound (SynthesiserSound* sound)
	{
		return dynamic_cast <WaveTableSound*> (sound) != 0;
	}

	void startNote (const int midiNoteNumber, const float velocity,
			SynthesiserSound* /*sound*/, const int /*currentPitchWheelPosition*/)
	{
		for (int i = 0; i < mProc->mSynth.getNumVoices(); i++)
		{
			auto *voice = mProc->mSynth.getVoice(i);
			if (midiNoteNumber == voice->getCurrentlyPlayingNote())
			{
				voice->stopNote(false);
				break;
			}
		}
		clearCurrentNote();
		mReleased = false;
		mDone = false;
		mCurrentTime = 0.0;
		mReleaseTime = 0.0;
		mLevel = velocity * 0.15;

		mCyclesPerSecond = MidiMessage::getMidiNoteInHertz (midiNoteNumber);
		mTimeDelta = 1 / getSampleRate();
		mCoeff.makeLowPass(getSampleRate(), 400);
		mFilter.setCoefficients(mCoeff);
		mFilter.reset();
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
			clear();
			mDeclickPreviousNote = true;
		}
	}

	void clear()
	{
		
		clearCurrentNote();
		mDone = true;
		mReleaseTime = 0.0;
		mReleased = false;
		mCurrentTime = 0.0;
	}

	void pitchWheelMoved (const int /*newValue*/)
	{
	}

	void controllerMoved (const int /*controllerNumber*/, const int /*newValue*/)
	{
	}

	void renderNextBlock (AudioSampleBuffer& outputBuffer, int startSample, int numSamples)
	{
		if (mDone)
		{
			return;
		}
		int cnt = numSamples;
		if (mTimeDelta != 0.0)
		{
			double resampleSteps = (1.f/mCyclesPerSecond) * MAXSAMPLERATE * mProc->mSampleRate;
			ADSRTable *table = &mProc->mADSRTables[2];
			while (--cnt >= 0)
			{
				float pos = (float) (mCurrentTime * WAVESIZE * mCyclesPerSecond);

				float currentSample = 0.0;
				mCurrentTime += mTimeDelta;
				mResampleTime += mTimeDelta;
				if (mResampleTime > resampleSteps || mLastLevels.size() == 0)
				{
					currentSample = (float) (mProc->getWaveValue(pos, (float) mCurrentTime, mReleased, (float) mReleaseTime));
					mResampleTime -= resampleSteps;
					currentSample *= table->getMod((float) mCurrentTime, mReleased, (float) mReleaseTime);
					currentSample *= (float) mLevel;
					mLastLevels.clear();
					for (int i = outputBuffer.getNumChannels(); --i >= 0;)
					{
						mLastLevels.push_back(currentSample);
					}
				} else
				{
					currentSample = mLastLevels[0];
				}

				if (mReleased)
				{
					mReleaseTime += mTimeDelta;
					if (table->isReleased((float)mReleaseTime))
					{
							clear();
							mDeclickPreviousNote = false;
							return;
					}
				}

				for (int i = outputBuffer.getNumChannels(); --i >= 0;)
				{
					float *ptr = outputBuffer.getSampleData (i, startSample);
					*ptr += currentSample;
					if (mDeclickPreviousNote && i < (int) mLastLevels.size())
					{
						 *ptr += (float) (mLastLevels[i] * std::max(0., ((NOTEDECLICK-mCurrentTime)/NOTEDECLICK)));
					}
				}
				
				++startSample;
			}
			
			//mFilter.processSamples(outputBuffer.getSampleData(0), outputBuffer.getNumSamples());
		}
	}

private:
	double mCurrentTime, mTimeDelta, mLevel, mCyclesPerSecond, mReleaseTime, mResampleTime;
	std::vector<float> mLastLevels;
	bool mReleased, mDone, mDeclickPreviousNote;
	IIRFilter mFilter;
	IIRCoefficients mCoeff;
	SimpleMorphSynth *mProc;
};


//==============================================================================
	SimpleMorphSynth::SimpleMorphSynth()
{
	mSampleRate = 0.0;
	mUpdateEditor = true;
	// Set up some default values..
	mSmoothStrengthFactor = mSmoothRangeFactor = 0;


	mLastUIWidth = WINDOWWIDTH;
	mLastUIHeight = WINDOWHEIGHT;

	mLastPosInfo.resetToDefault();
	mSmoothJaggedFactor = 0;
	mWaveTables[0] = WaveTable();
	mWaveTables[1] = WaveTable();
	mWaveTables[0].executeAction(SawFunc);
	mWaveTables[0].executeAction(Reverse);
	mWaveTables[1].executeAction(SinFunc);
	mWaveTables[1].executeAction(Flip);

	for (int i = 0; i < 3; i++)
	{
		mADSRTables[i] = ADSRTable();
		if (i < 2)
		{
			mADSRTables[i].mRelease = 1.0;
		}
	}


	
	mVoiceFactor = 0.25;
	adjustVoices();

	mSynth.addSound (new WaveTableSound());
}

float SimpleMorphSynth::getWaveValue(float pos, float time, bool released, float releasetime)
{
	int range = (int) (mSmoothRangeFactor*32);
	if (range < 1)
	{
		range = 1;
	}
	int minJumps = 1 + (range > 16) + (range > 24) * 2;
	int jumps = std::min(std::max(minJumps, (int) (mSmoothJaggedFactor*16)), range);
	if (range > jumps)
	{
		range -= (range % jumps);
	}

	int curSampPos = (int) pos;
	float posOffset = (float) pos-curSampPos;
	float outVal = 0.f;
	float regTotal = 0.0;
	for (size_t i = 0; i < NUMOSC; i++)
	{
		float tmpVal = 0;
		for (int j = -range; j <= range; j += jumps)
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
			tmpVal += mAmplifiers[i].mAmp *(from*(1-posOffset) + to*posOffset) * smooth * 2;
		}
		outVal += tmpVal * (time > 0 ? mADSRTables[i].getMod(time, released, releasetime) : 1);
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
		case SmoothJaggedParam:			return mSmoothJaggedFactor;
		case AdjustVoices:				return mVoiceFactor;
		case AdjustSampleRate:			return mSampleRate;
		case AmpAttackParam:			return mADSRTables[2].mAttack;
		case AmpSustainParam:			return mADSRTables[2].mSustain;
		case AmpDecayParam:				return mADSRTables[2].mDecay;
		case AmpReleaseParam:			return mADSRTables[2].mRelease;
		case SynthAmpParam:				return mAmplifiers[target].mAmp;
		case SynthAttackParam:			return mADSRTables[target].mAttack;
		case SynthSustainParam:			return mADSRTables[target].mSustain;
		case SynthDecayParam:			return mADSRTables[target].mDecay;
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
		case SmoothJaggedParam:			return "SmoothJagged";
		case AdjustVoices:				return "VoiceFactor";
		case AdjustSampleRate:			return "SampleRate";
		case AmpAttackParam:			return "AmpAttackParam";
		case AmpDecayParam:				return "AmpDecayParam";
		case AmpReleaseParam:			return "AmpReleaseParam";
		case AmpSustainParam:			return "AmpSustainParam";
		case AdjustPhaseParam:			return juce::String("AdjustOSCPhase") + tOsc;
		case SynthAmpParam:				return juce::String("SynthAmpParam") + tOsc;
		case SynthAttackParam:			return juce::String("SynthAttackParam") + tOsc;
		case SynthDecayParam:			return juce::String("SynthDecayParam") + tOsc;
		case SynthSustainParam:			return juce::String("SynthSustainParam") + tOsc;

		default:				break;
	}

	return "Undefined parameter name.";
}

const String SimpleMorphSynth::getShortParameterName (int index)
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
		case SmoothStrengthParam:		return "STR";
		case SmoothRangeParam:			return "RAN";
		case SmoothJaggedParam:			return "JAG";
		case AdjustVoices:				return "VCY";
		case AdjustSampleRate:			return "SAR";
		case AmpAttackParam:			return "ATK";
		case AmpDecayParam:				return "DCY";
		case AmpReleaseParam:			return "REL";
		case AmpSustainParam:			return "SUS";
		case AdjustPhaseParam:			return "PHASE";
		case SynthAmpParam:				return juce::String("AMP") + tOsc;
		case SynthAttackParam:			return juce::String("ATK") + tOsc;
		case SynthDecayParam:			return juce::String("DCY") + tOsc;
		case SynthSustainParam:			return juce::String("SUS") + tOsc;

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
		case SmoothJaggedParam:			mSmoothJaggedFactor = newValue; break;
		case AdjustVoices:				mVoiceFactor = newValue; adjustVoices(); break;
		case AdjustSampleRate:				mSampleRate = newValue; break;
		case AmpAttackParam:			mADSRTables[2].mAttack = std::max(newValue, DECLICK); break;
		case AmpSustainParam:			mADSRTables[2].mSustain = newValue; break;
		case AmpDecayParam:				mADSRTables[2].mDecay = newValue; break;
		case AmpReleaseParam:			mADSRTables[2].mRelease = std::max(newValue, DECLICK); break;
		case SynthAmpParam:				mAmplifiers[target].mAmp = newValue; break;
		case AdjustPhaseParam:			mWaveTables[target].executeAction(AdjustPhase, newValue); break;
		case SynthAttackParam:			mADSRTables[target].mAttack = std::max(newValue, DECLICK); break;
		case SynthSustainParam:			mADSRTables[target].mSustain = newValue; break;
		case SynthDecayParam:			mADSRTables[target].mDecay = newValue; break;
		default:   break;

	}
}

void SimpleMorphSynth::adjustVoices()
{
	int voices = std::max(1, (int) (mVoiceFactor*16));
	
	while (voices > mSynth.getNumVoices())
	{
		mSynth.addVoice (new WaveTableVoice(this));
	}

	while (voices < mSynth.getNumVoices())
	{
		mSynth.removeVoice(mSynth.getNumVoices()-1);
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
}

void SimpleMorphSynth::releaseResources()
{
	// When playback stops, you can use this as an opportunity to free up any
	// spare memory, etc.
	mKeyboardState.reset();
}

void SimpleMorphSynth::reset()
{

}

void SimpleMorphSynth::processBlock (AudioSampleBuffer& buffer, MidiBuffer& midiMessages)
{
	const int numSamples = buffer.getNumSamples();
	int channel;

	// Go through the incoming data, and apply our gain to it...
	for (channel = 0; channel < getNumInputChannels(); ++channel)
	{
		buffer.applyGain (channel, 0, buffer.getNumSamples(), 1);
	}

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


	// In case we have more outputs than inputs, we'll clear any output
	// channels that didn't contain input data, (because these aren't
	// guaranteed to be empty - they may contain garbage).
	for (int i = getNumInputChannels(); i < getNumOutputChannels(); ++i)
	{
		buffer.clear (i, 0, buffer.getNumSamples());
	}

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

	xml.setAttribute("smoothstrength", mSmoothStrengthFactor);
	xml.setAttribute("smoothrange", mSmoothRangeFactor);
	xml.setAttribute("smoothjagged", mSmoothJaggedFactor);
	xml.setAttribute("bits", mSampleRate); 

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
		if (i == 2)
		{
			xml.setAttribute(juce::String("release") + table, mADSRTables[i].mRelease);
		}
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
			mSmoothStrengthFactor  = (float) xmlState->getDoubleAttribute ("smoothstrength", 0.0);
			mSmoothRangeFactor  = (float) xmlState->getDoubleAttribute ("smoothrange", 0.0);
			mSmoothJaggedFactor = (float) xmlState->getDoubleAttribute("smoothjagged", 0.0);
			mSampleRate = (float) xmlState->getDoubleAttribute("bits", 1.0);
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
				mADSRTables[i].mAttack =  std::max((float)xmlState->getDoubleAttribute(juce::String("attack") + table), DECLICK);
				mADSRTables[i].mDecay = (float) xmlState->getDoubleAttribute(juce::String("decay") + table);
				mADSRTables[i].mSustain = (float) xmlState->getDoubleAttribute(juce::String("sustain") + table, 1.0);
				if (i == 2)
				{
					mADSRTables[i].mRelease = std::max((float) xmlState->getDoubleAttribute(juce::String("release") + table), DECLICK);
				}
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
