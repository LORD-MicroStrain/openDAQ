#include <wsda_200_device_module/wsda_200_channel_impl.h>
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


#define PI 3.141592653589793

BEGIN_NAMESPACE_WSDA_200_DEVICE_MODULE

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
//mscl::Connection connection = mscl::Connection::Serial("COM4", 3000000);
//mscl::Connection connection = mscl::Connection::Serial("COM12", 3000000);
//mscl::BaseStation basestation(connection);
////int node_id = 12345;
// //int node_id = 5;
//int node_id = 40415;

CircularBuffer<mscl::DataSweep> sweep_buffer(128);

int num_sweeps = 0;
bool data_flowing = false;

std::mutex load_buffer_lock;

WSDA200ChannelImpl::WSDA200ChannelImpl(const ContextPtr& context, const ComponentPtr& parent, const StringPtr& localId, const WSDA200ChannelInit& init)
    : ChannelImpl(FunctionBlockType("WSDA200Channel",  fmt::format("AI{}", init.index + 1), ""), context, parent, localId)
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


void WSDA200ChannelImpl::initMSCL(uint8_t section)
{

    std::cout << "\n\nenter the node id " << std::endl;
    std::cin >> node_id;

    std::cout << "enter the COM port " << std::endl;
    std::cin >> comPort;


    mscl::Connection connection = mscl::Connection::Serial(comPort, 3000000);
    basestation = new mscl::BaseStation(connection);
}

//uint64_t then;
//uint64_t last_size;


void WSDA200ChannelImpl::signalTypeChangedIfNotUpdating(const PropertyValueEventArgsPtr& args)
{
    if (!args.getIsUpdating())
        signalTypeChanged();
    else
        needsSignalTypeChanged = true;
}

void WSDA200ChannelImpl::initProperties()
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

void WSDA200ChannelImpl::packetSizeChangedInternal()
{
    fixedPacketSize = objPtr.getPropertyValue("FixedPacketSize");
    packetSize = objPtr.getPropertyValue("PacketSize");
}

void WSDA200ChannelImpl::packetSizeChanged()
{
    std::scoped_lock lock(sync);

    packetSizeChangedInternal();
}

void WSDA200ChannelImpl::waveformChangedInternal()
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

void WSDA200ChannelImpl::updateSamplesGenerated()
{
    if (lastCollectTime.count() > 0)
        samplesGenerated = getSamplesSinceStart(lastCollectTime);
}

void WSDA200ChannelImpl::waveformChanged()
{
    std::scoped_lock lock(sync);
    waveformChangedInternal();
}

void WSDA200ChannelImpl::signalTypeChanged()
{
    std::scoped_lock lock(sync);
    signalTypeChangedInternal();
    buildSignalDescriptors();
    updateSamplesGenerated();
}

void WSDA200ChannelImpl::signalTypeChangedInternal()
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

void WSDA200ChannelImpl::resetCounter()
{
    std::scoped_lock lock(sync);
    counter = 0;
}

uint64_t WSDA200ChannelImpl::getSamplesSinceStart(std::chrono::microseconds time) const
{
    const uint64_t samplesSinceStart = static_cast<uint64_t>(std::trunc(static_cast<double>((time - startTime).count()) / 1000000.0 * sampleRate));
    return samplesSinceStart;
}


void WSDA200ChannelImpl::collectSamples(std::chrono::microseconds curTime)
{
    mscl::DataSweeps sweeps = basestation->getData(20, 0);
    int sweep_size = sweeps.size();

    if (sweep_size > 0)
    {
        uint64_t sweep_time = (sweeps.data()[0].timestamp().nanoseconds() / 1000);
        samplesGenerated += sweep_size;

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
        }

        x_signal.sendPacket(std::move(x_packet));
        y_signal.sendPacket(std::move(y_packet));
        z_signal.sendPacket(std::move(z_packet));
        timeSignal.sendPacket(std::move(domainPacket));

    }
}

Int WSDA200ChannelImpl::getDeltaT(const double sr) const
{
    const double tickPeriod = getResolution();
    const double samplePeriod = 1.0 / sr;
    const Int deltaT = static_cast<Int>(std::round(samplePeriod / tickPeriod));
    return deltaT;
}

void WSDA200ChannelImpl::buildSignalDescriptors()
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

double WSDA200ChannelImpl::coerceSampleRate(const double wantedSampleRate) const
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

void WSDA200ChannelImpl::createSignals()
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

void WSDA200ChannelImpl::globalSampleRateChanged(double newGlobalSampleRate)
{
    std::scoped_lock lock(sync);

    globalSampleRate = coerceSampleRate(newGlobalSampleRate);
    signalTypeChangedInternal();
    buildSignalDescriptors();
    updateSamplesGenerated();
}

std::string WSDA200ChannelImpl::getEpoch()
{
    const std::time_t epochTime = std::chrono::system_clock::to_time_t(std::chrono::time_point<std::chrono::system_clock>{});

    char buf[48];
    strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%SZ", gmtime(&epochTime));

    return { buf };
}

RatioPtr WSDA200ChannelImpl::getResolution()
{
    return Ratio(1, 1000000);
}

void WSDA200ChannelImpl::endApplyProperties(const UpdatingActions& propsAndValues, bool parentUpdating)
{
    ChannelImpl<IWSDA200Channel>::endApplyProperties(propsAndValues, parentUpdating);

    if (needsSignalTypeChanged)
    {
        signalTypeChanged();
        needsSignalTypeChanged = false;
    }
}



END_NAMESPACE_WSDA_200_DEVICE_MODULE
