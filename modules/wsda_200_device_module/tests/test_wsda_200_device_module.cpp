#include <coretypes/common.h>
#include <gmock/gmock.h>
#include <opendaq/config_provider_factory.h>
#include <opendaq/context_factory.h>
#include <opendaq/data_packet_ptr.h>
#include <opendaq/device_ptr.h>
#include <opendaq/event_packet_ids.h>
#include <opendaq/event_packet_params.h>
#include <opendaq/event_packet_ptr.h>
#include <opendaq/input_port_factory.h>
#include <opendaq/module_ptr.h>
#include <opendaq/range_factory.h>
#include <opendaq/reader_factory.h>
#include <opendaq/removable_ptr.h>
#include <opendaq/search_filter_factory.h>
#include <wsda_200_device_module/module_dll.h>
#include <wsda_200_device_module/version.h>
#include <testutils/testutils.h>
#include <thread>
#include "../../../core/opendaq/opendaq/tests/test_config_provider.h"

using namespace daq;
using WSDA200DeviceModuleTest = testing::Test;
using namespace test_config_provider_helpers;
using WSDA200DeviceModuleTestConfig = ConfigProviderTest;

static ModulePtr CreateModule()
{
    ModulePtr module;
    createModule(&module, NullContext());
    return module;
}

TEST_F(WSDA200DeviceModuleTest, CreateModule)
{
    IModule* module = nullptr;
    ErrCode errCode = createModule(&module, NullContext());
    ASSERT_TRUE(OPENDAQ_SUCCEEDED(errCode));

    ASSERT_NE(module, nullptr);
    module->releaseRef();
}

TEST_F(WSDA200DeviceModuleTest, ModuleName)
{
    auto module = CreateModule();
    ASSERT_EQ(module.getName(), "WSDA200DeviceModule");
}

TEST_F(WSDA200DeviceModuleTest, VersionAvailable)
{
    auto module = CreateModule();
    ASSERT_TRUE(module.getVersionInfo().assigned());
}

TEST_F(WSDA200DeviceModuleTest, VersionCorrect)
{
    auto module = CreateModule();
    auto version = module.getVersionInfo();

    ASSERT_EQ(version.getMajor(), WSDA_200_DEVICE_MODULE_MAJOR_VERSION);
    ASSERT_EQ(version.getMinor(), WSDA_200_DEVICE_MODULE_MINOR_VERSION);
    ASSERT_EQ(version.getPatch(), WSDA_200_DEVICE_MODULE_PATCH_VERSION);
}

TEST_F(WSDA200DeviceModuleTest, EnumerateDevices)
{
    auto module = CreateModule();

    ListPtr<IDeviceInfo> deviceInfo;
    ASSERT_NO_THROW(deviceInfo = module.getAvailableDevices());

    ASSERT_EQ(deviceInfo.getCount(), 2u);
    ASSERT_EQ(deviceInfo[0].getConnectionString(), "daqwsda200://device0");
    ASSERT_EQ(deviceInfo[1].getConnectionString(), "daqwsda200://device1");
}

TEST_F(WSDA200DeviceModuleTest, CreateDeviceConnectionStringNull)
{
    auto module = CreateModule();

    DevicePtr device;
    ASSERT_THROW(device = module.createDevice(nullptr, nullptr), ArgumentNullException);
}

TEST_F(WSDA200DeviceModuleTest, CreateDeviceConnectionStringEmpty)
{
    auto module = CreateModule();

    ASSERT_THROW(module.createDevice("", nullptr), InvalidParameterException);
}

TEST_F(WSDA200DeviceModuleTest, CreateDeviceConnectionStringInvalid)
{
    auto module = CreateModule();

    ASSERT_THROW(module.createDevice("fdfdfdfdde", nullptr), InvalidParameterException);
}

TEST_F(WSDA200DeviceModuleTest, CreateDeviceConnectionStringInvalidId)
{
    auto module = CreateModule();

    ASSERT_THROW(module.createDevice("daqwsda200://devicett3axxr1", nullptr), InvalidParameterException);
}

