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

WSDA200ChannelImpl::WSDA200ChannelImpl(const ContextPtr& context, const ComponentPtr& parent, const StringPtr& localId, const WSDA200ChannelInit& init)
    : ChannelImpl(FunctionBlockType("WSDA200Channel",  fmt::format("AI{}", init.index + 1), ""), context, parent, localId)
    , waveformType(WaveformType::None)
    , freq(0)
    , ampl(0)
    , dc(0)
    , noiseAmpl(0)
    , constantValue(0)
    , node_id(init.node_id)
    , num_signals(init.num_signals)
    , basestation(init.basestation)
    , sampleRate(init.node_sample_rate)
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
    //initProperties();
    //waveformChangedInternal();
    //signalTypeChangedInternal();
    //packetSizeChangedInternal();
    //resetCounter();
    createSignals();
    buildSignalDescriptors();
}
/*
void WSDA200ChannelImpl::collectSamples(std::chrono::microseconds curTime)
{ 
    sweeps = basestation->getData(100, 0);
    int sweep_size = sweeps.size();

    int sweeps_we_want = 0;
    uint64_t sweep_time;
    bool first = 1; 

    for (int k = 0; k < sweep_size; k++)
        if ((int)sweeps.data()[k].nodeAddress() == node_id)       // sets time stamp as first instance of node we are plotting
        {                                                     
            sweeps_we_want++;                                // counts amount of relevent data points in our sweep 

            if (first)
            {
                auto temp = sweeps.data()[k]; 
                sweep_time = temp.timestamp().nanoseconds() / 1000; 
                first = 0; 
            }
        }

    if (sweeps_we_want > 0)
    { 
        samplesGenerated += sweeps_we_want;
        auto domainPacket = DataPacket(timeSignal.getDescriptor(), sweeps_we_want, sweep_time);


        DataPacketPtr* packets = new DataPacketPtr[num_signals];  // creates packet config info for each channel sending data
        for (int k = 0; k < num_signals; k++)
            packets[k] = DataPacketWithDomain(domainPacket, channel_list[k].getDescriptor(), sweeps_we_want);
                                                                                                                               

        double** packet_buffer = new double*[num_signals];  // creates packet we are storing out data in for each channel sending data 
        for (int k = 0; k < num_signals; k++)
            packet_buffer[k] = static_cast<double*>(packets[k].getRawData());


        int x = 0, y = 0, count = 0;

        for (int i = 0; i < sweep_size; i++)
            if ((int)sweeps[i].nodeAddress() == node_id) 
            {
                for (int k = 0; k < num_signals; k++) // loops through each channel sending data on node
                    packet_buffer[k][count] = sweeps[i].data()[k].as_float();

                x++; count++;
            }
            else
                y++; 

        for (int k = 0; k < num_signals; k++)
            channel_list[k].sendPacket(std::move(packets[k])); // finally push the data
        timeSignal.sendPacket(std::move(domainPacket)); // and time signal

        delete[] packets;
        delete[] packet_buffer; 
    }
}*/

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

        DataPacketPtr* packets = new DataPacketPtr[10]; 

        x_packet = DataPacketWithDomain(domainPacket, channel_list[0].getDescriptor(), sweep_size);
        y_packet = DataPacketWithDomain(domainPacket, channel_list[1].getDescriptor(), sweep_size);
        z_packet = DataPacketWithDomain(domainPacket, channel_list[2].getDescriptor(), sweep_size);

        double** packet_buffers = new double*[3];

        double* x_packet_buffer = static_cast<double*>(x_packet.getRawData());
        double* y_packet_buffer = static_cast<double*>(y_packet.getRawData());
        double* z_packet_buffer = static_cast<double*>(z_packet.getRawData());

        for (int i = 0; i < sweep_size; i++)
        {
            if ((int) sweeps[i].nodeAddress() == node_id)
            {
                x_packet_buffer[i] = sweeps[i].data()[0].as_float();
                y_packet_buffer[i] = sweeps[i].data()[1].as_float();
                z_packet_buffer[i] = sweeps[i].data()[2].as_float();
            }
            else
                break;
        }

        channel_list[0].sendPacket(std::move(x_packet));
        channel_list[1].sendPacket(std::move(y_packet));
        channel_list[2].sendPacket(std::move(z_packet));
        timeSignal.sendPacket(std::move(domainPacket));

        delete[] packets;
        delete[] packet_buffers;
    }
 }


void WSDA200ChannelImpl::createSignals()
{
    channel_list =  (SignalConfigPtr*)malloc(num_signals * sizeof(SignalConfigPtr));

    for (int k = 0; k < num_signals; k++) // creates signal for each channel on a node streaming data
        channel_list[k] = createAndAddSignal(fmt::format("node_" + std::to_string(node_id) + "_channel_" + std::to_string(k + 1)));
    
    timeSignal = createAndAddSignal(fmt::format("AI{}Time", 0), nullptr);

    for (int k = 0; k < num_signals; k++) // assigns time signal to each signal
        channel_list[k].setDomainSignal(timeSignal); 

    //valueSignal = createAndAddSignal(fmt::format("ACCEL AXIS {}", index));
    //timeSignal = createAndAddSignal(fmt::format("AI{}Time", index), nullptr, false);
    //valueSignal.setDomainSignal(timeSignal);
}

