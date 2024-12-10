#include <wsda_200_device_module/wsda_200_can_channel_impl.h>
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
#include <opendaq/dimension_factory.h>


#define PI 3.141592653589793

BEGIN_NAMESPACE_WSDA_200_DEVICE_MODULE

WSDA200CANChannelImpl::WSDA200CANChannelImpl(const ContextPtr& context,
                                     const ComponentPtr& parent,
                                     const StringPtr& localId,
                                     const WSDA200CANChannelInit& init)
    : ChannelImpl(FunctionBlockType("WSDA200CANChannel",  "CAN", ""), context, parent, localId)
    , startTime(init.startTime)
    , microSecondsFromEpochToStartTime(init.microSecondsFromEpochToStartTime)
    , lastCollectTime(0)
    , samplesGenerated(0)
{
    initProperties();
    propChangedInternal();
    createSignals();
    buildSignalDescriptors();
}

void WSDA200CANChannelImpl::initProperties()
{
    const auto upperLimitProp = IntPropertyBuilder("UpperLimit", 1000).setMaxValue(10000000).setMinValue(1).build();
    objPtr.addProperty(upperLimitProp);
    objPtr.getOnPropertyValueWrite("UpperLimit") +=
        [this](PropertyObjectPtr& obj, PropertyValueEventArgsPtr& args) { propChanged(); };

    const auto lowerLimitProp = IntPropertyBuilder("LowerLimit", -1000).setMaxValue(1).setMinValue(-10000000).build();
    objPtr.addProperty(lowerLimitProp);
    objPtr.getOnPropertyValueWrite("LowerLimit") +=
        [this](PropertyObjectPtr& obj, PropertyValueEventArgsPtr& args) { propChanged(); };
}

void WSDA200CANChannelImpl::propChangedInternal()
{
    lowerLimit = objPtr.getPropertyValue("LowerLimit");
    upperLimit = objPtr.getPropertyValue("UpperLimit");
    counter1 = 0;
    counter2 = 0;
}

void WSDA200CANChannelImpl::propChanged()
{
    std::scoped_lock lock(sync);
    propChangedInternal();
}

void WSDA200CANChannelImpl::collectSamples(std::chrono::microseconds curTime)
{
    std::scoped_lock lock(sync);
    const auto duration = static_cast<int64_t>(curTime.count() - lastCollectTime.count());

    if (duration > 0 && valueSignal.getActive())
    {
        const auto time = static_cast<int64_t>(lastCollectTime.count()) + static_cast<int64_t>(microSecondsFromEpochToStartTime.count());
        generateSamples(time, duration, 2);
    }

    lastCollectTime = curTime;
}

void WSDA200CANChannelImpl::globalSampleRateChanged(double /* globalSampleRate */)
{
}

void WSDA200CANChannelImpl::generateSamples(int64_t curTime, uint64_t duration, size_t newSamples)
{
    const auto domainPacket = DataPacket(timeSignal.getDescriptor(), newSamples, curTime);
    const auto dataPacket = DataPacketWithDomain(domainPacket, valueSignal.getDescriptor(), newSamples);

    CANData* dataBuffer;
    int64_t* timeBuffer;

    dataBuffer = static_cast<CANData*>(dataPacket.getRawData());
    timeBuffer = static_cast<int64_t*>(domainPacket.getRawData());

    for (size_t i = 0; i < newSamples; i++)
    {
        dataBuffer->arbId = 12;
        dataBuffer->length = 8;

        int32_t* dataPtr = reinterpret_cast<int32_t*>(&(dataBuffer->data[0]));
        *dataPtr++ = counter1++;
        *dataPtr++ = counter2--;

        if (counter1 == upperLimit)
            counter1 = 0;
        if (counter2 == lowerLimit)
            counter2 = 0;
        dataBuffer++;

        *timeBuffer++ = curTime;
        curTime += duration / static_cast<uint64_t>(newSamples);
    }


    valueSignal.sendPacket(dataPacket);
    timeSignal.sendPacket(domainPacket);
}

void WSDA200CANChannelImpl::buildSignalDescriptors()
{
    const auto arbIdDescriptor = DataDescriptorBuilder().setName("ArbId").setSampleType(SampleType::Int32).build();

    const auto lengthDescriptor = DataDescriptorBuilder().setName("Length").setSampleType(SampleType::Int8).build();

    const auto dataDescriptor = DataDescriptorBuilder()
                                    .setName("Data")
                                    .setSampleType(SampleType::UInt8)
                                    .setDimensions(List<IDimension>(DimensionBuilder()
                                                                        .setRule(LinearDimensionRule(0, 1, 64))
                                                                        .setName("Dimension")
                                                                        .build()))
                                    .build();

    const auto canMsgDescriptor = DataDescriptorBuilder()
        .setSampleType(SampleType::Struct)
        .setStructFields(List<IDataDescriptor>(arbIdDescriptor, lengthDescriptor, dataDescriptor))
        .setName("CAN")
        .build();

    valueSignal.setDescriptor(canMsgDescriptor);

    const auto timeDescriptor = DataDescriptorBuilder()
                                .setSampleType(SampleType::Int64)
                                .setUnit(Unit("s", -1, "seconds", "time"))
                                .setTickResolution(getResolution())
                                .setOrigin(getEpoch())
                                .setName("Time CAN");

    timeSignal.setDescriptor(timeDescriptor.build());

    valueSignal.setDomainSignal(timeSignal);
}

void WSDA200CANChannelImpl::createSignals()
{
    valueSignal = createAndAddSignal("CAN");
    timeSignal = createAndAddSignal("CanTime", nullptr, false);
}

std::string WSDA200CANChannelImpl::getEpoch()
{
    const std::time_t epochTime = std::chrono::system_clock::to_time_t(std::chrono::time_point<std::chrono::system_clock>{});

    char buf[48];
    strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%SZ", gmtime(&epochTime));

    return { buf };
}

RatioPtr WSDA200CANChannelImpl::getResolution()
{
    return Ratio(1, 1000000);
}

END_NAMESPACE_WSDA_200_DEVICE_MODULE