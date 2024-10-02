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
    , sampleRate(init.node_sample_rate)
    , node_id(init.node_id)
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
    //initMSCL();

    initProperties();
    //waveformChangedInternal();
    //signalTypeChangedInternal();
    //packetSizeChangedInternal();
    resetCounter();
    createSignals();
    buildSignalDescriptors();
}

void WSDA200ChannelImpl::initMSCL()
{
    int option = 0;
    bool choice_done = false; 

    while (!choice_done)
    {
        std::cout << "\n\n1: WSDA-200 \n2: WSDA-2000\nENTER OPTION: ";
        std::cin >> option;

        if (option == 1)                                // WSDA-200 
        {
            std::cout << "\nenter the COM port: ";
            std::cin >> comPort;
            mscl::Connection connection = mscl::Connection::Serial(comPort, 3000000);
            basestation = new mscl::BaseStation(connection);

            choice_done = true;
        }
        else if (option == 2)                           // WSDA-2000
        {
            char *tcpaddress, *ipaddress; 
            std::cout << "\nEnter your tcp address: \n";
            //std::cin >> tcpaddress; 
            std::cout << "\nEnter your ip address: "; 
            //std::cin >> ipaddress;

            mscl::Connection connection = mscl::Connection::TcpIp("fd7a:cafa:0eb7:6578:0001:d585::1", 5000);   // address for jeffs wsda-2000
            basestation = new mscl::BaseStation(connection);

            choice_done = true;
        }
        else
        {
            for (int k = 0; k < 0xFFFFFFF; k++); 
            std::cout << "\nretry\n";
            for (int k = 0; k < 0xFFFFFFF; k++); 
        }
    }

    //idleAll(); 
    nodePollAndSelection();  
 }

void WSDA200ChannelImpl::nodePollAndSelection()
{
    std::cout << "\n\npolling for nodes sampling.";
    for (int k = 0; k < 0xFFFFFFF; k++);
    std::cout << ".";
    for (int k = 0; k < 0xFFFFFFF; k++);
    std::cout << ".";
    for (int k = 0; k < 0xFFFFFFF; k++);
    std::cout << ".";
    for (int k = 0; k < 0xFFFFFFF; k++);
    std::cout << ".";
    for (int k = 0; k < 0xFFFFFFF; k++);

    //basestation->resetRadio(); 
    mscl::DataSweeps sweeps = basestation->getData(1000, 0);
    

    int sweep_size = basestation->getData(1000, 0).size();

    if (sweep_size == 0)
    {
        //basestation->resetRadio();
        mscl::Connection connection = mscl::Connection::TcpIp("fd7a:cafa:0eb7:6578:0001:d585::1", 5000);  // address for jeffs wsda-2000
        basestation = new mscl::BaseStation(connection);

        std::cout << "\n\nretrying"; 
        for (int k = 0; k < 0xFFFFFFF; k++);
        std::cout << ".";
        for (int k = 0; k < 0xFFFFFFF; k++);
        std::cout << ".";
        for (int k = 0; k < 0xFFFFFFF; k++);
        std::cout << ".";
        for (int k = 0; k < 0xFFFFFFF; k++);
        std::cout << ".";
        for (int k = 0; k < 0xFFFFFFF; k++);
        sweeps = basestation->getData(1000, 0);
        sweep_size = sweeps.size();

        if (sweep_size > 0)
            auto a = 0;
    }

    int node_list[20];
    int node_list_size = 0;
    bool found_in_node_list;


    int z, k;
    for (k = 0; k < sweep_size; k++)  // iterates through sweeps
    {
        found_in_node_list = false;
        for (z = 0; z < node_list_size; z++)  // checks our node list for unique id
            if (node_list[z] == (int) sweeps[k].nodeAddress())
                found_in_node_list = true;

        if (!found_in_node_list)
        {
            auto temp = mscl::WirelessNode(sweeps[k].nodeAddress(), *basestation);

            std::cout << "\n\nnode #" << z + 1 << "\n";
            // std::cout << "\nmodel name: " << temp.model() << " ";
            // std::cout << "\nactive channels: " << (int)temp.getActiveChannels().count();  
            std::cout << "\nnode address: " << (int) sweeps[k].nodeAddress();        
            std::cout << "\nnode samplerate: " << sweeps[k].sampleRate().samples();         // prints out charactaristics of node
            std::cout << "\nnode sampling type: " << sweeps[k].samplingType();  
            std::cout << "\n";

            sampleRate = sweeps[k].sampleRate().samples();

            node_list[z] = sweeps[k].nodeAddress();
            node_list_size++;
        }
    }

    std::cout << "\n\nenter the node id: ";
    std::cin >> node_id;

    for (mscl::DataSweep sweep : sweeps)
        if (sweep.nodeAddress() == node_id)
        {
            // sampleRate = sweep.sampleRate().samples();
            sampleRate = sweep.sampleRate().samples();  // establishes sample rate for opendaq
            break;
        }
}

