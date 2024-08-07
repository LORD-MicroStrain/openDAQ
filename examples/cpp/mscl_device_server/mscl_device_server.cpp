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

//mscl::Connection connection = mscl::Connection::Serial("COM12", 3000000);
//mscl::BaseStation basestation(connection);

float x_buffer[5]; 
int y_buffer[5]; 
int z_buffer[5]; 

int x_read = 0;
int x_write = 0; 


bool x_first_pass = 1;

void x_add(float element)
{
    if (x_first_pass)
    {
        x_buffer[x_write] = element;

        if (x_write == 4)
            x_write = 0;
        else
            x_write++;

        x_first_pass = 0; 
    }
    else
    {
        if (x_read == x_write)
        {
            x_buffer[x_write] = element;

            if (x_write == 4)
                x_write = 0;
            else
                x_write++;

            if (x_read == 4)
                x_read = 0;
            else
                x_read++;
        }
        else
        {
            x_buffer[x_write] = element;

            if (x_write == 4)
                x_write = 0;
            else
                x_write++;
        }
    }
}

float x_release()
{
    float temp = x_buffer[x_read];

    if (x_read == 4)
        x_read = 0;
    else
        x_read++;

    return temp; 
}

float cou = 0; 

void fetch_MSCL_data()
{
    mscl::DataSweeps sweeps = basestation.getData(1000, 3);

    int sweep_num = 1;
    for (mscl::DataSweep sweep : sweeps)
    {
        mscl::ChannelData data = sweep.data();
        std::cout << sweep.tick() << std::endl;

        // iterate over each point in the sweep (one point per channel)
        for (mscl::WirelessDataPoint dataPoint : data)
        {
            std::cout << dataPoint.channelName() << " value fetched: " << dataPoint.as_float() << std::endl << std::endl;

            x_add(cou++);

            std::cout << "buffer: " << std::endl;


            for (float x : x_buffer)
                std::cout << x << std::endl;

            std::cout << "-----" << std::endl;

            std::cout << "read order: " << std::endl;

            for (int k = 0; k < 5; k++)
                std::cout << x_release() << std::endl;

            std::cout << "-----" << std::endl;
        }
    }
}

int main(int /*argc*/, const char* /*argv*/[])
{
    /*
    mscl::WirelessNode node(node_id, basestation);

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
    config.inactivityTimeout(0xFFFF);
    config.activeChannels(mscl::ChannelMask::ChannelMask(0b111)); // activate channels 1 through three using 0b111 bit mask  
    config.samplingMode(mscl::WirelessTypes::samplingMode_sync);
    config.sampleRate(mscl::WirelessTypes::sampleRate_1Hz);
    config.unlimitedDuration(true);

    // apply the configuration to the Node
    node.applyConfig(config);

    // create a SyncSamplingNetwork object, giving it the BaseStation that will be the master BaseStation for the network
    mscl::SyncSamplingNetwork network(basestation);

    // add a WirelessNode to the network.
    // Note: The Node must already be configured for Sync Sampling before adding to the network, or else Error_InvalidNodeConfig will be
    // thrown.
    network.addNode(node);
    network.lossless(true);      // enable Lossless for the network
    network.percentBandwidth();  // get the total percent of bandwidth of the network

    // apply the network configuration to every node in the network
    network.applyConfiguration();

    // start all the nodes in the network sampling.
    // if (init_check == 1)
    network.startSampling();
    //node.startNonSyncSampling();

    //while (1)
    //    fetch_MSCL_data(); 
*/
    // Create an Instance, loading modules at MODULE_PATH
    const InstancePtr instance = Instance(MODULE_PATH);

    std::cout << " -------------------------------------------------------------------- " << MODULE_PATH;

    // Add a reference device and set it as root
    auto device = instance.addDevice("daqref://device0");
    //auto device = instance.addDevice("daq.opcua://127.0.0.1");

    // Add statistics and renderer function block
    FunctionBlockPtr statistics = instance.addFunctionBlock("ref_fb_module_statistics");
    FunctionBlockPtr renderer = instance.addFunctionBlock("ref_fb_module_renderer");

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
    double ampl_step = 0.1;
    for (int i = 0; i < 400; ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
        const double ampl = sineChannel.getPropertyValue("Amplitude");
        if (9.95 < ampl || ampl < 3.05)
            ampl_step *= -1;
        sineChannel.setPropertyValue("Amplitude", ampl + ampl_step);
    }


    return 0;
}