TEST_F(WSDA200DeviceModuleTest, CreateDeviceConnectionStringOutOfRange)
{
    auto module = CreateModule();

    ASSERT_THROW(module.createDevice("daqwsda200://device3", nullptr), NotFoundException);
}

TEST_F(WSDA200DeviceModuleTest, CreateDeviceConnectionStringCorrect)
{
    auto module = CreateModule();

    DevicePtr device;
    ASSERT_NO_THROW(device = module.createDevice("daqwsda200://device1", nullptr));
}

TEST_F(WSDA200DeviceModuleTest, DeviceDomainResolution)
{
    auto module = CreateModule();

    auto device = module.createDevice("daqwsda200://device1", nullptr);
    auto domain = device.getDomain();

    auto res = domain.getTickResolution();
    ASSERT_EQ(res, Ratio(1, 1000000));
}

TEST_F(WSDA200DeviceModuleTest, DeviceDomainUnit)
{
    auto module = CreateModule();

    auto device = module.createDevice("daqwsda200://device1", nullptr);
    auto domain = device.getDomain();

    auto unit = domain.getUnit();
    ASSERT_EQ(unit.getSymbol(), "s");
    ASSERT_EQ(unit.getName(), "second");
    ASSERT_EQ(unit.getQuantity(), "time");
}

TEST_F(WSDA200DeviceModuleTest, DeviceDomainTicksSinceEpoch)
{
    auto module = CreateModule();

    auto device = module.createDevice("daqwsda200://device1", nullptr);

    auto res = device.getTicksSinceOrigin();
    ASSERT_GT(res, 0u);
}

TEST_F(WSDA200DeviceModuleTest, DeviceDomainOrigin)
{
    auto module = CreateModule();

    auto device = module.createDevice("daqwsda200://device1", nullptr);
    auto domain = device.getDomain();

    auto res = domain.getOrigin();
    ASSERT_FALSE(static_cast<std::string>(res).empty());
}

TEST_F(WSDA200DeviceModuleTest, GetAvailableComponentTypes)
{
    const auto module = CreateModule();

    DictPtr<IString, IFunctionBlockType> functionBlockTypes;
    ASSERT_NO_THROW(functionBlockTypes = module.getAvailableFunctionBlockTypes());
    ASSERT_EQ(functionBlockTypes.getCount(), 0u);

    DictPtr<IString, IDeviceType> deviceTypes;
    ASSERT_NO_THROW(deviceTypes = module.getAvailableDeviceTypes());
    ASSERT_EQ(deviceTypes.getCount(), 1u);
    ASSERT_TRUE(deviceTypes.hasKey("daqwsda200"));
    ASSERT_EQ(deviceTypes.get("daqwsda200").getId(), "daqwsda200");

    DictPtr<IString, IServerType> serverTypes;
    ASSERT_NO_THROW(serverTypes = module.getAvailableServerTypes());
    ASSERT_EQ(serverTypes.getCount(), 0u);
}

TEST_F(WSDA200DeviceModuleTest, CreateFunctionBlockIdNull)
{
    auto module = CreateModule();

    FunctionBlockPtr functionBlock;
    ASSERT_THROW(functionBlock = module.createFunctionBlock(nullptr, nullptr, "Id"), ArgumentNullException);
}

TEST_F(WSDA200DeviceModuleTest, CreateFunctionBlockIdEmpty)
{
    auto module = CreateModule();

    ASSERT_THROW(module.createFunctionBlock("", nullptr, "Id"), NotFoundException);
}

TEST_F(WSDA200DeviceModuleTest, DeviceNumberOfChannels)
{
    auto module = CreateModule();

    auto device = module.createDevice("daqwsda200://device1", nullptr);

    Int numChannels = device.getPropertyValue("NumberOfChannels");
    ASSERT_EQ(numChannels, 2);

    ASSERT_EQ(device.getChannels().getCount(), 2u);
}

