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

    std::cout << " " << MODULE_PATH;

    // Add a reference device as root device
    instance.setRootDevice("daqref://device0");
    
    // Start streaming and openDAQ OpcUa servers 
    //instance.addStandardServers();


    const auto servers = instance.addStandardServers();

    for (const auto& server : servers)
    {
        server.enableDiscovery();
        std::cout << server.getId() << std::endl << std::endl;
    }

    std::cout << "Press \"enter\" to exit the application..." << std::endl;
    std::cin.get();
    return 0;
}
