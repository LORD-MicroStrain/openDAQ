/**
 * Creates a simulated reference device and sets it as the root device.
 * An openDAQ server and a web-socket streaming server are started on the aplication.
 */

#include <iostream>
#include <opendaq/opendaq.h>

using namespace daq;

int main(int /*argc*/, const char* /*argv*/[])
{
    // Create an openDAQ instance, loading modules at MODULE_PATH
    const InstancePtr instance = Instance(MODULE_PATH);

    // Add a reference device as root device
    //instance.setRootDevice("daqref://device0");
    instance.setRootDevice("daqwsda://device0");

    // Start streaming and openDAQ OpcUa servers
    // Jeff's Note: this does everything the code below does
    //instance.addStandardServers();

    // Creates and registers a Server capability with the ID `opendaq_lt_streaming` and the default port number 7414
    instance.addServer("openDAQ LT Streaming", nullptr);

    // Creates and registers a Server capability with the ID `opendaq_native_streaming` and the default port number 7420
    instance.addServer("openDAQ Native Streaming", nullptr);

    // As the Streaming servers were added first, the registered Server capabilities are published over OPC UA
    instance.addServer("openDAQ OpcUa", nullptr);

    while (1)
    {
    }

    return 0;
}