TEST_F(WSDA200DeviceModuleTest, DeviceChangeNumberOfChannels)
{
    auto module = CreateModule();

    auto device = module.createDevice("daqwsda200://device1", nullptr);

    device.setPropertyValue("NumberOfChannels", 5);
    Int numChannels = device.getPropertyValue("NumberOfChannels");
    ASSERT_EQ(numChannels, 5);
    ASSERT_EQ(device.getChannels().getCount(), 5u);

    device.setPropertyValue("NumberOfChannels", 3);
    numChannels = device.getPropertyValue("NumberOfChannels");
    ASSERT_EQ(numChannels, 3);
    ASSERT_EQ(device.getChannels().getCount(), 3u);
}

TEST_F(WSDA200DeviceModuleTest, DeviceChangeAcqLoopTime)
{
    auto module = CreateModule();

    auto device = module.createDevice("daqwsda200://device1", nullptr);

    device.setPropertyValue("AcquisitionLoopTime", 100);
    Int acqLoopTime = device.getPropertyValue("AcquisitionLoopTime");
    ASSERT_EQ(acqLoopTime, 100);
}

TEST_F(WSDA200DeviceModuleTest, DeviceGlobalSampleRate)
{
    auto module = CreateModule();

    auto device = module.createDevice("daqwsda200://device1", nullptr);

    Float globalSampleRate = device.getPropertyValue("GlobalSampleRate");
    ASSERT_DOUBLE_EQ(globalSampleRate, 1000.0);

    device.setPropertyValue("GlobalSampleRate", 500.0);
    globalSampleRate = device.getPropertyValue("GlobalSampleRate");
    ASSERT_DOUBLE_EQ(globalSampleRate, 500.0);
}

TEST_F(WSDA200DeviceModuleTest, ChannelWaveform)
{
    auto module = CreateModule();
    auto device = module.createDevice("daqwsda200://device1", nullptr);
    auto channels = device.getChannels();
    auto channel = channels[0];

    Int waveform = channel.getPropertyValue("Waveform");
    ASSERT_EQ(waveform, 0);
    channel.setPropertyValue("Waveform", 1);
    waveform = channel.getPropertyValue("Waveform");
    ASSERT_EQ(waveform, 1);
}

TEST_F(WSDA200DeviceModuleTest, ChannelFrequency)
{
    auto module = CreateModule();
    auto device = module.createDevice("daqwsda200://device1", nullptr);
    auto channels = device.getChannels();
    auto channel = channels[0];

    Int frequency = channel.getPropertyValue("Frequency");
    ASSERT_FLOAT_EQ(frequency, 10.0);
    channel.setPropertyValue("Frequency", 100.0);
    frequency = channel.getPropertyValue("Frequency");
    ASSERT_FLOAT_EQ(frequency, 100.0);
}

TEST_F(WSDA200DeviceModuleTest, ChannelDC)
{
    auto module = CreateModule();
    auto device = module.createDevice("daqwsda200://device1", nullptr);
    auto channels = device.getChannels();
    auto channel = channels[0];

    Int dc = channel.getPropertyValue("DC");
    ASSERT_FLOAT_EQ(dc, 0.0);
    channel.setPropertyValue("DC", 1.0);
    dc = channel.getPropertyValue("DC");
    ASSERT_FLOAT_EQ(dc, 1.0);
}

TEST_F(WSDA200DeviceModuleTest, ChannelAmplitude)
{
    auto module = CreateModule();
    auto device = module.createDevice("daqwsda200://device1", nullptr);
    auto channels = device.getChannels();
    auto channel = channels[0];

    Int amplitude = channel.getPropertyValue("Amplitude");
    ASSERT_FLOAT_EQ(amplitude, 5.0);
    channel.setPropertyValue("Amplitude", 6.0);
    amplitude = channel.getPropertyValue("Amplitude");
    ASSERT_FLOAT_EQ(amplitude, 6.0);
}

TEST_F(WSDA200DeviceModuleTest, ChannelName)
{
    auto module = CreateModule();
    auto device = module.createDevice("daqwsda200://device1", nullptr);
    auto channels = device.getChannels();

    size_t i = 0;
    for (const auto& ch : channels)
    {
        std::string chName = ch.getFunctionBlockType().getName();
        ASSERT_EQ(chName, fmt::format("AI{}", ++i));
    }
}

