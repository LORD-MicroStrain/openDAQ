#include <coretypes/version_info_factory.h>
#include <opendaq/custom_log.h>

#include <wsda_device_module/wsda_device_impl.h>
#include <wsda_device_module/wsda_device_module_impl.h>
#include <wsda_device_module/version.h>

BEGIN_NAMESPACE_WSDA_DEVICE_MODULE

VersionInfoPtr WSDADeviceModule::CreateDeviceModuleVersionInfo()
{
    return VersionInfo(WSDA_DEVICE_MODULE_MAJOR_VERSION, WSDA_DEVICE_MODULE_MINOR_VERSION, WSDA_DEVICE_MODULE_PATCH_VERSION);
}

/* DictPtr<IString, IDeviceType> RefDeviceModule::onGetAvailableDeviceTypes()
{
    auto result = Dict<IString, IDeviceType>();

    auto deviceType = RefDeviceImpl::CreateType();                   Already exists in the file
    result.set(deviceType.getId(), deviceType);
    return result;
}*/

void WSDADeviceModule::readWSDADeviceInfo()
{
    bool success = false;

    /* ::hbk::lanxi::JetPaths jetpath;
    if (jetpath.waitReady(0.001))
    {  // We have already waited in lanxiApp.cpp
        Json::Value productType = jetpath.getProductType();
        m_name = productType["TypeNumber"].asString();
        // Might be empty if eeprom not programmed or if eeprom-image file is missing when simulating
        if (!m_name.empty())
        {
            Json::Value hwVer = jetpath.getHardwareVersion();
            Json::Value swVer = jetpath.getUserFirmwareVersion();

            m_name = productType["TypeNumber"].asString();
            m_model = productType["Model"].asString();
            m_connectorConfiguration = productType["Variant"].asString();                                       UNSURE WHAT THE POINT OF
    THIS SECTION IS m_serialNumber = std::to_string(jetpath.getSerialNumber()); m_macAddress = jetpath.getMacAddress(); m_location =
    jetpath.getLocation(); m_contact = jetpath.getContact(); m_localId = "HBK-" + m_name + "-" + m_model + "-" + m_serialNumber; m_hwVersion
    = hwVer["major"].asString() + "." + hwVer["minor"].asString() + "." + hwVer["build"].asString();  // + "." + hwVer["patch"].asString();
            m_swVersion = getBuildInfo();
            // m_swVersion              = swVer["major"].asString() + "." + swVer["minor"].asString() + "." + swVer["build"].asString();// +
            // "." + swVer["patch"].asString();
            success = true;
        }
    }*/
    if (!success)
    {
        LOG_E("Unable to determine WSDA-200 device type, using 3050 as default device");
        m_name = "WSDA";
        m_localId = "HBK-WSDA-0";
        m_model = "3050";
        m_serialNumber = "0";
        m_macAddress = "00:00:00:00:00:00";
        m_location = "";
        m_contact = "";
        m_hwVersion = "0.0.0.0";
        m_swVersion = "0.0.0";
        m_connectorConfiguration = "060";
    }
}

std::vector<DeviceInfoPtr> WSDADeviceModule::getAvailableDevices()
{
    auto devInfo = DeviceInfo(fmt::format("{}://{}", m_name, m_model));
    devInfo.setName(m_name);
    devInfo.setModel(m_model);
    devInfo.setSerialNumber(m_serialNumber);
    devInfo.setMacAddress(m_macAddress);
    devInfo.setHardwareRevision(m_hwVersion);
    devInfo.setManufacturer("HBK");
    devInfo.setProductCode(m_name + "-" + m_model + "-" + m_connectorConfiguration);
    devInfo.setSoftwareRevision(m_swVersion);
    return {devInfo};
}

//*****************************************************************************
std::string WSDADeviceModule::getBuildInfo()
//*****************************************************************************
// Purpose:  Extracts software build info from sysfs /etc/build
// Returns:  Software build info
// ----------------------------------------------------------------------------
{
    std::string buildInfo;
    /* std::ifstream sysfs("/etc/build");
    if (sysfs.is_open())
    {
        std::string str;
        while (std::getline(sysfs, str))
        {
            std::string::size_type start = str.find("DISTRO_VERSION = ");
            if (start != std::string::npos)
            {
                buildInfo = str.substr(start + 17, start + str.size());  // strlen("DISTRO_VERSION = ") = 17
                break;
            }
        }
        sysfs.close();
    }
    if (buildInfo.empty())
    {
        std::cerr << "Error, cannot obtain software revision info." << std::endl;
    }*/
    return buildInfo;
}

