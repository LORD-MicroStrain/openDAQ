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

template <typename T>
class CircularBuffer
{
public:
    CircularBuffer(size_t size)
        : buffer(size)
        , maxSize(size)
        , head(0)
        , tail(0)
        , full(false)
    {
    }

    void add(T item)
    {
        buffer[head] = item;
        if (full)
        {
            tail = (tail + 1) % maxSize;
        }
        head = (head + 1) % maxSize;
        full = head == tail;
    }

    T get()
    {
        if (isEmpty())
        {
            return 0; 
        }
        T item = buffer[tail];
        full = false;
        tail = (tail + 1) % maxSize;
        return item;
    }

    bool isEmpty() const
    {
        return (!full && (head == tail));
    }

    bool isFull() const
    {
        return full;
    }

    void printBuffer() const
    {
        std::cout << "Buffer contents: ";
        for (size_t i = tail; i != head; i = (i + 1) % maxSize)
        {
            std::cout << buffer[i] << " ";
        }
        std::cout << std::endl;
    }

    void printReadOrder() const
    {
        std::cout << "Read order: ";
        for (size_t i = tail; i != head; i = (i + 1) % maxSize)
        {
            std::cout << buffer[i] << " ";
        }
        std::cout << std::endl;
    }

    size_t size() const
    {
        size_t size = maxSize;
        if (!full)
        {
            if (head >= tail)
            {
                size = head - tail;
            }
            else
            {
                size = maxSize + head - tail;
            }
        }
        return size;
    }

private:
    std::vector<T> buffer;
    size_t maxSize;
    size_t head;
    size_t tail;
    bool full;
};


CircularBuffer<float> x_buffer(256);
CircularBuffer<int> y_buffer(256);
CircularBuffer<int> z_buffer(256);


/* void fetch_MSCL_data()
{
    mscl::DataSweeps sweeps = basestation.getData(1000, 3);

    int sweep_num = 1;
    int cou = 0; 
    for (mscl::DataSweep sweep : sweeps)
    {
        mscl::ChannelData data = sweep.data();
        std::cout << sweep.tick() << std::endl;

        // iterate over each point in the sweep (one point per channel)
        for (mscl::WirelessDataPoint dataPoint : data)
        {
            std::cout << dataPoint.channelName() << " value fetched: " << dataPoint.as_float() << std::endl << std::endl;

            x_buffer.add(dataPoint.as_float());


            x_buffer.printBuffer();
            std::cout << "-------" << std::endl; 
            x_buffer.printReadOrder(); 
        }

    }
    cou++; 
}*/

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
    const mscl::ChannelMask channel_mask(0b111);
    config.inactivityTimeout(0xFFFF);
    config.activeChannels(channel_mask); // activate channels 1 through three using 0b111 bit mask  
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

    while (1)
        ;
        //fetch_MSCL_data(); 

       return 0;
}