TEST_F(WSDA200DeviceModuleTest, ChannelNoiseAmplitude)
{
    auto module = CreateModule();
    auto device = module.createDevice("daqwsda200://device1", nullptr);
    auto channels = device.getChannels();
    auto channel = channels[0];

    Int noiseAmpl = channel.getPropertyValue("NoiseAmplitude");
    ASSERT_FLOAT_EQ(noiseAmpl, 0.0);
    channel.setPropertyValue("NoiseAmplitude", 1.0);
    noiseAmpl = channel.getPropertyValue("NoiseAmplitude");
    ASSERT_FLOAT_EQ(noiseAmpl, 1.0);
}

TEST_F(WSDA200DeviceModuleTest, ChannelCustomRange)
{
    auto module = CreateModule();
    auto device = module.createDevice("daqwsda200://device1", nullptr);
    auto channel = device.getChannels()[0];
    auto signal = channel.getSignals()[0];

    auto desc = signal.getDescriptor();
    ASSERT_EQ(desc.getValueRange().getHighValue(), 10.0);
    ASSERT_EQ(desc.getValueRange().getLowValue(), -10.0);

    channel.setPropertyValue("CustomRange", Range(-5.0, 5.0));

    desc = signal.getDescriptor();
    ASSERT_EQ(desc.getValueRange().getHighValue(), 5.0);
    ASSERT_EQ(desc.getValueRange().getLowValue(), -5.0);
}

TEST_F(WSDA200DeviceModuleTest, ChannelSampleRate)
{
    auto module = CreateModule();
    auto device = module.createDevice("daqwsda200://device1", nullptr);
    auto channels = device.getChannels();
    auto channel = channels[0];

    channel.setPropertyValue("SampleRate", 10000.0);

    ASSERT_FALSE(channel.getProperty("SampleRate").getVisible());
    channel.setPropertyValue("UseGlobalSampleRate", False);
    auto sampleRate = channel.getPropertyValue("SampleRate");
    ASSERT_DOUBLE_EQ(sampleRate, 10000.0);

    ASSERT_TRUE(channel.getProperty("SampleRate").getVisible());
}

TEST_F(WSDA200DeviceModuleTest, CoerceChannelSampleRate)
{
    auto module = CreateModule();
    auto device = module.createDevice("daqwsda200://device1", nullptr);
    auto channels = device.getChannels();
    auto channel = channels[0];

    device.setPropertyValue("GlobalSampleRate", 49999);
    double sampleRate = channel.getPropertyValue("SampleRate");
    ASSERT_DOUBLE_EQ(sampleRate, 50000.0);
}

TEST_F(WSDA200DeviceModuleTest, Ids)
{
    auto module = CreateModule();
    auto device = module.createDevice("daqwsda200://device1", nullptr);
    auto channels = device.getChannels();
    auto channel = channels[0];
    auto valueSignal = channel.getSignals()[0];
    auto domainSignal = channel.getSignals(search::Any())[1];

    ASSERT_EQ(channel.getLocalId(), "WSDA200Ch0");
    ASSERT_EQ(channel.getGlobalId(), "/WSDA200Dev1/IO/AI/WSDA200Ch0");

    ASSERT_EQ(valueSignal.getLocalId(), "AI0");
    ASSERT_EQ(valueSignal.getGlobalId(), "/WSDA200Dev1/IO/AI/WSDA200Ch0/Sig/AI0");

    ASSERT_EQ(domainSignal.getLocalId(), "AI0Time");
    ASSERT_EQ(domainSignal.getGlobalId(), "/WSDA200Dev1/IO/AI/WSDA200Ch0/Sig/AI0Time");
}

bool propertyInfoListContainsProperty(const ListPtr<IProperty>& list, const std::string& propName)
{
    auto it = std::find_if(list.begin(), list.end(), [propName](const PropertyPtr& prop) { return prop.getName() == propName; });

    return it != list.end();
}

