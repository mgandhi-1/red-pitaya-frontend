#include "TRedPitayaCanvas.hxx"
#include "TGLabel.h"

TRedPitayaCanvas::TRedPitayaCanvas() : TCanvasHandleBase("Red Pitaya Data")
{
	for (int i = 0; i < 32; i++)
	{
		char name[100];
		char title[100];
		sprintf(name, "derivateHist_%i", i);
		sprintf(title, "Data for Channel %i", i);
		derivativeHist[i] = new TH1F(name, title, 1000, -1500, 1500);
	}
}

void TRedPitayaCanvas::SetUpCompositeFrame(TGCompositeFrame *compFrame, TRootanaDisplay *display)
{
	TGHorizontalFrame *frame = new TGHorizontalFrame(compFrame, 200, 40);

	fChannelSelector = new TGNumberEntry(frame, 0,9,999, TGNumberFormat::kNESInteger,
										TGNumberFormat:: kNEAAnyNumber,
										TGNumberFormat:: kNELLimitMinMax,
										0,256);

	fChannelSelector->Connect("ValueSet(Long_t)", "TRootanaDisplay", display, "UpdatePlotsAction()");
	fChannelSelector->GetNumberEntry()->Connect("ReturnPressed()", "TRootanaDisplay", display, "UpdatePlotsAction()");
	frame->AddFrame(fChannelSelector, new TGLayoutHints(kLHintsTop | kLHintsLeft, 5, 5, 5,5));
	TGLabel *labelMinicrate = new TGLabel(frame, "Red Pitaya Channel" );
	frame->AddFrame(labelMinicrate, new TGLayoutHints(kLHintsTop | kLHintsLeft, 5, 5, 5,5));
	compFrame->AddFrame(frame, new TGLayoutHints(kLHintsCenterX,2,2,2,2));
}

void TRedPitayaCanvas::ResetCanvasHistograms()
{
	for (int i = 0; i < 256; i++)
	{
		derivativeHist[i]->Reset();
	}
}

void TRedPitayaCanvas::UpdateCanvasHistograms(TDataContainer& dataContainer)
{
	MyData *data = dataContainer.GetEventData<MyData>("DATA");
	if (data) 
	{
		size_t numSamples = data->GetNumSamples();
		for (size_t i = 0; i < numSamples; i++)
		{
			int sample = data->GetSample(i);
			int channel = i % 256;
			if (channel >= 0 && channel < 256)
			{
				derivativeHist[channel]->Fill(sample);
			}
		}
	}
}

void TRedPitayaCanvas::PlotCanvas(TDataContainer& dataContainer, TRootEmbeddedCanvas *embedCanvas)
{
	TCanvas *canvas = embedCanvas->GetCanvas();
	canvas->Clear();
	int selectedChannel = fChannelSelector->GetNumberEntry()->GetIntNumber();
	if (selectedChannel >= 0 && selectedChannel < 256)
	{
		derivativeHist[selectedChannel]->Draw();
	}

	canvas->Modified();
	canvas->Update();
}
