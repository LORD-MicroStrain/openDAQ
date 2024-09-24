#include <wsda_200_device_module/wsda_200_device_impl.h>
#include <opendaq/device_info_factory.h>
#include <coreobjects/unit_factory.h>
#include <wsda_200_device_module/wsda_200_channel_impl.h>
#include <wsda_200_device_module/wsda_200_can_channel_impl.h>
#include <opendaq/module_manager_ptr.h>
#include <fmt/format.h>
#include <opendaq/custom_log.h>
#include <opendaq/device_type_factory.h>
#include <opendaq/device_domain_factory.h>
#include <utility>
#include "mscl/mscl.h"

BEGIN_NAMESPACE_WSDA_200_DEVICE_MODULE

WSDA200DeviceImpl::WSDA200DeviceImpl(size_t id, const PropertyObjectPtr& config, const ContextPtr& ctx, const ComponentPtr& parent, const StringPtr& localId, const StringPtr& name)
    : GenericDevice<>(ctx, parent, localId, nullptr, name)
    , id(id)
    , microSecondsFromEpochToDeviceStart(0)
    , acqLoopTime(0)
    , stopAcq(false)
    , logger(ctx.getLogger())
    , loggerComponent( this->logger.assigned()
                          ? this->logger.getOrAddComponent(WSDA_200_MODULE_NAME)
                          : throw ArgumentNullException("Logger must not be null"))
{
    initIoFolder();
    initSyncComponent();
    initClock();
    initProperties(config);
    updateNumberOfChannels();
    enableCANChannel();
    updateAcqLoopTime();

    if (config.assigned())
    {
        if (config.hasProperty("LocalId"))
            serialNumber = config.getPropertyValue("SerialNumber");
    }
    
    const auto options = this->context.getModuleOptions(WSDA_200_MODULE_NAME);
    if (options.assigned())
    {
        if (options.hasKey("SerialNumber"))
            serialNumber = options.get("SerialNumber");
    }

    acqThread = std::thread{ &WSDA200DeviceImpl::acqLoop, this };
    //acqThread2 = std::thread{ &MSCLDeviceImpl::hello, this };
}

WSDA200DeviceImpl::~WSDA200DeviceImpl()
{
    {
        std::scoped_lock<std::mutex> lock(sync);
        stopAcq = true;
    }
    cv.notify_one();

    acqThread.join();
}

DeviceInfoPtr WSDA200DeviceImpl::CreateDeviceInfo(size_t id, const StringPtr& serialNumber)
{
    auto devInfo = DeviceInfo(fmt::format("daqwsda200://device{}", id));
    devInfo.setName(fmt::format("Device {}", id));
    devInfo.setManufacturer("openDAQ");
    devInfo.setModel("WSDA200 refrence device");
    devInfo.setSerialNumber(serialNumber.assigned() ? serialNumber : String(fmt::format("dev_ser_{}", id)));
    devInfo.setDeviceType(CreateType());

    return devInfo;
}

DeviceTypePtr WSDA200DeviceImpl::CreateType()
{
    return DeviceType("daqwsda200",
                      "WSDA200 refrence device",
                      "WSDA200 refrence device",
                      "daqwsda200");
}

DeviceInfoPtr WSDA200DeviceImpl::onGetInfo()
{
    auto deviceInfo = WSDA200DeviceImpl::CreateDeviceInfo(id, serialNumber);
    deviceInfo.freeze();
    return deviceInfo;
}

uint64_t WSDA200DeviceImpl::onGetTicksSinceOrigin()
{
    auto microSecondsSinceDeviceStart = getMicroSecondsSinceDeviceStart();
    auto ticksSinceEpoch = microSecondsFromEpochToDeviceStart + microSecondsSinceDeviceStart;
    return static_cast<SizeT>(ticksSinceEpoch.count());
}

bool WSDA200DeviceImpl::allowAddDevicesFromModules()
{
    return true;
}

bool WSDA200DeviceImpl::allowAddFunctionBlocksFromModules()
{
    return true;
}