bool propertyInfoListDoesntContainProperty(const ListPtr<IProperty>& list, const std::string& propName)
{
    return !propertyInfoListContainsProperty(list, propName);
}

TEST_F(WSDA200DeviceModuleTest, ChannelProperties)
{
    auto module = CreateModule();
    auto device = module.createDevice("daqwsda200://device1", nullptr);
    auto channels = device.getChannels();
    auto channel = channels[0];

    channel.setPropertyValue("Waveform", 0);
    auto visibleProps = channel.getVisibleProperties();
    ASSERT_PRED2(propertyInfoListContainsProperty, visibleProps, "Amplitude");

    channel.setPropertyValue("Waveform", 3);
    visibleProps = channel.getVisibleProperties();
    ASSERT_PRED2(propertyInfoListDoesntContainProperty, visibleProps, "Amplitude");
}

TEST_F(WSDA200DeviceModuleTest, SignalCheck)
{
    auto module = CreateModule();
    auto device = module.createDevice("daqwsda200://device1", nullptr);
    auto channels = device.getChannels();
    auto channel = channels[0];
    auto signals = channel.getSignals(search::Any());
    ASSERT_EQ(signals.getCount(), 2u);
}

TEST_F(WSDA200DeviceModuleTest, DeviceRemoveDisconnectsInputPort)
{
    auto module = CreateModule();
    auto device = module.createDevice("daqwsda200://device1", nullptr);
    auto channels = device.getChannels();
    auto channel = channels[0];
    auto signals = channel.getSignals();

    auto inputPort = InputPort(NullContext(), nullptr, "Input");
    inputPort.connect(signals[0]);

    ASSERT_EQ(inputPort.getSignal(), signals[0]);

    device.asPtr<IRemovable>().remove();
    ASSERT_EQ(inputPort.getSignal(), nullptr);
}

TEST_F(WSDA200DeviceModuleTest, ChannelRemovedDisconnectsInputPort)
{
    auto module = CreateModule();
    auto device = module.createDevice("daqwsda200://device1", nullptr);
    auto channels = device.getChannels();
    auto channel = channels[1];
    auto signals = channel.getSignals();

    auto inputPort = InputPort(NullContext(), nullptr, "Input");
    inputPort.connect(signals[0]);

    ASSERT_EQ(inputPort.getSignal(), signals[0]);

    device.setPropertyValue("NumberOfChannels", 1);
    ASSERT_EQ(inputPort.getSignal(), nullptr);
}

TEST_F(WSDA200DeviceModuleTest, CreateDeviceTwice)
{
    auto module = CreateModule();

    DevicePtr device;
    ASSERT_NO_THROW(device = module.createDevice("daqwsda200://device1", nullptr));
    ASSERT_THROW(module.createDevice("daqwsda200://device1", nullptr), AlreadyExistsException);
}

TEST_F(WSDA200DeviceModuleTest, CreateReleaseAndCreateDevice)
{
    auto module = CreateModule();

    DevicePtr device;
    ASSERT_NO_THROW(device = module.createDevice("daqwsda200://device1", nullptr));
    device.release();
    ASSERT_NO_THROW(module.createDevice("daqwsda200://device1", nullptr));
}

TEST_F(WSDA200DeviceModuleTest, Folders)
{
    auto module = CreateModule();
    auto device = module.createDevice("daqwsda200://device1", nullptr);
    FolderPtr ioFolder = device.getItem("IO");
    FolderPtr aiFolder = ioFolder.getItem("AI");
    ChannelPtr chX = aiFolder.getItems()[0];

    auto channels = device.getChannels();
    auto chY = channels[0];

    ASSERT_EQ(chX, chY);
}

TEST_F(WSDA200DeviceModuleTest, Sync)
{
    auto module = CreateModule();
    auto device = module.createDevice("daqwsda200://device1", nullptr);
    ComponentPtr syncComponent = device.getItem("sync");

    ASSERT_FALSE(syncComponent.getPropertyValue("UseSync"));
    syncComponent.setPropertyValue("UseSync", True);
    ASSERT_TRUE(syncComponent.getPropertyValue("UseSync"));
}

