/**
 * Adds a reference device that outputs sine waves. Its noisy output signal is averagedSine
 * by an statistics function block. Both the noisy and averaged sine waves are rendered with
 * the renderer function block for 10s.
 */

#include <chrono>
#include <thread>
#include <iostream>

#include <opendaq/opendaq.h>

using namespace daq;

int main(int /*argc*/, const char* /*argv*/[])
{

    std::cout << "......................... " << MODULE_PATH;
    // Create an Instance, loading modules at MODULE_PATH
    const InstancePtr instance = Instance(MODULE_PATH);

    // Add a reference device and set it as root
    auto device = instance.addDevice("daqref://device0");
    //auto device = instance.addDevice("daq.opcua://127.0.0.1");

    // Add statistics and renderer function block
    FunctionBlockPtr statistics = instance.addFunctionBlock("RefFBModuleStatistics");
    FunctionBlockPtr renderer = instance.addFunctionBlock("RefFBModuleRenderer");

    // Set renderer to draw 2.5s of data
    renderer.setPropertyValue("Duration", 1);

    // Get channel and signal of reference device
    //const auto x_Channel = device.getChannels()[0];
    //const auto y_Channel = device.getChannels()[0];
    //const auto z_Channel = device.getChannels()[0];

    const auto temp = device.getChannels()[0].getSignalsRecursive(); 

    //const auto x_Signal = device.getChannels()[0].getSignals()[0];
    //const auto y_Signal = device.getChannels()[0].getSignals()[1];
    //const auto z_Signal = device.getChannels()[0].getSignals()[2];
    //const auto y_Signal = y_Channel.getSignals()[1];
    //const auto z_Signal = z_Channel.getSignals()[2];

/*
    const auto sineChannel = device.getChannels()[0];
    const auto sineSignal = sineChannel.getSignals()[0];
    // Add noise to the sine wave
    sineChannel.setPropertyValue("NoiseAmplitude", 1);

    // Connect the signals to the renderer and statistics
    statistics.getInputPorts()[0].connect(sineSignal);
    const auto averagedSine = statistics.getSignalsRecursive()[0];
*/
    const auto input_Temp = renderer.getInputPorts(); 

    renderer.getInputPorts()[0].connect(temp[0]);  
    renderer.getInputPorts()[1].connect(temp[1]);
    renderer.getInputPorts()[2].connect(temp[2]);

    // Process and render data for 10s, modulating the amplitude
    //double ampl_step = 0.1;
    for (;;)
    {
        //std::this_thread::sleep_for(std::chrono::milliseconds(25));
        //const double ampl = sineChannel.getPropertyValue("Amplitude");
        //if (9.95 < ampl || ampl < 3.05)
        //    ampl_step *= -1;
        //sineChannel.setPropertyValue("Amplitude", ampl + ampl_step);
    }

    return 0;
}
