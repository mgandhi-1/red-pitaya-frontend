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
        hist = new TH1F("hist", "Data from Red Pitaya", 6000, -50000000000000000, 50000000000000000);
    }

	void AddAllCanvases()
	{
		// Set up tabbed canvases
		AddSingleCanvas("DATA");
		
		// Choose how many events to skip before updating
		SetNumberSkipEvent(10);

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
		if(GetDisplayWindow()->GetCurrentTabName().compare("DATA") == 0)
		{
			TCanvas* c1 = GetDisplayWindow()->GetCanvas("DATA");
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
