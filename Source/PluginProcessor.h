/*
   ==============================================================================

   This file was auto-generated by the Jucer!

   It contains the basic startup code for a Juce application.

   ==============================================================================
 */

#ifndef __PLUGINPROCESSOR_H_526ED7A9__
#define __PLUGINPROCESSOR_H_526ED7A9__

#include "../JuceLibraryCode/JuceHeader.h"

#define WAVESIZE 200
#define WAVEHEIGHT 176.f
#define TABLESPACING 8.f

#define WINDOWWIDTH 720
#define WINDOWHEIGHT 520

#define LASTRADIOACTION Action::UserFunc

enum Action
{
	SinFunc = 0,
	CosFunc,
	SawFunc,
	SquareFunc,
	TriFunc,
	UserFunc,
	Reverse,
	Flip,
	MeanFilter,
	AdjustPhase,
};

#define LASTCOMMONPARAM AmpReleaseParam
#define LASTSYNTHPARAM SynthReleaseParam
#define TOTALNUMPARAMS (LastParam-LASTCOMMONPARAM+LASTSYNTHPARAM)
#define SYNTHPARAMS (LASTSYNTHPARAM-LASTCOMMONPARAM)

enum Parameter
{
	SourceParam = 0,
	SmoothStrengthParam,
	SmoothRangeParam,
	AmpAttackParam,
	AmpDecayParam,
	AmpSustainParam,
	AmpReleaseParam,

	AdjustPhaseParam,
	SynthAttackParam,
	SynthDecayParam,
	SynthSustainParam,
	SynthReleaseParam,

	LastParam,
	NoneParam
};

juce::String convertActionToString(int act);
String getParameterName (int index);

class ADSRTable
{
public:
	ADSRTable()
	{
		mAttack = 0.0;
		mDecay = 0.0;
		mSustain = 1.0;
		mRelease = 0.0;
	}
	float mAttack, mDecay, mSustain, mRelease;
};


class WaveTable
{
public:
	WaveTable()
	{
		for (int i = 0; i < WAVESIZE; i++)
		{
			mWave[i] = 0;
		}
	}
	void setWaveSection(int x, float y)
	{
		if (0 <= x && x < WAVESIZE)
		{
			mWave[x] = std::max(std::min(y, 1.f), -1.f);
		}
	}
	void executeAction(size_t ac, int param = 0)
	{ executeAction((Action) ac, param); }
	void executeAction(Action ac, int param = 0)
	{
		if (ac == SinFunc)
		{
			for (int i = 0; i < WAVESIZE; i++)
			{
				mWave[i] = (float) sin((i*2*double_Pi)/WAVESIZE);
			}
		} else if (ac == CosFunc) 
		{
			for (int i = 0; i < WAVESIZE; i++)
			{
				mWave[i] = (float) cos((i*2*double_Pi)/WAVESIZE);
			}
		}  else if (ac == SawFunc)
		{
			for (int i = 0; i < WAVESIZE; i++)
			{
				mWave[i] = (float) (WAVESIZE/2-i)/(WAVESIZE/2);
			}
		} else if (ac == SquareFunc)
		{
			for (int i = 0; i < WAVESIZE; i++)
			{
				mWave[i] = (float) (1-2*(i*2/WAVESIZE));
			}
		} else if (ac == TriFunc)
		{
			for (int i = 0; i < WAVESIZE; i++)
			{
				int imod = i % (WAVESIZE/2);
				mWave[i] = (1-2*(i > WAVESIZE/2)) * std::abs(imod - (imod > (WAVESIZE/4))*(imod-WAVESIZE/4)*2) / ((float) (WAVESIZE/4));
			}
		} else if (ac == Reverse)
		{
			for (int i = 0; i < WAVESIZE/2; i++)
			{
				float tmp = mWave[WAVESIZE-i-1];
				mWave[WAVESIZE-i-1] = mWave[i];
				mWave[i] = tmp;
			}
		} else if (ac == Flip)
		{
			for (int i = 0; i < WAVESIZE; i++)
			{
				mWave[i] = -mWave[i];
			}
		} else if (ac == MeanFilter)
		{
			// Filtering with no neighbour range is meaningless.
			if (param < 0)
			{
				param = 1;
			}

			// Copy old wave
			float oldWave[WAVESIZE];
			for (int i = 0; i < WAVESIZE; i++)
			{
				oldWave[i] = mWave[i];
				mWave[i] = 0;
			}

			// Calculate new wave
			int range = param*2+1;
			for (int i = 0; i < WAVESIZE; i++)
			{
				for (int j = 0; j < range; j++)
				{
					int from = (i-param+j) % WAVESIZE;
					if (from < 0)
					{
						from += WAVESIZE;
					}
					mWave[i] += oldWave[from];
				}
				mWave[i] /= range;
			}
		} else if (ac == AdjustPhase)
		{
			if (param == 0)
			{
				return;
			}
			float oldWave[WAVESIZE];
			for (int i = 0; i < WAVESIZE; i++)
			{
				oldWave[i] = mWave[i];
			}
			for (int i = 0; i < WAVESIZE; i++)
			{
				int target = (i + param)%WAVESIZE;
				if (target < 0)
				{
					target += WAVESIZE;
				}
				mWave[i] = oldWave[target];
			}
		}
	}