void WSDA200ChannelImpl::idleAll()
{
    std::cout << "\n\npolling for nodes to idle";
    for (int k = 0; k < 0xFFFFFFF; k++);
    std::cout << ".";
    for (int k = 0; k < 0xFFFFFFF; k++);
    std::cout << ".";
    for (int k = 0; k < 0xFFFFFFF; k++);
    std::cout << ".";
    for (int k = 0; k < 0xFFFFFFF; k++);
    std::cout << ".";
    for (int k = 0; k < 0xFFFFFFF; k++);

    mscl::DataSweeps sweeps = basestation->getData(100, 0);
    int sweep_size = sweeps.size();

    int node_list[20];
    int node_list_size = 0;
    bool found_in_node_list; 

    int z, k;
    for (k = 0; k < sweep_size; k++)  // iterates through sweeps
    {

        auto temp_id = sweeps[k].nodeAddress(); 
            
        found_in_node_list = false;
        for (z = 0; z < node_list_size; z++)  // checks our node list for unique id
            if (node_list[z] == (int) sweeps[k].nodeAddress())
                found_in_node_list = true;

        if (!found_in_node_list)
        {
            auto temp = mscl::WirelessNode(sweeps[k].nodeAddress(), *basestation);


            mscl::SetToIdleStatus idleStatus = temp.setToIdle();  std::cout << "\n";

            auto sync_info = mscl::SyncNetworkInfo(temp); 

            std::cout << "\nidling node: " << sweeps[k].nodeAddress();

            while (!idleStatus.complete())
            {
                std::cout << ".";
                for (int k = 0; k < 0xFFFFFFF; k++);
            }

            switch (idleStatus.result())
            {
                case mscl::SetToIdleStatus::setToIdleResult_success:
                    std::cout << "Node is now in idle mode.\n" << std::endl;
                    break;

                case mscl::SetToIdleStatus::setToIdleResult_canceled:
                    std::cout << "Set to Idle was canceled!\n" << std::endl;
                    break;

                case mscl::SetToIdleStatus::setToIdleResult_failed:
                    std::cout << "Set to Idle has failed!\n" << std::endl;
                    break;
            }


            node_list[z] = sweeps[k].nodeAddress();
            node_list_size++;
            temp.resendStartSyncSampling();

            while (!sync_info.startedSampling())
            {
                std::cout << ".";
                for (int k = 0; k < 0xFFFFFF; k++); 
            }
        }
    }
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
    /* const auto waveformProp = SelectionProperty("Waveform", List<IString>("Sine", "Rect", "None", "Counter", "Constant"), 0);
    
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


*/
    const auto defaultCustomRange = Range(0.0 * (10), 2.5 * (10));
    objPtr.addProperty(StructPropertyBuilder("CustomRange", defaultCustomRange).build());
    objPtr.getOnPropertyValueWrite("CustomRange") +=
        [this](PropertyObjectPtr& obj, PropertyValueEventArgsPtr& args) { signalTypeChangedIfNotUpdating(args); };

    
/*
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

        */
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
    /* if (objPtr.getPropertyValue("UseGlobalSampleRate"))
        sampleRate = globalSampleRate;
    else
        sampleRate = objPtr.getPropertyValue("SampleRate");
     */   
    clientSideScaling = objPtr.getPropertyValue("ClientSideScaling");

    customRange = objPtr.getPropertyValue("CustomRange");

    waveformType = objPtr.getPropertyValue("Waveform");

    //sampleRate = 256;  // PETER heres where sample rate is established

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


void WSDA200ChannelImpl::setSampleRate(int rate)
{
    //sampleRate = rate; 
}


void WSDA200ChannelImpl::collectSamples(std::chrono::microseconds curTime)
{
    mscl::DataSweeps sweeps = basestation->getData(100, 0);
    int sweep_size = sweeps.size();

    int sweeps_we_want = 0;
    uint64_t sweep_time;
    bool first = 1; 

    for (int k = 0; k < sweep_size; k++)
        if (sweeps.data()[k].nodeAddress() == node_id)       // sets time stamp as first instance of node we are plotting
        {                                                    // counts amount of relevent data points in our sweep  
            sweeps_we_want++;

            if (first)
            {
                auto temp = sweeps.data()[k]; 
                sweep_time = temp.timestamp().nanoseconds() / 1000; 
                first = 0; 
            }
        }

    if (sweeps_we_want > 0)
    {
        //samplesGenerated += sweep_size;
        samplesGenerated += sweeps_we_want;

        DataPacketPtr x_packet, y_packet, z_packet; 
        //auto domainPacket = DataPacket(timeSignal.getDescriptor(), sweep_size, sweep_time);
        auto domainPacket = DataPacket(timeSignal.getDescriptor(), sweeps_we_want, sweep_time);

        x_packet = DataPacketWithDomain(domainPacket, channel_1.getDescriptor(), sweeps_we_want);
        y_packet = DataPacketWithDomain(domainPacket, channel_2.getDescriptor(), sweeps_we_want);
        z_packet = DataPacketWithDomain(domainPacket, channel_3.getDescriptor(), sweeps_we_want);

        double* x_packet_buffer = static_cast<double*>(x_packet.getRawData());
        double* y_packet_buffer = static_cast<double*>(y_packet.getRawData());
        double* z_packet_buffer = static_cast<double*>(z_packet.getRawData());

        int x = 0, y = 0, count = 0;

        for (int i = 0; i < sweep_size; i++)
            if (sweeps[i].nodeAddress() == node_id)
            {
                x_packet_buffer[count] = sweeps[i].data()[0].as_float() * 10;
                y_packet_buffer[count] = sweeps[i].data()[1].as_float() * 10;
                z_packet_buffer[count] = sweeps[i].data()[2].as_float() * 10;

                x++; count++;
            }
            else
                y++; 

        channel_1.sendPacket(std::move(x_packet));
        channel_2.sendPacket(std::move(y_packet));
        channel_3.sendPacket(std::move(z_packet));
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
                                  .setUnit(Unit("HBK"))
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


    channel_1.setDescriptor(valueDescriptor.build());
    channel_2.setDescriptor(valueDescriptor.build());
    channel_3.setDescriptor(valueDescriptor.build());
    

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
    channel_1 = createAndAddSignal(fmt::format("node_" + std::to_string(node_id) + "_channel_1")); 
    channel_2 = createAndAddSignal(fmt::format("node_" + std::to_string(node_id) + "_channel_2")); 
    channel_3 = createAndAddSignal(fmt::format("node_" + std::to_string(node_id) + "_channel_3")); 

    timeSignal = createAndAddSignal(fmt::format("AI{}Time", 0), nullptr);

    channel_1.setDomainSignal(timeSignal);
    channel_2.setDomainSignal(timeSignal);
    channel_3.setDomainSignal(timeSignal);


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