TEST_F(WSDA200DeviceModuleTest, Serialize)
{
    auto module = CreateModule();
    auto device = module.createDevice("daqwsda200://device1", nullptr);

    device.setPropertyValue("NumberOfChannels", 5);
    device.setPropertyValue("GlobalSampleRate", 500.0);

    auto serializer = JsonSerializer(True);

    device.serialize(serializer);

    auto str = serializer.getOutput();
    std::cout << str << std::endl;
}

TEST_F(WSDA200DeviceModuleTest, DeviceEnableCANChannel)
{
    auto module = CreateModule();

    auto device = module.createDevice("daqwsda200://device1", nullptr);

    Int numChannels = device.getPropertyValue("NumberOfChannels");
    ASSERT_EQ(numChannels, 2);

    ASSERT_EQ(device.getChannels().getCount(), 2u);

    device.setPropertyValue("EnableCANChannel", True);

    ASSERT_EQ(device.getChannels().getCount(), 3u);
}

TEST_F(WSDA200DeviceModuleTest, ReadCANChannel)
{
    auto module = CreateModule();

    auto device = module.createDevice("daqwsda200://device1", nullptr);

    device.setPropertyValue("EnableCANChannel", True);

    const ChannelPtr canCh = device.getInputsOutputsFolder().getItem("CAN").asPtr<IFolder>().getItems()[0];

    const auto canSignal = canCh.getSignals()[0];
    const auto canTimeSignal = canSignal.getDomainSignal();
    const auto packetReader = PacketReader(canSignal);

    while (packetReader.getAvailableCount() < 2u)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

    EventPacketPtr eventPacket = packetReader.read();
    ASSERT_EQ(eventPacket.getEventId(), event_packet_id::DATA_DESCRIPTOR_CHANGED);

    const DataDescriptorPtr dataDesc = eventPacket.getParameters().get(event_packet_param::DATA_DESCRIPTOR);
    const DataDescriptorPtr domainDesc = eventPacket.getParameters().get(event_packet_param::DOMAIN_DATA_DESCRIPTOR);
    ASSERT_EQ(dataDesc, canSignal.getDescriptor());
    ASSERT_EQ(domainDesc, canTimeSignal.getDescriptor());

    ASSERT_GT(dataDesc.getStructFields().getCount(), 0u);

    DataPacketPtr dataPacket = packetReader.read();
    ASSERT_EQ(dataPacket.getDataDescriptor(), dataDesc);

    const auto sampleCount = dataPacket.getSampleCount();
    ASSERT_GE(sampleCount, 1u);

#pragma pack(push, 1)
    struct CANData
    {
        uint32_t arbId;
        uint8_t length;
        uint8_t data[64];
    };
#pragma pack(pop)

    auto* canData = reinterpret_cast<CANData*>(dataPacket.getData());
    for (size_t i = 0; i < sampleCount; i++)
    {
        ASSERT_EQ(canData->arbId, (uint32_t) 12);
        ASSERT_EQ(canData->length, (uint8_t) 8);
        canData++;
    }
}

TEST_F(WSDA200DeviceModuleTest, ReadCANChannelWithStreamReader)
{
    const auto module = CreateModule();

    const auto device = module.createDevice("daqwsda200://device1", nullptr);

    device.setPropertyValue("EnableCANChannel", True);

    const ChannelPtr canCh = device.getInputsOutputsFolder().getItem("CAN").asPtr<IFolder>().getItems()[0];

    const auto canSignal = canCh.getSignals()[0];
    const auto canTimeSignal = canSignal.getDomainSignal();
    const auto streamReader = StreamReader<void*>(canSignal);

#pragma pack(push, 1)
    struct CANData
    {
        uint32_t arbId;
        uint8_t length;
        uint8_t data[64];
    };
#pragma pack(pop)

    CANData canData[10];
    SizeT count = 0;
    do
    {
        SizeT toRead = 2;
        streamReader.read(&canData[count], &toRead, 10);
        count += toRead;
    } while (count < 2);

    for (size_t i = 0; i < count; i++)
    {
        ASSERT_EQ(canData[i].arbId, (uint32_t) 12);
        ASSERT_EQ(canData[i].length, (uint8_t) 8);
    }
}