std::chrono::microseconds WSDA200DeviceImpl::getMicroSecondsSinceDeviceStart() const
{
    auto currentTime = std::chrono::steady_clock::now();
    auto microSecondsSinceDeviceStart = std::chrono::duration_cast<std::chrono::microseconds>(currentTime - startTime);
    return microSecondsSinceDeviceStart;
}

void WSDA200DeviceImpl::initClock()
{
    startTime = std::chrono::steady_clock::now();
    auto startAbsTime = std::chrono::system_clock::now();

    microSecondsFromEpochToDeviceStart = std::chrono::duration_cast<std::chrono::microseconds>(startAbsTime.time_since_epoch());

    this->setDeviceDomain(DeviceDomain(WSDA200ChannelImpl::getResolution(), WSDA200ChannelImpl::getEpoch(), UnitBuilder().setName("second").setSymbol("s").setQuantity("time").build()));
}

void WSDA200DeviceImpl::initIoFolder()
{
    aiFolder = this->addIoFolder("AI", ioFolder);
    canFolder = this->addIoFolder("CAN", ioFolder);
}

void WSDA200DeviceImpl::initSyncComponent()
{
    syncComponent = this->addComponent("sync");

    syncComponent.addProperty(BoolProperty("UseSync", False));
    syncComponent.getOnPropertyValueWrite("UseSync") +=
        [this](PropertyObjectPtr& obj, PropertyValueEventArgsPtr& args) { };
}

void WSDA200DeviceImpl::acqLoop()
{
    using namespace std::chrono_literals;
    using  milli = std::chrono::milliseconds;

    auto startLoopTime = std::chrono::high_resolution_clock::now();
    const auto loopTime = milli(acqLoopTime);

    std::unique_lock<std::mutex> lock(sync);

    while (!stopAcq)
    {
        const auto time = std::chrono::high_resolution_clock::now();
        const auto loopDuration = std::chrono::duration_cast<milli>(time - startLoopTime);
        const auto waitTime = loopDuration.count() >= loopTime.count() ? milli(0) : milli(loopTime.count() - loopDuration.count());
        startLoopTime = time;

        cv.wait_for(lock, waitTime);
        if (!stopAcq)
        {
            auto curTime = getMicroSecondsSinceDeviceStart();

            //std::cout << "------------------------curTime: " <<  curTime.count() << std::endl; 

            for (auto& ch : channels)
            {
                auto chPrivate = ch.asPtr<IWSDA200Channel>();
                chPrivate->collectSamples(curTime);
            }

            if (canChannel.assigned())
            {
                auto chPrivate = canChannel.asPtr<IWSDA200Channel>();
                chPrivate->collectSamples(curTime);
            }
        }
    }
}

void WSDA200DeviceImpl::initProperties(const PropertyObjectPtr& config)
{
    size_t numberOfChannels = 1;
    bool enableCANChannel = false;

    if (config.assigned())
    {
        if (config.hasProperty("NumberOfChannels"))
            numberOfChannels = config.getPropertyValue("NumberOfChannels");

        if (config.hasProperty("EnableCANChannel"))
            enableCANChannel = config.getPropertyValue("EnableCANChannel");
    } 
    
    const auto options = this->context.getModuleOptions(WSDA_200_MODULE_NAME);
    if (options.assigned())
    {
        if (options.hasKey("NumberOfChannels"))
            numberOfChannels = options.get("NumberOfChannels");

        if (options.hasKey("EnableCANChannel"))
            enableCANChannel = options.get("EnableCANChannel");
    }

    if (numberOfChannels < 1 || numberOfChannels > 4096)
        throw InvalidParameterException("Invalid number of channels");

    objPtr.addProperty(IntProperty("NumberOfChannels", numberOfChannels));
    objPtr.getOnPropertyValueWrite("NumberOfChannels") +=
        [this](PropertyObjectPtr& obj, PropertyValueEventArgsPtr& args) { updateNumberOfChannels(); };
		
    const auto globalSampleRatePropInfo =
        FloatPropertyBuilder("GlobalSampleRate", 1000.0).setUnit(Unit("Hz")).setMinValue(1.0).setMaxValue(1000000.0).build();

    objPtr.addProperty(globalSampleRatePropInfo);
    objPtr.getOnPropertyValueWrite("GlobalSampleRate") +=
        [this](PropertyObjectPtr& obj, PropertyValueEventArgsPtr& args) { updateGlobalSampleRate(); };

    const auto acqLoopTimePropInfo =
        IntPropertyBuilder("AcquisitionLoopTime", 20).setUnit(Unit("ms")).setMinValue(10).setMaxValue(1000).build();

    objPtr.addProperty(acqLoopTimePropInfo);
    objPtr.getOnPropertyValueWrite("AcquisitionLoopTime") += [this](PropertyObjectPtr& obj, PropertyValueEventArgsPtr& args) {
        updateAcqLoopTime();
    };

    objPtr.addProperty(BoolProperty("EnableCANChannel", enableCANChannel));
    objPtr.getOnPropertyValueWrite("EnableCANChannel") +=
        [this](PropertyObjectPtr& obj, PropertyValueEventArgsPtr& args) { this->enableCANChannel(); };
}

