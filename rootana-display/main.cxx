#include <stdio.h>
#include <iostream>
#include "TRootanaDisplay.hxx"
#include "TH1D.h"
#include "TRootanaEventLoop.hxx"
#include "TRedPitayaCanvas.hxx"
#include "THttpServer.h"

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
        SetNumberSkipEvent(5);
    }

    void BeginRun(int transition, int run, int time) override {
        std::cout << "Starting run: " << run << std::endl;
        //ResetCanvasHistograms();
    }

    void EndRun(int transition, int run, int time) override {
        std::cout << "Ending run: " << run << std::endl;
    }

    virtual ~MyRealTimePlot() {
        if (redPitayaCanvas) delete redPitayaCanvas;
    }

    //void ResetCanvasHistograms() 
    //{
    //    redPitayaCanvas->ResetCanvasHistograms();
    //}
};

int main(int argc, char *argv[]) {
	MyRealTimePlot::CreateSingleton<MyRealTimePlot>();
    return MyRealTimePlot::Get().ExecuteLoop(argc, argv);
}
