#include "TRedPitayaCanvas.hxx"
#include "TGLabel.h"

TRedPitayaCanvas::TRedPitayaCanvas() : TCanvasHandleBase("Red Pitaya Data")
{
	derivativeHist = new TH1F("DerivateHist", "Data for Channel 0", 5000, 0 , 5000);
	derivativeHist->GetYaxis()->SetRangeUser(-1500, 1500); 
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
	derivativeHist->Reset();
}

void TRedPitayaCanvas::UpdateCanvasHistograms(TDataContainer& dataContainer)
{
	std::cout << "Available Banks: " << dataContainer.GetMidasEvent().GetBankList() << std::endl;

	std::cout << "Address of DATA bank: " << dataContainer.GetEventData<MyData>("DATA") << std::endl;


	MyData* data = dataContainer.GetEventData<MyData>("DATA");
	if (!data) 
	{
    	std::cerr << "Error: Failed to retrieve DATA bank!" << std::endl;
    	return;
	}

	std::cout << "Number of samples: " << data->GetNumSamples() << std::endl;

	if (data) 
	{
		int numSamples = data->GetNumSamples();
		for (int i = 0; i < numSamples; i++)
		{
			int sample = data->GetSample(i);
			derivativeHist-> Fill(i, sample);
		}
	}
}

void TRedPitayaCanvas::PlotCanvas(TDataContainer& dataContainer, TRootEmbeddedCanvas *embedCanvas)
{
	TCanvas *canvas = embedCanvas->GetCanvas();
	canvas->Clear();
	//int selectedChannel = fChannelSelector->GetNumberEntry()->GetIntNumber();
	
	derivativeHist-> Draw();

	canvas->Modified();
	canvas->Update();
}