TEST_F(WSDA200DeviceModuleTest, ReadAIChannelWithFixedPacketSize)
{
    const auto module = CreateModule();

    constexpr SizeT packetSize = 1000;

    const auto device = module.createDevice("daqwsda200://device1", nullptr);
    device.setPropertyValue("GlobalSampleRate", 1000);

    const auto channel = device.getChannels()[0];
    channel.setPropertyValue("FixedPacketSize", True);
    channel.setPropertyValue("PacketSize", packetSize);

    const auto signal = channel.getSignals()[0];
    const auto domainSignal = signal.getDomainSignal();
    const auto packetReader = PacketReader(signal);

    while (packetReader.getAvailableCount() < 1u)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

    const EventPacketPtr eventPacket = packetReader.read();
    ASSERT_EQ(eventPacket.getEventId(), event_packet_id::DATA_DESCRIPTOR_CHANGED);

    const DataDescriptorPtr dataDesc = eventPacket.getParameters().get(event_packet_param::DATA_DESCRIPTOR);
    const DataDescriptorPtr domainDesc = eventPacket.getParameters().get(event_packet_param::DOMAIN_DATA_DESCRIPTOR);
    ASSERT_EQ(dataDesc, signal.getDescriptor());
    ASSERT_EQ(domainDesc, domainSignal.getDescriptor());

    // there might be old packets in the signal path, so exit when we encounter the first with packet size
    for (;;)
    {
        while (packetReader.getAvailableCount() < 1u)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));

        const DataPacketPtr dataPacket = packetReader.read();
        ASSERT_EQ(dataPacket.getDataDescriptor(), dataDesc);

        const auto sampleCount = dataPacket.getSampleCount();
        if (sampleCount == packetSize)
            break;
    };
}

