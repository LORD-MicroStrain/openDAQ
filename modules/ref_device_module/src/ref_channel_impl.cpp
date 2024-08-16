#include <ref_device_module/ref_channel_impl.h>
#include <coreobjects/eval_value_factory.h>
#include <coretypes/procedure_factory.h>
#include <opendaq/signal_factory.h>
#include <coreobjects/coercer_factory.h>
#include <opendaq/range_factory.h>
#include <opendaq/packet_factory.h>
#include <fmt/format.h>
#include <coreobjects/callable_info_factory.h>
#include <opendaq/data_rule_factory.h>
#include <coreobjects/unit_factory.h>
#include <opendaq/scaling_factory.h>
#include <opendaq/custom_log.h>
#include <coreobjects/property_object_protected_ptr.h>
#include <iostream>
#include <chrono>
#include <vector>
#include <thread>
#include <ctime>
#include "mscl/mscl.h"

#define PI 3.141592653589793

BEGIN_NAMESPACE_REF_DEVICE_MODULE

/// FOR FLOATS
//template <typename T>
//class CircularBuffer
//{
//public:
//    CircularBuffer(size_t size)
//        : buffer(size)
//        , maxSize(size)
//        , head(0)
//        , tail(0)
//        , full(false)
//    {
//    }
//
//    void add(T item)
//    {
//        buffer[head] = item;
//        if (full)
//        {
//            tail = (tail + 1) % maxSize;
//        }
//        head = (head + 1) % maxSize;
//        full = head == tail;
//    }
//
//    T get()
//    {
//        if (isEmpty())
//        {
//            return 0;
//        }
//        T item = buffer[tail];
//        full = false;
//        tail = (tail + 1) % maxSize;
//        return item;
//    }
//
//    bool isEmpty() const
//    {
//        return (!full && (head == tail));
//    }
//
//    void printBuffer() const
//    {
//        std::cout << "Buffer contents: ";
//        for (size_t i = tail; i != head; i = (i + 1) % maxSize)
//        {
//            std::cout << buffer[i] << " ";
//        }
//        std::cout << std::endl;
//    }
//
//    void printReadOrder() const
//    {
//        std::cout << "Read order: ";
//        for (size_t i = tail; i != head; i = (i + 1) % maxSize)
//        {
//            std::cout << buffer[i] << " ";
//        }
//        std::cout << std::endl;
//    }
//
//    bool isFull() const
//    {
//        return full;
//    }
//
//    size_t size() const
//    {
//        size_t size = maxSize;
//        if (!full)
//        {
//            if (head >= tail)
//            {
//                size = head - tail;
//            }
//            else
//            {
//                size = maxSize + head - tail;
//            }
//        }
//        return size;
//    }
//
//private:
//    std::vector<T> buffer;
//    size_t maxSize;
//    size_t head;
//    size_t tail;
//    bool full;
//};


