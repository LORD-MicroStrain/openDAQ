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
    initMSCL(); 

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

void WSDA200DeviceImpl::initMSCL()
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

void WSDA200DeviceImpl::nodePollAndSelection()
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

            channel_sample_rate = sweeps[k].sampleRate().samples();

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
            channel_sample_rate = sweep.sampleRate().samples();  // establishes sample rate for opendaq
            break;
        }
    
}

void WSDA200DeviceImpl::idleAll()
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

        FloatPropertyBuilder("GlobalSampleRate", channel_sample_rate).setUnit(Unit("Hz")).setMinValue(1.0).setMaxValue(1000000.0).build();
        //FloatPropertyBuilder("GlobalSampleRate", 1000.0).setUnit(Unit("Hz")).setMinValue(1.0).setMaxValue(1000000.0).build();

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
        WSDA200ChannelInit init;
        init.index = i;
        init.globalSampleRate = globalSampleRate;
        init.node_id = node_id; 
        init.node_sample_rate = channel_sample_rate;
        init.startTime = microSecondsSinceDeviceStart;
        init.microSecondsFromEpochToStartTime = microSecondsFromEpochToDeviceStart; 
        //{ i, globalSampleRate, channel_sample_rate, node_id, microSecondsSinceDeviceStart, microSecondsFromEpochToDeviceStart };

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
