/*
 * Copyright 2022-2024 openDAQ d.o.o.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once
#include <wsda_device_module/common.h>
#include <opendaq/module_impl.h>

BEGIN_NAMESPACE_WSDA_DEVICE_MODULE

class WSDADeviceModule final : public Module
{
public:
    explicit WSDADeviceModule(ContextPtr context);
    ListPtr<IDeviceInfo> onGetAvailableDevices() override;
    DictPtr<IString, IDeviceType> onGetAvailableDeviceTypes() override;
    DevicePtr onCreateDevice(const StringPtr& connectionString, const ComponentPtr& parent, const PropertyObjectPtr& config) override;

private:
    std::vector<DeviceInfoPtr> getAvailableDevices();
    VersionInfoPtr CreateDeviceModuleVersionInfo();
    void readWSDADeviceInfo();
    std::string getBuildInfo();

    StringPtr m_name;
    StringPtr m_localId;
    StringPtr m_model;
    StringPtr m_serialNumber;
    StringPtr m_macAddress;
    StringPtr m_location;
    StringPtr m_contact;
    StringPtr m_hwVersion; 
    StringPtr m_swVersion;
    StringPtr m_connectorConfiguration;

    /// ///////////////////////////////////////////////////////////////////////

    std::vector<WeakRefPtr<IDevice>> devices;
    std::mutex sync;
    size_t maxNumberOfDevices;

    size_t getIdFromConnectionString(const std::string& connectionString) const;
};

END_NAMESPACE_WSDA_DEVICE_MODULE