	float mWave[WAVESIZE];
};


//==============================================================================
/**
 */
class SimpleMorphSynth  : public AudioProcessor
{
public:
	//==============================================================================
	SimpleMorphSynth();
	~SimpleMorphSynth();

	//==============================================================================
	void prepareToPlay (double sampleRate, int samplesPerBlock);
	void releaseResources();
	void processBlock (AudioSampleBuffer& buffer, MidiBuffer& midiMessages);
	void reset();

	//==============================================================================
	bool hasEditor() const				 
	{ return true; }
	AudioProcessorEditor* createEditor();

	//==============================================================================
	const String getName() const			{ return JucePlugin_Name; }

	int getNumParameters();
	float getParameter (int index);
	void setParameter (int index, float newValue);
	const String getParameterName (int index);
	const String getParameterText (int index);

	const String getInputChannelName (int channelIndex) const;
	const String getOutputChannelName (int channelIndex) const;
	bool isInputChannelStereoPair (int index) const;
	bool isOutputChannelStereoPair (int index) const;

	bool acceptsMidi() const;
	bool producesMidi() const;
	bool silenceInProducesSilenceOut() const;
	double getTailLengthSeconds() const;

	//==============================================================================
	int getNumPrograms()													{ return 0; }
	int getCurrentProgram()													{ return 0; }
	void setCurrentProgram (int /*index*/)									{ }
	const String getProgramName (int /*index*/)								{ return String::empty; }
	void changeProgramName (int /*index*/, const String& /*newName*/)		{ }
	void setWaveTableValue(size_t table, int pos, float value);
	WaveTable *getWaveTable(size_t table);
	size_t getNumTables()
	{ return mWaveTables.size(); }

	float getWaveTableValue(size_t table, int pos);
	float getWaveValue(float pos);

	//==============================================================================
	void getStateInformation (MemoryBlock& destData);
	void setStateInformation (const void* data, int sizeInBytes);

	//==============================================================================
	// These properties are public so that our editor component can access them
	// A bit of a hacky way to do it, but it's only a demo! Obviously in your own
	// code you'll do this much more neatly..

	// this is kept up to date with the midi messages that arrive, and the UI component
	// registers with it so it can represent the incoming messages
	MidiKeyboardState mKeyboardState;

	// this keeps a copy of the last set of time info that was acquired during an audio
	// callback - the UI component will read this and display it.
	AudioPlayHead::CurrentPositionInfo mLastPosInfo;

	// these are used to persist the UI's size - the values are stored along with the
	// filter's other parameters, and the UI component will update them when it gets
	// resized.
	int mLastUIWidth, mLastUIHeight;
	bool mUpdateEditor;

	//==============================================================================

	std::vector<WaveTable *> mWaveTables;
	std::vector<ADSRTable *> mADSRTables;
	float mSourceFactor, mSmoothStrengthFactor, mSmoothRangeFactor;

private:
	//==============================================================================
	AudioSampleBuffer mDelayBuffer;
	int mDelayPosition;

	Synthesiser mSynth;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SimpleMorphSynth)
};

#endif  // __PLUGINPROCESSOR_H_526ED7A9__
