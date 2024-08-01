/**
 * Creates a simulated reference device and sets it as the root device.
 * An openDAQ server and a web-socket streaming server are started on the aplication.
 */

#include <chrono>
#include <thread>
#include <iostream>

#include <opendaq/opendaq.h>

#include "mscl/mscl.h"

using namespace daq;

mscl::Connection connection = mscl::Connection::Serial("COM4", 3000000);
mscl::BaseStation basestation(connection);


float fetch_MSCL_data()
{
    mscl::DataSweeps sweeps = basestation.getData(0, 3);

    // mscl::ChannelData temp = sweeps[0].data();

    int sweep_num = 1;
    for (mscl::DataSweep sweep : sweeps)
    {
        mscl::ChannelData data = sweep.data();

        mscl::uint64 now = sweep.timestamp().nanoseconds();
        std::cout << "new val" << std::endl << std::endl; 
        // iterate over each point in the sweep (one point per channel)
        for (mscl::WirelessDataPoint dataPoint : data)
        {
            std::cout << "channel: " << dataPoint.channelName() << std::endl;  // the name of the channel for this point
            //std::cout << "value type: " << dataPoint.storedAs() << std::endl;     // the ValueType that the data is stored as
            std::cout << "value fetched: " << dataPoint.as_float() << std::endl << std::endl;
        }
    }
    return 1; 
}

int main(int /*argc*/, const char* /*argv*/[])
{
    mscl::WirelessNode node(12345, basestation);

    mscl::PingResponse response = node.ping();

    // if the ping response was a success
    if (response.success())
    {
        response.baseRssi();  // the BaseStation RSSI
        response.nodeRssi();  // the Node RSSI
    }

    // call the setToIdle function and get the resulting SetToIdleStatus object
    mscl::SetToIdleStatus idleStatus = node.setToIdle();

    // checks if the set to idle operation has completed (successfully or with a failure)
    while (!idleStatus.complete())
    {
        std::cout << ".";
    }

    // check the result of the Set to Idle operation
    switch (idleStatus.result())
    {
        case mscl::SetToIdleStatus::setToIdleResult_success:
            std::cout << "Node is now in idle mode.";
            break;

        case mscl::SetToIdleStatus::setToIdleResult_canceled:
            std::cout << "Set to Idle was canceled!";
            break;

        case mscl::SetToIdleStatus::setToIdleResult_failed:
            std::cout << "Set to Idle has failed!";
            break;
    }

    // create a WirelessNodeConfig which is used to set all node configuration options
    mscl::WirelessNodeConfig config;

    // set the configuration options that we want to change
    //config.bootMode(mscl::WirelessTypes::bootMode_normal);
    config.inactivityTimeout(7200);
    //config.activeChannels(); 
    config.samplingMode(mscl::WirelessTypes::samplingMode_sync);
    config.sampleRate(mscl::WirelessTypes::sampleRate_128Hz);
    config.unlimitedDuration(true);

    // apply the configuration to the Node
    node.applyConfig(config);

    // create a SyncSamplingNetwork object, giving it the BaseStation that will be the master BaseStation for the network
    mscl::SyncSamplingNetwork network(basestation);

    // add a WirelessNode to the network.
    // Note: The Node must already be configured for Sync Sampling before adding to the network, or else Error_InvalidNodeConfig will be
    // thrown.
    network.addNode(node);

    network.ok();                // check if the network status is ok
    network.lossless(true);      // enable Lossless for the network
    network.percentBandwidth();  // get the total percent of bandwidth of the network

    // apply the network configuration to every node in the network
    network.applyConfiguration();

    // start all the nodes in the network sampling.
    // if (init_check == 1)
    network.startSampling();
    //node.startNonSyncSampling();

    while (1)
        fetch_MSCL_data(); 

    return 0;
}
