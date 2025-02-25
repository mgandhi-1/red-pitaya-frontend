#include "TRedPitayaCanvas.hxx"
#include "TGLabel.h"
#include "TAxis.h"

TRedPitayaCanvas::TRedPitayaCanvas() : TCanvasHandleBase("Red Pitaya Data")
{
	//derivativeHist = new TH1F("DerivateHist", "Data for Channel 0", 1000, 0 ,25);
	derivativeHist = new TGraph();
	derivativeHist->SetTitle("Red Pitaya Data; Event Index; Value");
	derivativeHist->SetMarkerStyle(20);
	derivativeHist->SetMarkerColor(kBlue);
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


void TRedPitayaCanvas::UpdateCanvasHistograms(TDataContainer& dataContainer)
{
	//std::cout << "Available Banks: " << dataContainer.GetMidasEvent().GetBankList() << std::endl;

	//std::cout << "Address of bank: " << dataContainer.GetEventData<MyData>("DATA") << std::endl;

	//void *ptr = nullptr;
	//int size = dataContainer.GetMidasData().LocateBank(NULL, "DATA", &ptr);

	//static int eventIndex = 0;

	//if (size > 0) {
	//	derivativeHist -> Fill(eventIndex, size);
	//	eventIndex ++;
	//}

	
	MyData* data = dataContainer.GetEventData<MyData>("DATA");
	if (!data) 
	{
  	  	std::cerr << "Error: Failed to retrieve DATA bank!" << std::endl;
    	return ;
	}

	//std::cout << "Number of samples: " << data->GetNumSamples() << std::endl;

	if (data) 
	{
		int numSamples = data->GetNumSamples();
		for (int i = 0; i < numSamples; i++)
		{
			int sample = data->GetSample(i)/2000000;
			//printf("sample: %d\n", sample);
			int xIndex = eventIndex - xOrigin;
			derivativeHist-> SetPoint(xIndex, xIndex, sample);
			eventIndex++;

			// Reset the graph every x events
			if (eventIndex % 500 == 0)
			{
				xOrigin = eventIndex;
				derivativeHist->Set(0);
			}
		}
	}
}

void TRedPitayaCanvas::PlotCanvas(TDataContainer& dataContainer, TRootEmbeddedCanvas *embedCanvas)
{
	TCanvas *canvas = embedCanvas->GetCanvas();
	canvas->Clear();
	//int selectedChannel = fChannelSelector->GetNumberEntry()->GetIntNumber();
	derivativeHist->GetYaxis()->SetRangeUser(-1000, 1000); 
	derivativeHist-> Draw("AL");

	canvas->Modified();
	canvas->Update();
}

void TRedPitayaCanvas::ResetCanvasHistograms()
{
	derivativeHist->Set(0); // Clear all data points in the graph
	eventIndex = 0;
	xOrigin = 0;
}