/* DevicePtr RefDeviceModule::onCreateDevice(const StringPtr& connectionString,
                                            const daq::ComponentPtr& parent,
                                            const PropertyObjectPtr& config,
                                            const DeviceInfoPtr& deviceInfo)                    ALREADY IMPLEMENTED
{
    // connect to device
    return Device(context, parent, m_localId, deviceInfo);
}*/

/////////////////////////////////////////////////////////////////////////////////////////// above is accomidating code ///////////////////////////////////////////////////////////////////////////////////////
 
WSDADeviceModule::WSDADeviceModule(ContextPtr context)
    : Module("WSDADeviceModule",
             daq::VersionInfo(WSDA_DEVICE_MODULE_MAJOR_VERSION, WSDA_DEVICE_MODULE_MINOR_VERSION, WSDA_DEVICE_MODULE_PATCH_VERSION),
             std::move(context),
             WSDA_MODULE_NAME)
    , maxNumberOfDevices(1)
{
    auto options = this->context.getModuleOptions(WSDA_MODULE_NAME);
    if (options.hasKey("MaxNumberOfDevices"))
        maxNumberOfDevices = options.get("MaxNumberOfDevices");
    devices.resize(maxNumberOfDevices);
}

ListPtr<IDeviceInfo> WSDADeviceModule::onGetAvailableDevices()
{
    StringPtr serialNumber;

    auto temp = getAvailableDevices(); 

    const auto options = this->context.getModuleOptions(WSDA_MODULE_NAME);

    if (options.assigned())
    {
        if (options.hasKey("SerialNumber"))
            serialNumber = options.get("SerialNumber");
    }

    auto availableDevices = List<IDeviceInfo>();

    if (serialNumber.assigned())
    {
        auto info = WSDADeviceImpl::CreateDeviceInfo(0, serialNumber);
        availableDevices.pushBack(info);
    }
    else
    {
        for (size_t i = 0; i < 2; i++)
        {
            auto info = WSDADeviceImpl::CreateDeviceInfo(i);
            availableDevices.pushBack(info);
        }
    }

    return availableDevices;
}

DictPtr<IString, IDeviceType> WSDADeviceModule::onGetAvailableDeviceTypes()
{
    auto result = Dict<IString, IDeviceType>();

    auto deviceType = WSDADeviceImpl::CreateType();
    result.set(deviceType.getId(), deviceType);

    return result;
}

DevicePtr WSDADeviceModule::onCreateDevice(const StringPtr& connectionString,
                                          const ComponentPtr& parent,
                                          const PropertyObjectPtr& config)
{
    const auto id = getIdFromConnectionString(connectionString);

    std::scoped_lock lock(sync);

    if (id >= devices.size())
    {
        LOG_W("Device with id \"{}\" not found", id);
        throw NotFoundException();
    }

    if (devices[id].assigned() && devices[id].getRef().assigned())
    {
        LOG_W("Device with id \"{}\" already exist", id);
        throw AlreadyExistsException();
    }
    
    const auto options = this->context.getModuleOptions(WSDA_MODULE_NAME);
    StringPtr localId;
    StringPtr name = fmt::format("Device {}", id);

    if (config.assigned())
    {
        if (config.hasProperty("LocalId"))
            localId = config.getPropertyValue("LocalId");
        if (config.hasProperty("Name"))
            name = config.getPropertyValue("Name");
    }

    if (options.assigned())
    {
        if (options.hasKey("LocalId"))
            localId = options.get("LocalId");
        if (options.hasKey("Name"))
            name = options.get("Name");
    }

    if (!localId.assigned())
        localId = fmt::format("WSDADev{}", id);

    auto devicePtr = createWithImplementation<IDevice, WSDADeviceImpl>(id, config, context, parent, localId, name);
    devices[id] = devicePtr;
    return devicePtr;
}

size_t WSDADeviceModule::getIdFromConnectionString(const std::string& connectionString) const
{
    std::string pwsdaixWithDeviceStr = "daqwsda://device";
    auto found = connectionString.find(pwsdaixWithDeviceStr);
    if (found != 0)
    {
        LOG_W("Invalid connection string \"{}\", no pwsdaix", connectionString);
        throw InvalidParameterException();
    }

    auto idStr = connectionString.substr(pwsdaixWithDeviceStr.size(), std::string::npos);
    size_t id;
    try
    {
        id = std::stoi(idStr);
    }
    catch (const std::invalid_argument&)
    {
        LOG_W("Invalid connection string \"{}\", no id", connectionString);
        throw InvalidParameterException();
    }

    return id;
}

END_NAMESPACE_WSDA_DEVICE_MODULE