void WSDA200ChannelImpl::buildSignalDescriptors()
{
    //const auto valueDescriptor = DataDescriptorBuilder()
    //                             .setSampleType(SampleType::Float64)
    //                             .setUnit(Unit("V", -1, "volts", "voltage"))
    //                             .setValueRange(customRange)
    //                             .setName("AI " + std::to_string(index + 1));

    /* std::cout << "\n\nSetting Ranges";
    delay(0xFFFFFFF, 10);
    sweeps = basestation->getData(1000, 0);
    float* mins = (float*) malloc(num_signals * sizeof(float)); 
    float* maxs = (float*) malloc(num_signals * sizeof(float));  // creating arrays to store max and min value for each channel streaming

    float min = 0, max = 0;

    int x = 0, y = 0; 
    for (int k = 0; k < num_signals; k++)
    {
        for (int z = 0; z < sweeps.size(); z++)
            if (sweeps[z].nodeAddress() == node_id)
            {                                              // initializes first instance of data we've selected
                mins[k] = sweeps[z].data()[k].as_float();  // as max and min values and breaks out
                maxs[k] = sweeps[z].data()[k].as_float();
                x++;
                break;
            }
            else
                y++; 
    }

    for (int k = 0; k < sweeps.size(); k++)
    {
        if (sweeps[k].nodeAddress() == node_id)             // loops through all valid data and set max and min range
            for (int z = 0; z < num_signals; z++)           // needed to display data in a pretty and readable way
            {
                auto temp = sweeps[k].data()[z].as_float();

                if (temp < mins[z])
                    mins[z] = temp;

                if (temp > maxs[z])
                    maxs[z] = temp;
            }
    }

    x = 0, y = 0;
    for (int k = 0; k < sweeps.size(); k++)
        if (sweeps[k].nodeAddress() == node_id)
            x++;
        else
            y++; */                 // un comment block for auto scale

    /* if (clientSideScaling)
    {
        const double scale = 20.0 / std::pow(2, 24);
        constexpr double offset = -10.0;
        valueDescriptor.setPostScaling(LinearScaling(scale, offset, SampleType::Int32, ScaledSampleType::Float64));
    }

    if (waveformType == WaveformType::ConstantValue)
    {
        valueDescriptor.setRule(ConstantDataRule());
    }*/


    //customRange = objPtr.getPropertyValue("CustomRange");
    for (int k = 0; k < num_signals; k++)
    {                                                   // function loops through all channels we've set to stream
        //float delta = maxs[k] - mins[k];   uncomment for auto scale  // and sets their y axis via the value descriptor to make the data
                                                        // pretty and readable. function block needs this data value to be set
        //const auto defaultCustomRange = Range(mins[k] - delta * 0.2, maxs[k] + delta * 0.2); this line will auto scale 
        const auto defaultCustomRange = Range(-10, 10);
            objPtr.addProperty(StructPropertyBuilder("CustomRange_" + std::to_string(k), defaultCustomRange).build());
            objPtr.getOnPropertyValueWrite("CustomRange_" + std::to_string(k)) +=
            [this](PropertyObjectPtr& obj, PropertyValueEventArgsPtr& args) { signalTypeChangedIfNotUpdating(args); };

        const auto valueDescriptor = DataDescriptorBuilder()
                                  .setSampleType(SampleType::Float64)
                                  //.setUnit(Unit("HBK"))
                                  .setUnit(Unit("g"))
                                  .setValueRange(objPtr.getPropertyValue("CustomRange_" + std::to_string(k)))
                                  .setName("AXIS " + std::to_string(index + 1));

        channel_list[k].setDescriptor(valueDescriptor.build());
    }

    deltaT = getDeltaT(sampleRate);
    const auto timeDescriptor = DataDescriptorBuilder()
                                .setSampleType(SampleType::Int64)
                                .setUnit(Unit("s", -1, "seconds", "time"))  // sets value descritor for time signal
                                .setTickResolution(getResolution())         // unsure how needed this one is 
                                .setRule(LinearDataRule(deltaT, 0))
                                .setOrigin(getEpoch())
                                .setName("Time AI " + std::to_string(index + 1));

    timeSignal.setDescriptor(timeDescriptor.build());

    //free(mins); autoscale
    //free(maxs); autoscale
}

void WSDA200ChannelImpl::delay(int counter, int times)
{
    std::cout << ".";
    for (int z = 0; z < times; z++)
    {
        for (int k = 0; k < counter; k++); 
        std::cout << ".";
    }
}


//////////////////////////////////////////////////////////////////////////////////////////////////


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



    const auto defaultCustomRange = Range(-1, 1);
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
    // if (objPtr.getPropertyValue("UseGlobalSampleRate"))
      //  sampleRate = globalSampleRate;
    //else
      //  sampleRate = objPtr.getPropertyValue("SampleRate");
        
    clientSideScaling = objPtr.getPropertyValue("ClientSideScaling");

    customRange = objPtr.getPropertyValue("CustomRange");

    waveformType = objPtr.getPropertyValue("Waveform");

    //sampleRate = 256;  // PETER heres where sample rate is established

    LOG_I("Properties: SampleRate {}, ClientSideScaling {}", sampleRate, clientSideScaling);
}

Int WSDA200ChannelImpl::getDeltaT(const double sr) const
{
    const double tickPeriod = getResolution();
    const double samplePeriod = 1.0 / sr;
    const Int deltaT = static_cast<Int>(std::round(samplePeriod / tickPeriod));
    return deltaT;
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

void WSDA200ChannelImpl::globalSampleRateChanged(double newGlobalSampleRate)
{
    std::scoped_lock lock(sync);

    globalSampleRate = coerceSampleRate(newGlobalSampleRate);
    signalTypeChangedInternal();
    buildSignalDescriptors();
    updateSamplesGenerated();
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