/// <summary> for MSCL sweeps
/// 
/// </summary>
/// <typeparam name="T"></typeparam>
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

    uint64_t get_time()
    {
        return buffer[tail].timestamp().nanoseconds() / 1000; 
    }

    T get()
    {
        if (isEmpty())
        {
            return buffer[tail];  // total hack for supporting sweeps
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

    bool isFull() const
    {
        return full;
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
/*
int main()
{
    CircularBuffer<int> cb(5);

    cb.add(1);
    cb.add(2);
    cb.add(3);
    cb.add(4);
    cb.add(5);

    std::cout << "Buffer contents: ";
    while (!cb.isEmpty())
    {
        std::cout << cb.get() << " ";
    }
    std::cout << std::endl;

    return 0;
}*/
mscl::Connection connection = mscl::Connection::Serial("COM4", 3000000);
mscl::BaseStation basestation(connection);
int node_id = 12345;
 //int node_id = 5;
//int node_id = 40415;

// CircularBuffer<float> x_buffer(128);
// CircularBuffer<float> y_buffer(128);
// CircularBuffer<float> z_buffer(128);
CircularBuffer<mscl::DataSweep> sweep_buffer(128);

int num_sweeps = 0;
bool data_flowing = false;

std::mutex load_buffer_lock;

RefChannelImpl::RefChannelImpl(const ContextPtr& context, const ComponentPtr& parent, const StringPtr& localId, const RefChannelInit& init)
    : ChannelImpl(FunctionBlockType("RefChannel",  fmt::format("AI{}", init.index + 1), ""), context, parent, localId)
    , waveformType(WaveformType::None)
    , freq(0)
    , ampl(0)
    , dc(0)
    , noiseAmpl(0)
    , constantValue(0)
    , sampleRate(0)
    , index(init.index)
    , globalSampleRate(init.globalSampleRate)
    , counter(0)
    , startTime(init.startTime)
    , microSecondsFromEpochToStartTime(init.microSecondsFromEpochToStartTime)
    , lastCollectTime(0)
    , samplesGenerated(0)
    , re(std::random_device()())
    , needsSignalTypeChanged(false)
{


    initProperties();
    waveformChangedInternal();
    signalTypeChangedInternal();
    packetSizeChangedInternal();
    resetCounter();
    createSignals();
    buildSignalDescriptors();

    initMSCL(0);
}


void RefChannelImpl::initMSCL(uint8_t section)
{
    if (1)
    {
        // create the connection object with port and baud rate
        //mscl::Connection connection = mscl::Connection::Serial("COM4", 3000000);

        // create the BaseStation, passing in the connection
       // mscl::BaseStation basestation(connection);

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
        //config.inactivityTimeout(7200);
        config.activeChannels(mscl::ChannelMask::ChannelMask(0b111)); 
        config.samplingMode(mscl::WirelessTypes::samplingMode_sync);
        config.sampleRate(mscl::WirelessTypes::sampleRate_512Hz);
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
        //if (init_check == 1)
            network.startSampling();
        //node.startNonSyncSampling();

    }
}


void RefChannelImpl::fetch_MSCL_data()
{
    load_buffer_lock.lock();

    mscl::DataSweeps sweeps = basestation.getData(1000, 0);


        //auto now = sweep_buffer.get_time();

        //std::cout << "fetch mscl time: " << now << std::endl;
        //std::cout << "time since last fetch mscl stamp: " << now - then << std::endl; 

        //then = now;

    for (mscl::DataSweep sweep : sweeps)
    {
        //mscl::ChannelData data = sweep.data();

        //x_buffer.add(data[0].as_float()); 
        //y_buffer.add(data[1].as_float()); 
        //z_buffer.add(data[2].as_float()); 

        if (sweep.data().size() < 4)  //avoids loading in diagnostic packet 
            sweep_buffer.add(sweep);

    } 
    load_buffer_lock.unlock();
}

void RefChannelImpl::signalTypeChangedIfNotUpdating(const PropertyValueEventArgsPtr& args)
{
    if (!args.getIsUpdating())
        signalTypeChanged();
    else
        needsSignalTypeChanged = true;
}

void RefChannelImpl::initProperties()
{
    const auto waveformProp = SelectionProperty("Waveform", List<IString>("Sine", "Rect", "None", "Counter", "Constant"), 0);
    
    objPtr.addProperty(waveformProp);
    objPtr.getOnPropertyValueWrite("Waveform") +=
        [this](PropertyObjectPtr& obj, PropertyValueEventArgsPtr& args) { signalTypeChangedIfNotUpdating(args); };

    const auto freqProp = FloatPropertyBuilder("Frequency", 10.0)
                              .setVisible(EvalValue("$Waveform < 2"))
                              .setUnit(Unit("Hz"))
                              .setMinValue(0.1)
                              .setMaxValue(10000.0)
                              .build();
    
    objPtr.addProperty(freqProp);
    objPtr.getOnPropertyValueWrite("Frequency") +=
        [this](PropertyObjectPtr& obj, PropertyValueEventArgsPtr& args) { waveformChanged(); };

    const auto dcProp =
        FloatPropertyBuilder("DC", 0.0).setVisible(EvalValue("$Waveform < 3")).setUnit(Unit("V")).setMaxValue(10.0).setMinValue(-10.0).build();
    

    objPtr.addProperty(dcProp);
    objPtr.getOnPropertyValueWrite("DC") += [this](PropertyObjectPtr& obj, PropertyValueEventArgsPtr& args) { waveformChanged(); };

    const auto amplProp =
        FloatPropertyBuilder("Amplitude", 5.0).setVisible(EvalValue("$Waveform < 2")).setUnit(Unit("V")).setMaxValue(10.0).setMinValue(0.0).build();

    objPtr.addProperty(amplProp);
    objPtr.getOnPropertyValueWrite("Amplitude") +=
        [this](PropertyObjectPtr& obj, PropertyValueEventArgsPtr& args) { waveformChanged(); };

    const auto noiseAmplProp = FloatPropertyBuilder("NoiseAmplitude", 0.0)
                                   .setUnit(Unit("V"))
                                   .setVisible(EvalValue("$Waveform < 3"))
                                   .setMinValue(0.0)
                                   .setMaxValue(10.0)
                                   .build();
    
    objPtr.addProperty(noiseAmplProp);
    objPtr.getOnPropertyValueWrite("NoiseAmplitude") +=
        [this](PropertyObjectPtr& obj, PropertyValueEventArgsPtr& args) { waveformChanged(); };

    const auto useGlobalSampleRateProp = BoolProperty("UseGlobalSampleRate", True);

    objPtr.addProperty(useGlobalSampleRateProp);
    objPtr.getOnPropertyValueWrite("UseGlobalSampleRate") +=
        [this](PropertyObjectPtr& obj, PropertyValueEventArgsPtr& args) { signalTypeChangedIfNotUpdating(args); };

    const auto sampleRateProp = FloatPropertyBuilder("SampleRate", 10.0) //Peter
                                    .setVisible(EvalValue("!$UseGlobalSampleRate"))
                                    .setUnit(Unit("Hz"))
                                    .setMinValue(1.0)
                                    .setMaxValue(1000000.0)
                                    .setSuggestedValues(List<Float>(10.0, 100.0, 1000.0, 10000.0, 100000.0, 1000000.0))
                                    .build();

    objPtr.addProperty(sampleRateProp);
    objPtr.getOnPropertyValueWrite("SampleRate") += [this](PropertyObjectPtr& obj, PropertyValueEventArgsPtr& args)
    {
        if (args.getPropertyEventType() == PropertyEventType::Update)
        {
            double sr = args.getValue();
            auto coercedSr = this->coerceSampleRate(sr);
            if (coercedSr != sr)
                args.setValue(coercedSr);
        }

        signalTypeChangedIfNotUpdating(args);
    };

    objPtr.getOnPropertyValueRead("SampleRate") += [this](PropertyObjectPtr& obj, PropertyValueEventArgsPtr& args)
    {
        if (objPtr.getPropertyValue("UseGlobalSampleRate"))
        {
            args.setValue(globalSampleRate);
        }
    };

    const auto resetCounterProp =
        FunctionProperty("ResetCounter", ProcedureInfo(), EvalValue("$Waveform == 3"));
    objPtr.addProperty(resetCounterProp);
    objPtr.setPropertyValue("ResetCounter", Procedure([this] { this->resetCounter(); }));

    const auto clientSideScalingProp = BoolProperty("ClientSideScaling", False);

    objPtr.addProperty(clientSideScalingProp);
    objPtr.getOnPropertyValueWrite("ClientSideScaling") += [this](PropertyObjectPtr& obj, PropertyValueEventArgsPtr& args) { signalTypeChangedIfNotUpdating(args); };

    const auto testReadOnlyProp = BoolPropertyBuilder("TestReadOnly", False).setReadOnly(True).build();
    objPtr.addProperty(testReadOnlyProp);
    objPtr.asPtr<IPropertyObjectProtected>().setProtectedPropertyValue("TestReadOnly", True);

    const auto defaultCustomRange = Range(-10.0, 10.0);
    objPtr.addProperty(StructPropertyBuilder("CustomRange", defaultCustomRange).build());
    objPtr.getOnPropertyValueWrite("CustomRange") +=
        [this](PropertyObjectPtr& obj, PropertyValueEventArgsPtr& args) { signalTypeChangedIfNotUpdating(args); };

    objPtr.addProperty(BoolPropertyBuilder("FixedPacketSize", False).build());
    objPtr.getOnPropertyValueWrite("FixedPacketSize") +=
        [this](PropertyObjectPtr& obj, PropertyValueEventArgsPtr& args) { packetSizeChanged(); };

    objPtr.addProperty(IntPropertyBuilder("PacketSize", 1000).setVisible(EvalValue("$FixedPacketSize")).build());
    objPtr.getOnPropertyValueWrite("PacketSize") +=
        [this](PropertyObjectPtr& obj, PropertyValueEventArgsPtr& args) { packetSizeChanged(); };

    const auto constantValueProp =
        FloatPropertyBuilder("ConstantValue", 2.0)
            .setVisible(EvalValue("$Waveform == 4"))
            .setMinValue(-10.0)
            .setMaxValue(10.0)
            .build();

    objPtr.addProperty(constantValueProp);
    objPtr.getOnPropertyValueWrite("ConstantValue") +=
        [this](PropertyObjectPtr& obj, PropertyValueEventArgsPtr& args) { waveformChanged(); };
}

void RefChannelImpl::packetSizeChangedInternal()
{
    fixedPacketSize = objPtr.getPropertyValue("FixedPacketSize");
    packetSize = objPtr.getPropertyValue("PacketSize");
}

void RefChannelImpl::packetSizeChanged()
{
    std::scoped_lock lock(sync);

    packetSizeChangedInternal();
}

void RefChannelImpl::waveformChangedInternal()
{
    waveformType = objPtr.getPropertyValue("Waveform");
    freq = objPtr.getPropertyValue("Frequency");
    dc = objPtr.getPropertyValue("DC");
    ampl = objPtr.getPropertyValue("Amplitude");
    noiseAmpl = objPtr.getPropertyValue("NoiseAmplitude");
    constantValue = objPtr.getPropertyValue("ConstantValue");
    LOG_I("Properties: Waveform {}, Frequency {}, DC {}, Amplitude {}, NoiseAmplitude {}, ConstantValue {}",
          objPtr.getPropertySelectionValue("Waveform").toString(), freq, dc, ampl, noiseAmpl, constantValue);
}

void RefChannelImpl::updateSamplesGenerated()
{
    if (lastCollectTime.count() > 0)
        samplesGenerated = getSamplesSinceStart(lastCollectTime);
}

void RefChannelImpl::waveformChanged()
{
    std::scoped_lock lock(sync);
    waveformChangedInternal();
}

void RefChannelImpl::signalTypeChanged()
{
    std::scoped_lock lock(sync);
    signalTypeChangedInternal();
    buildSignalDescriptors();
    updateSamplesGenerated();
}

void RefChannelImpl::signalTypeChangedInternal()
{
    // TODO: Should global sample rate be coerced? We only coerce it on read now.
    if (objPtr.getPropertyValue("UseGlobalSampleRate"))
        sampleRate = globalSampleRate;
    else
        sampleRate = objPtr.getPropertyValue("SampleRate");
    clientSideScaling = objPtr.getPropertyValue("ClientSideScaling");

    customRange = objPtr.getPropertyValue("CustomRange");

    waveformType = objPtr.getPropertyValue("Waveform");

    sampleRate = 512;  // PETER heres where sample rate is established

    LOG_I("Properties: SampleRate {}, ClientSideScaling {}", sampleRate, clientSideScaling);
}

void RefChannelImpl::resetCounter()
{
    std::scoped_lock lock(sync);
    counter = 0;
}

uint64_t RefChannelImpl::getSamplesSinceStart(std::chrono::microseconds time) const
{
    const uint64_t samplesSinceStart = static_cast<uint64_t>(std::trunc(static_cast<double>((time - startTime).count()) / 1000000.0 * sampleRate));
    return samplesSinceStart;
}

//void RefChannelImpl::collectSamples(std::chrono::microseconds curTime)
//{
//    std::scoped_lock lock(sync);
//    const uint64_t samplesSinceStart = getSamplesSinceStart(curTime);
//
//
//
//    mutex_buffer.lock();
//
//    //int buffer_size = sweep_buffer.size();
//    int buffer_size = basestation.totalData();
//
//
//                //const auto packetTime = samplesGenerated * deltaT + static_cast<uint64_t>(microSecondsFromEpochToStartTime.count());
//    int64_t msTime = sweep_buffer.get_time();
//
//
//                //std::cout << packetTime << "\n";
//                //std::cout << sweep_buffer.get_time() / 1000 << " ----" << "\n";
//
//                //auto [x_packet, y_packet, z_packet, domainPacket] = generateSamples(sweep_buffer.get_time(), samplesGenerated, newSamples);
//    if (buffer_size > 0)
//    {
//        auto [x_packet, y_packet, z_packet, domainPacket] = generateSamples(msTime, samplesGenerated, buffer_size);
//
//        ////auto now = packetTime;
//        //uint64_t now = sweep_buffer.get_time();
//        //std::cout << "time: " << now << std::endl;
//        //int delta = now -then_s;
//
//        x_signal.sendPacket(std::move(x_packet));
//        y_signal.sendPacket(std::move(y_packet));
//        z_signal.sendPacket(std::move(z_packet));
//        timeSignal.sendPacket(std::move(domainPacket));
//    }
//                //timeSignal.sendPacket(std::move(DataPacket(timeSignal.getDescriptor(), newSamples, sweep_buffer.get_time())));
//
//            samplesGenerated += buffer_size;
//
//    lastCollectTime = curTime;
//
//    mutex_buffer.unlock(); 
//}

std::mutex mutex_buffer;

int then = 0; 
void RefChannelImpl::collectSamples(std::chrono::microseconds curTime)
{
    mutex_buffer.lock();

    mscl::DataSweeps sweeps = basestation.getData(20, 0);
    int sweep_size = sweeps.size();

    if (sweep_size > 0)
    {
        uint64_t sweep_time = sweeps.data()[0].timestamp().nanoseconds() / 1000;
        auto temp_time = sweeps[0].data()[0];
        auto temp_time_1 = sweeps.data()[0];



        uint64_t now = sweep_time;
       /* std::cout << "current time: " << now << std::endl;
        int diff = now - then;

        if (diff < 0)
           std::cout << "" << std::endl;

        std::cout << "delta: " << diff << std::endl;
        then = now;*/
        std::cout << "current time: " << now << std::endl;
        std::cout << "size: " << sweep_size << std::endl;
        int diff = now - then;

        if (diff < 0)
           std::cout << "" << std::endl;

        std::cout << "delta: " << diff << std::endl << std::endl;
        then = now;






        DataPacketPtr x_packet, y_packet, z_packet; 
        auto domainPacket = DataPacket(timeSignal.getDescriptor(), sweep_size, sweep_time);

        x_packet = DataPacketWithDomain(domainPacket, x_signal.getDescriptor(), sweep_size);
        y_packet = DataPacketWithDomain(domainPacket, y_signal.getDescriptor(), sweep_size);
        z_packet = DataPacketWithDomain(domainPacket, z_signal.getDescriptor(), sweep_size);

        double* x_packet_buffer = static_cast<double*>(x_packet.getRawData());
        double* y_packet_buffer = static_cast<double*>(y_packet.getRawData());
        double* z_packet_buffer = static_cast<double*>(z_packet.getRawData());
        
        for (int i = 0; i < sweep_size; i++)
        {
            if (sweeps[i].nodeAddress() == node_id)
            {
                x_packet_buffer[i] = sweeps[i].data()[0].as_float() * 100;
                y_packet_buffer[i] = sweeps[i].data()[1].as_float() * 100;
                z_packet_buffer[i] = sweeps[i].data()[2].as_float() * 100;
            }
            else
                break; 

           // std::cout << "sweep: " << i << "---> " << sweeps[i].data()[0].as_Timestamp().nanoseconds() / 1000 << std::endl; 
        }

        x_signal.sendPacket(std::move(x_packet));
        y_signal.sendPacket(std::move(y_packet));
        z_signal.sendPacket(std::move(z_packet));
        timeSignal.sendPacket(std::move(domainPacket));
    }

    samplesGenerated += sweep_size;
    //lastCollectTime = curTime;
    mutex_buffer.unlock(); 
}



std::tuple<PacketPtr, PacketPtr, PacketPtr, PacketPtr> RefChannelImpl::generateSamples(int64_t curTime, uint64_t samplesGenerated, uint64_t newSamples)
{
    mscl::DataSweeps sweeps = basestation.getData(1000, 0);
    int sweep_size = sweeps.size();


    uint64_t sweep_time = sweeps.data()[0].timestamp().nanoseconds() / 1000; 
    auto domainPacket = DataPacket(timeSignal.getDescriptor(), sweep_size, sweep_time); 

    DataPacketPtr x_packet, y_packet, z_packet; 

    x_packet = DataPacketWithDomain(domainPacket, x_signal.getDescriptor(), sweep_size);
    y_packet = DataPacketWithDomain(domainPacket, y_signal.getDescriptor(), sweep_size);
    z_packet = DataPacketWithDomain(domainPacket, z_signal.getDescriptor(), sweep_size);

    double* x_packet_buffer = static_cast<double*>(x_packet.getRawData());
    double* y_packet_buffer = static_cast<double*>(y_packet.getRawData());
    double* z_packet_buffer = static_cast<double*>(z_packet.getRawData());


        //mscl::ChannelData data = sweep.data();
    //if (sweep_buffer.size() > newSamples)

        //for (uint64_t i = 0; i < newSamples; i++)
        int i = 0; 
        for (mscl::DataSweep sweep : sweeps)
        {
            
            x_packet_buffer[i] = sweep.data()[0].as_float() * 100;
            y_packet_buffer[i] = sweep.data()[1].as_float() * 100;
            z_packet_buffer[i] = sweep.data()[2].as_float() * 100;

            i++;
        }


    return {x_packet, y_packet, z_packet, domainPacket};
}

Int RefChannelImpl::getDeltaT(const double sr) const
{
    const double tickPeriod = getResolution();
    const double samplePeriod = 1.0 / sr;
    const Int deltaT = static_cast<Int>(std::round(samplePeriod / tickPeriod));
    return deltaT;
}

void RefChannelImpl::buildSignalDescriptors()
{
    //const auto valueDescriptor = DataDescriptorBuilder()
    //                             .setSampleType(SampleType::Float64)
    //                             .setUnit(Unit("V", -1, "volts", "voltage"))
    //                             .setValueRange(customRange)
    //                             .setName("AI " + std::to_string(index + 1));

     const auto valueDescriptor = DataDescriptorBuilder()
                                  .setSampleType(SampleType::Float64)
                                  .setUnit(Unit("g"))
                                  .setValueRange(customRange)
                                  .setName("AXIS " + std::to_string(index + 1));

    if (clientSideScaling)
    {
        const double scale = 20.0 / std::pow(2, 24);
        constexpr double offset = -10.0;
        valueDescriptor.setPostScaling(LinearScaling(scale, offset, SampleType::Int32, ScaledSampleType::Float64));
    }

    if (waveformType == WaveformType::ConstantValue)
    {
        valueDescriptor.setRule(ConstantDataRule());
    }


    x_signal.setDescriptor(valueDescriptor.build());
    y_signal.setDescriptor(valueDescriptor.build());
    z_signal.setDescriptor(valueDescriptor.build());
    

    deltaT = getDeltaT(sampleRate);

    const auto timeDescriptor = DataDescriptorBuilder()
                                .setSampleType(SampleType::Int64)
                                .setUnit(Unit("s", -1, "seconds", "time"))
                                .setTickResolution(getResolution())
                                .setRule(LinearDataRule(deltaT, 0))
                                .setOrigin(getEpoch())
                                .setName("Time AI " + std::to_string(index + 1));

    timeSignal.setDescriptor(timeDescriptor.build());
}

double RefChannelImpl::coerceSampleRate(const double wantedSampleRate) const
{
    const double tickPeriod = getResolution();
    const double samplePeriod = 1.0 / wantedSampleRate;

    const double multiplier = samplePeriod / tickPeriod;

    double roundedMultiplier = std::round(multiplier);

    if (roundedMultiplier < 1.0)
        roundedMultiplier = 1.0;

    const double roundedSamplePeriod = roundedMultiplier * tickPeriod;

    double roundedSampleRate = 1.0 / roundedSamplePeriod;

    if (roundedSampleRate > 1000000)
        roundedSampleRate = 1000000;

    return roundedSampleRate;
}

void RefChannelImpl::createSignals()
{
    x_signal = createAndAddSignal(fmt::format("G-Link-200 Axis X")); 
    y_signal = createAndAddSignal(fmt::format("G-Link-200 Axis Y")); 
    z_signal = createAndAddSignal(fmt::format("G-Link-200 Axis Z")); 

    timeSignal = createAndAddSignal(fmt::format("AI{}Time", 0), nullptr);

    x_signal.setDomainSignal(timeSignal);
    y_signal.setDomainSignal(timeSignal);
    z_signal.setDomainSignal(timeSignal);


    //valueSignal = createAndAddSignal(fmt::format("ACCEL AXIS {}", index));
    //timeSignal = createAndAddSignal(fmt::format("AI{}Time", index), nullptr, false);
    //valueSignal.setDomainSignal(timeSignal);
}

void RefChannelImpl::globalSampleRateChanged(double newGlobalSampleRate)
{
    std::scoped_lock lock(sync);

    globalSampleRate = coerceSampleRate(newGlobalSampleRate);
    signalTypeChangedInternal();
    buildSignalDescriptors();
    updateSamplesGenerated();
}

std::string RefChannelImpl::getEpoch()
{
    const std::time_t epochTime = std::chrono::system_clock::to_time_t(std::chrono::time_point<std::chrono::system_clock>{});

    char buf[48];
    strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%SZ", gmtime(&epochTime));

    return { buf };
}

RatioPtr RefChannelImpl::getResolution()
{
    return Ratio(1, 1000000);
}

void RefChannelImpl::endApplyProperties(const UpdatingActions& propsAndValues, bool parentUpdating)
{
    ChannelImpl<IRefChannel>::endApplyProperties(propsAndValues, parentUpdating);

    if (needsSignalTypeChanged)
    {
        signalTypeChanged();
        needsSignalTypeChanged = false;
    }
}



END_NAMESPACE_REF_DEVICE_MODULE
