#include <stdio.h>
#include <iostream>
#include "TRootanaDisplay.hxx"
#include "TH1D.h"
#include "TRootanaEventLoop.hxx"
#include "THttpServer.h"


class MyRealTimePlot : public TRootanaDisplay {

    TH1F *hist;

public:
    MyRealTimePlot() {
	//	DisableRootOutput(true); // Disable default ROOT output files
        hist = new TH1F("hist", "Data from Red Pitaya", 6000, -8000, 8000);
    }

	void AddAllCanvases()
	{
		// Set up tabbed canvases
		AddSingleCanvas("DATA");
		
		// Choose how many events to skip before updating
		SetNumberSkipEvent(400);

		// Choose Display name
		SetDisplayName("Red Pitaya Data Stream");
	
	};

	virtual ~MyRealTimePlot() {};

	void ResetHistograms() 
	{
		hist->Reset();
	}

	void UpdateHistograms(TDataContainer& dataContainer)
	{
		void *ptr;
		// Update histograms
		int size = dataContainer.GetMidasData().LocateBank(NULL, "DATA", &ptr);
		hist->Fill(size);
	}

	void PlotCanvas(TDataContainer& dataContainer)
	{
		if(GetDisplayWindow()->GetCurrentTabName().compare("RPDA") == 0)
		{
			TCanvas* c1 = GetDisplayWindow()->GetCanvas("RPDA");
			c1->Clear();
			hist->Draw();
			c1->Modified();
			c1->Update();
		}
	}


//	void Reset();

//	void QuitButtonAction();
};

int main(int argc, char *argv[]) {
	MyRealTimePlot::CreateSingleton<MyRealTimePlot>();
    return MyRealTimePlot::Get().ExecuteLoop(argc, argv);
}
