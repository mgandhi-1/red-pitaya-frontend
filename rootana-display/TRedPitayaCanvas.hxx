#ifndef TREDPITAYACANVAS_H
#define TREDPITAYACANVAS_H

#include <iostream>
#include <string>
#include <vector>

#include "TH1F.h"
#include "TCanvasHandleBase.hxx"
#include "TGenericData.hxx"
#include "TGNumberEntry.h"
#include "TDataContainer.hxx"
#include "TV792Data.hxx"

// Custom class for interpreting Red Pitaya Data in MIDAS data banks
class MyData : public TGenericData {
public: 
	MyData(int bankLen, int bankType, const char* bankName, void* ptr)
	: TGenericData(bankLen, bankType, bankName, ptr)
	{
		data = static_cast<int*>(ptr);
		num_samples = bankLen / sizeof(int);
	}

	int GetSample(size_t index) const {
		if (index < num_samples)
			return data[index];
		return -1; //Invalid index
	}

	size_t GetNumSamples() const {
		return num_samples;
	}

private:
	int* data;
	size_t num_samples;
};

class TRedPitayaCanvas : public TCanvasHandleBase {

public : 
	TRedPitayaCanvas();

	// Reset the histograms for this canvas
	void ResetCanvasHistograms() override;

	// Update the histograms for this canvas
	void UpdateCanvasHistograms(TDataContainer& dataContainer);

	// Plot the histograms for this canvas
	void PlotCanvas(TDataContainer& dataContainer, TRootEmbeddedCanvas *embedCanvas);

	// Set up the composite frame for canvas controls
	void SetUpCompositeFrame(TGCompositeFrame *compFrame, TRootanaDisplay *display);

	void BeginRun(int transition, int run, int time) override {
		std::cout << "BOR: Red Pitaya Data - Begin Run " << run << std::endl;
	}

private: 
	TH1F *derivativeHist;
	TGNumberEntry *fChannelSelector;
};

#endif
