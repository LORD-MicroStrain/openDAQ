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
#include <opendaq/channel_ptr.h>
#include <opendaq/device_impl.h>
#include <opendaq/logger_ptr.h>
#include <opendaq/logger_component_ptr.h>
#include <thread>
#include <condition_variable>
#include "mscl/mscl.h"

BEGIN_NAMESPACE_WSDA_DEVICE_MODULE

class WSDADeviceImpl final : public Device
{
public:
    explicit WSDADeviceImpl(size_t id, const PropertyObjectPtr& config, const ContextPtr& ctx, const ComponentPtr& parent, const StringPtr& localId, const StringPtr& name = nullptr);
    ~WSDADeviceImpl() override;

    static DeviceInfoPtr CreateDeviceInfo(size_t id, const StringPtr& serialNumber = nullptr);
    static DeviceTypePtr CreateType();

    // IDevice
    DeviceInfoPtr onGetInfo() override;
    uint64_t onGetTicksSinceOrigin() override;

    bool allowAddDevicesFromModules() override;
    bool allowAddFunctionBlocksFromModules() override;



private:
    // MSCL / Wireless /////////////////////////

    char comPort[7] = {0,0,0,0,0,0,0};
    int node_id;
    int node_selection;
    int channel_sample_rate;
    int num_signals;

    mscl::BaseStation* basestation;
    mscl::DataSweeps sweeps; 
    mscl::Connection connection;

    void initMSCL();
    void nodePollAndSelection();
    void nodeSearch(); 
    void idleAll();
    void delay(int counter, int times); 

    /// ////////////////////////////////////////
    void initClock();
    void initIoFolder();
    void initSyncComponent();
    void initProperties(const PropertyObjectPtr& config);
    void acqLoop();
    void updateNumberOfChannels();
    void enableCANChannel();
    void updateAcqLoopTime();
    void updateGlobalSampleRate();
    std::chrono::microseconds getMicroSecondsSinceDeviceStart() const;

     //void hello();

    size_t id;
    StringPtr serialNumber;

    std::thread acqThread;
    std::thread acqThread2;
    std::condition_variable cv;

    std::chrono::steady_clock::time_point startTime;
    std::chrono::microseconds microSecondsFromEpochToDeviceStart;

    std::vector<ChannelPtr> channels;
    ChannelPtr canChannel;
    size_t acqLoopTime;
    bool stopAcq;

    FolderConfigPtr aiFolder;
    FolderConfigPtr canFolder;
    ComponentPtr syncComponent;

    LoggerPtr logger;
    LoggerComponentPtr loggerComponent;
};

END_NAMESPACE_WSDA_DEVICE_MODULE