void WSDA200DeviceImpl::updateNumberOfChannels()
{
    std::size_t num = objPtr.getPropertyValue("NumberOfChannels");
    LOG_I("Properties: NumberOfChannels {}", num);
    auto globalSampleRate = objPtr.getPropertyValue("GlobalSampleRate");

    std::scoped_lock lock(sync);

    if (num < channels.size())
    {
        std::for_each(std::next(channels.begin(), num), channels.end(), [this](const ChannelPtr& ch)
            {
                removeChannel(nullptr, ch);
            });
        channels.erase(std::next(channels.begin(), num), channels.end());
    }

    auto microSecondsSinceDeviceStart = getMicroSecondsSinceDeviceStart();
    for (auto i = channels.size(); i < num; i++)
    {
        WSDA200ChannelInit init{ i, globalSampleRate, microSecondsSinceDeviceStart, microSecondsFromEpochToDeviceStart };
        auto localId = fmt::format("WSDA200Ch{}", i);
        auto ch = createAndAddChannel<WSDA200ChannelImpl>(aiFolder, localId, init);
        channels.push_back(std::move(ch));
    }
}

void WSDA200DeviceImpl::enableCANChannel()
{
    bool enableCANChannel = objPtr.getPropertyValue("EnableCANChannel");

    std::scoped_lock lock(sync);

    if (!enableCANChannel)
    {
        if (canChannel.assigned() && hasChannel(nullptr, canChannel))
            removeChannel(nullptr, canChannel);
        canChannel.release();
    }
    else
    {
        auto microSecondsSinceDeviceStart = getMicroSecondsSinceDeviceStart();
        WSDA200CANChannelInit init{microSecondsSinceDeviceStart, microSecondsFromEpochToDeviceStart};
        canChannel = createAndAddChannel<WSDA200CANChannelImpl>(canFolder, "wsda200:canch", init);
    }
}

void WSDA200DeviceImpl::updateGlobalSampleRate()
{
    auto globalSampleRate = objPtr.getPropertyValue("GlobalSampleRate");
    LOG_I("Properties: GlobalSampleRate {}", globalSampleRate);

    std::scoped_lock lock(sync);

    for (auto& ch : channels)
    {
        auto chPriv = ch.asPtr<IWSDA200Channel>();
        chPriv->globalSampleRateChanged(globalSampleRate);
    }
}

void WSDA200DeviceImpl::updateAcqLoopTime()
{
    Int loopTime = objPtr.getPropertyValue("AcquisitionLoopTime");
    LOG_I("Properties: AcquisitionLoopTime {}", loopTime);

    std::scoped_lock lock(sync);
    this->acqLoopTime = static_cast<size_t>(loopTime);
}

END_NAMESPACE_WSDA_200_DEVICE_MODULE