TEST_F(WSDA200DeviceModuleTest, ReadConstantRule)
{
    auto module = CreateModule();

    auto device = module.createDevice("daqwsda200://device1", nullptr);

    const ChannelPtr ch = device.getChannels()[0];
    ch.setPropertyValue("ConstantValue", 4.0);
    ch.setPropertyValue("Waveform", 4);
    const auto signal = ch.getSignals()[0];

    const auto packetReader = PacketReader(signal);
    while (true)
    {
        const auto packet = packetReader.read();
        if (packet.assigned() && packet.getType() == PacketType::Data &&
            packet.asPtr<IDataPacket>(true).getDataDescriptor().getRule().getType() == DataRuleType::Constant)
        {
            DataPacketPtr dataPacket = packet;
            const auto data = reinterpret_cast<double*>(dataPacket.getData());
            ASSERT_EQ(*data, 4.0);
            break;
        }
        else
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

TEST_F(WSDA200DeviceModuleTestConfig, JsonConfigReadWSDA200DeviceLocalId)
{
    std::string filename = "test.json";
    std::string json = "{ \"Modules\": { \"WSDA200Device\": { \"LocalId\": \"testtest\" } } }";
    createConfigFile(filename, json);

    auto options = GetOptionsWithReferenceDevice();

    auto expectedOptions = GetOptionsWithReferenceDevice();
    getChildren(getChildren(expectedOptions, "Modules"), "WSDA200Device").set("LocalId", "testtest");

    auto provider = JsonConfigProvider(StringPtr(filename));
    provider.populateOptions(options);

    ASSERT_EQ(options, expectedOptions);
}
TEST_F(WSDA200DeviceModuleTestConfig, DeviceModuleJsonConfigLocalId)
{
    std::string filename = "test.json";
    std::string json = "{ \"Modules\": { \"WSDA200Device\": { \"LocalId\": \"testtest\" } } }";
    createConfigFile(filename, json);

    auto options = GetDefaultOptions();

    auto provider = JsonConfigProvider(StringPtr(filename));
    provider.populateOptions(options);

    auto context = NullContext(Logger(), TypeManager(), options);

    ModulePtr module;
    createModule(&module, context);

    DevicePtr ptr;
    ASSERT_NO_THROW(ptr = module.createDevice("daqwsda200://device1", nullptr));
    ASSERT_EQ(ptr.getLocalId(), "testtest");
}

TEST_F(WSDA200DeviceModuleTestConfig, DeviceModuleJsonConfigName)
{
    std::string filename = "test.json";
    std::string json = "{ \"Modules\": { \"WSDA200Device\": { \"Name\": \"testname\" } } }";
    createConfigFile(filename, json);

    auto options = GetDefaultOptions();

    auto provider = JsonConfigProvider(StringPtr(filename));
    provider.populateOptions(options);

    auto context = NullContext(Logger(), TypeManager(), options);

    ModulePtr module;
    createModule(&module, context);

    DevicePtr ptr;
    ASSERT_NO_THROW(ptr = module.createDevice("daqwsda200://device1", nullptr));
    ASSERT_EQ(ptr.getName(), "testname");
}

TEST_F(WSDA200DeviceModuleTestConfig, DeviceModuleJsonConfigLocalIdAndName)
{
    std::string filename = "test.json";
    std::string json = "{ \"Modules\": { \"WSDA200Device\": { \"LocalId\": \"testtest\", \"Name\": \"testname\" } } }";
    createConfigFile(filename, json);

    auto options = GetDefaultOptions();

    auto provider = JsonConfigProvider(StringPtr(filename));
    provider.populateOptions(options);

    auto context = NullContext(Logger(), TypeManager(), options);

    ModulePtr module;
    createModule(&module, context);

    DevicePtr ptr;
    ASSERT_NO_THROW(ptr = module.createDevice("daqwsda200://device1", nullptr));
    ASSERT_EQ(ptr.getLocalId(), "testtest");
    ASSERT_EQ(ptr.getName(), "testname");
}

TEST_F(WSDA200DeviceModuleTestConfig, DeviceModuleJsonConfigMalformed)
{
    std::string filename = "test.json";
    std::string json = "{ \"Modules\": { \"WSDA200Device\": { \"LocalId\": { \"Error\": \"testtest\" } } } }";
    createConfigFile(filename, json);

    auto options = GetDefaultOptions();

    auto provider = JsonConfigProvider(StringPtr(filename));
    provider.populateOptions(options);

    auto context = NullContext(Logger(), TypeManager(), options);

    ModulePtr module;
    createModule(&module, context);

    ASSERT_THROW(module.createDevice("daqwsda200://device1", nullptr), NoInterfaceException);
}

TEST_F(WSDA200DeviceModuleTestConfig, DeviceModuleJsonConfigDefault)
{
    auto options = GetDefaultOptions();

    auto context = NullContext(Logger(), TypeManager(), options);

    ModulePtr module;
    createModule(&module, context);

    DevicePtr ptr;
    ASSERT_NO_THROW(ptr = module.createDevice("daqwsda200://device1", nullptr));
    ASSERT_EQ(ptr.getLocalId(), "WSDA200Dev1");
}

TEST_F(WSDA200DeviceModuleTestConfig, DeviceModuleJsonConfigNoOptions)
{
    auto module = CreateModule();

    DevicePtr ptr;
    ASSERT_NO_THROW(ptr = module.createDevice("daqwsda200://device1", nullptr));
    ASSERT_EQ(ptr.getLocalId(), "WSDA200Dev1");
}

TEST_F(WSDA200DeviceModuleTestConfig, DeviceModuleJsonConfigEmptyString)
{
    auto options = GetOptionsWithReferenceDevice();

    auto context = NullContext(Logger(), TypeManager(), options);

    ModulePtr module;
    createModule(&module, context);

    DevicePtr ptr;
    ASSERT_THROW(ptr = module.createDevice("daqwsda200://device1", nullptr), GeneralErrorException);
}
