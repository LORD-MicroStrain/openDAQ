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
    // Create an Instance, loading modules at MODULE_PATH
    const InstancePtr instance = Instance(MODULE_PATH);

    // Add a reference device and set it as root
    //auto device = instance.addDevice("daqref://device0");
    //auto device = instance.addDevice("daq.opcua://127.0.0.1");
    auto device = instance.addDevice("daqwsda200://device0"); 

    // Add renderer function block
    FunctionBlockPtr renderer = instance.addFunctionBlock("RefFBModuleRenderer");

    // Set renderer to draw 2.5s of data
    renderer.setPropertyValue("Duration", 10.0);

    // load all available signals in Channel 0
    const auto channel = device.getChannels(); 
    const auto signals = device.getChannels()[0].getSignalsRecursive();

    // -1 to avoid last signal (its a time signal we dont want to plot)
    for(int k = 0; k < signals.getCount()-1; k++)
        renderer.getInputPorts()[k].connect(signals[k]);  

    for (;;); return 0;
}
