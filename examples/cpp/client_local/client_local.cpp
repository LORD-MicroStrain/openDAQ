/**
 * Creates a simulated reference device and sets it as the root device.
 * An openDAQ server and a web-socket streaming server are started on the aplication.
 */

#include <chrono>
#include <iostream>
#include <thread>

#include <opendaq/opendaq.h>

using namespace daq;

int main(int /*argc*/, const char* /*argv*/[])
{
    // Create an Instance, loading modules at MODULE_PATH
    const InstancePtr instance = Instance(MODULE_PATH);

    // Add a reference device and set it as root
    //auto device = instance.addDevice("daqref://device0");
    auto device = instance.addDevice("daq.opcua://127.0.0.1");

    // Add statistics and renderer function block
    FunctionBlockPtr statistics = instance.addFunctionBlock("RefFBModuleStatistics");
    FunctionBlockPtr renderer = instance.addFunctionBlock("RefFBModuleRenderer");

    // Set renderer to draw 2.5s of data
    renderer.setPropertyValue("Duration", 2.5);

    // Get channel and signal of reference device
    const auto sineChannel = device.getChannels()[0];
    const auto sineSignal = sineChannel.getSignals()[0];

    // Add noise to the sine wave
    sineChannel.setPropertyValue("NoiseAmplitude", 1);

    // Connect the signals to the renderer and statistics
    statistics.getInputPorts()[0].connect(sineSignal);
    const auto averagedSine = statistics.getSignalsRecursive()[0];

    renderer.getInputPorts()[0].connect(sineSignal);
    renderer.getInputPorts()[1].connect(averagedSine);

    // Process and render data for 10s, modulating the amplitude
    
    for (;;)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        //const double ampl = sineChannel.getPropertyValue("Amplitude");
        //if (9.95 < ampl || ampl < 3.05)
        //    ampl_step *= -1;
        //sineChannel.setPropertyValue("Amplitude", ampl + ampl_step);
    }

    return 0;
}
