#include <stdio.h>
#include <iostream>
#include "TRootanaDisplay.hxx"
#include "TH1D.h"
#include "TRootanaEventLoop.hxx"
#include "TRedPitayaCanvas.hxx"
#include "THttpServer.h"


//class MyRealTimePlot : public TRootanaDisplay {

  //  TRedPitayaCanvas *hist;

//public:
  //  MyRealTimePlot() {

//		DisableRootOutput(true); // Disable default ROOT output files

    //    hist = new TH1F("hist", "Data from Red Pitaya", 10000, 0, 1500);
  //  }

//	void AddAllCanvases()
//	{
		// Set up tabbed canvases
//		AddSingleCanvas("DATA");
		
		// Choose how many events to skip before updating
//		SetNumberSkipEvent(15);

		// Choose Display name
//		SetDisplayName("Red Pitaya Data Stream");
	
//	};

//	virtual ~MyRealTimePlot() {};

//	void ResetHistograms() 
//	{
//		hist->Reset();
//	}

//	void UpdateHistograms(TDataContainer& dataContainer)
//	{
//		void *ptr;
		// Update histograms
//		int size = dataContainer.GetMidasData().LocateBank(NULL, "DATA", &ptr);
//		hist->Fill(size);
//	}

//	void PlotCanvas(TDataContainer& dataContainer)
//	{
//		if(GetDisplayWindow()->GetCurrentTabName().compare("DATA") == 0)
//		{
//			TCanvas* c1 = GetDisplayWindow()->GetCanvas("DATA");
//			c1->Clear();
//			hist->Draw();
//			c1->Modified();
//			c1->Update();
//		}
//	}


//	void QuitButtonAction();
//};

// Main class for Red Pitaya Real-Time Data Display
class MyRealTimePlot : public TRootanaDisplay {

    TRedPitayaCanvas* redPitayaCanvas; // Canvas object to handle Red Pitaya data

public:
    MyRealTimePlot() {
        // Disable default ROOT output files
        DisableRootOutput(true);

        // Set display name
        SetDisplayName("Red Pitaya Data Stream");

        // Initialize the Red Pitaya canvas
        redPitayaCanvas = new TRedPitayaCanvas();
    }

    void AddAllCanvases() override
    {
        // Add the Red Pitaya Canvas to the Rootana display
        AddSingleCanvas(redPitayaCanvas);

        // Number of events to skip before updating plots
        SetNumberSkipEvent(40);
    }

    void BeginRun(int transition, int run, int time) override {
        std::cout << "Starting run: " << run << std::endl;
        ResetCanvasHistograms();
    }

    void EndRun(int transition, int run, int time) override {
        std::cout << "Ending run: " << run << std::endl;
    }

    virtual ~MyRealTimePlot() {
        if (redPitayaCanvas) delete redPitayaCanvas;
    }

    void ResetCanvasHistograms() 
    {
        redPitayaCanvas->ResetCanvasHistograms();
    }
};

int main(int argc, char *argv[]) {
	MyRealTimePlot::CreateSingleton<MyRealTimePlot>();
    return MyRealTimePlot::Get().ExecuteLoop(argc, argv);
}
