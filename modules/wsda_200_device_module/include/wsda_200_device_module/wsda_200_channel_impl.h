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
#include <wsda_200_device_module/common.h>
#include <opendaq/channel_impl.h>
#include <opendaq/signal_config_ptr.h>
#include <optional>
#include <random>
#include <thread>
#include "mscl/mscl.h"

#include "mscl/Types.h"
#include "mscl/MicroStrain/Wireless/WirelessModels.h"
#include "mscl/MicroStrain/Wireless/WirelessNode.h"
#include "mscl/MicroStrain/Wireless/WirelessTypes.h"
#include "mscl/MicroStrain/Wireless/Configuration/WirelessNodeConfig.h"
#include "mscl/MicroStrain/Wireless/SyncNetworkInfo.h"


//#include "mscl/MicroStrain/Wireless/Features/SyncNetworkInfo.h"

BEGIN_NAMESPACE_WSDA_200_DEVICE_MODULE

enum class WaveformType { Sine, Rect, None, Counter, ConstantValue };

DECLARE_OPENDAQ_INTERFACE(IWSDA200Channel, IBaseObject)
{
    virtual void collectSamples(std::chrono::microseconds curTime) = 0;
    virtual void globalSampleRateChanged(double globalSampleRate) = 0;
};

struct WSDA200ChannelInit
{
    size_t index;
    double globalSampleRate;
    int node_sample_rate;
    int node_id;
    int num_signals; 
    mscl::BaseStation* basestation;
    std::chrono::microseconds startTime;
    std::chrono::microseconds microSecondsFromEpochToStartTime;
};

class WSDA200ChannelImpl final : public ChannelImpl<IWSDA200Channel>
{
public:
    explicit WSDA200ChannelImpl(const ContextPtr& context, const ComponentPtr& parent, const StringPtr& localId, const WSDA200ChannelInit& init);

    // IMSCLChannel
    void collectSamples(std::chrono::microseconds curTime) override;
    void globalSampleRateChanged(double newGlobalSampleRate) override;
    static std::string getEpoch();
    static RatioPtr getResolution();
    static void setSampleRate(int rate); 
    std::thread fetchThread;



protected:
    void endApplyProperties(const UpdatingActions& propsAndValues, bool parentUpdating) override;

private:
    /// MSCL/Wireless/////////////////////////////////////

    char comPort[7] = {0,0,0,0,0,0,0};
    int node_id;
    int node_selection;
    const int num_signals; 

    mscl::Connection connection; 
    mscl::BaseStation* basestation;
    mscl::DataSweeps sweeps;

    SignalConfigPtr valueSignal;
    SignalConfigPtr timeSignal;
    SignalConfigPtr channel_1;
    SignalConfigPtr channel_2;
    SignalConfigPtr channel_3;
    SignalConfigPtr* channel_list;

    double** packet_buffers; 

    //////////////////////////////////////////////////////
    WaveformType waveformType;
    double freq;
    double ampl;
    double dc;
    double noiseAmpl;
    double constantValue;
    double sampleRate;
    int node_sample_rate; 
    StructPtr customRange;
    bool clientSideScaling;
    size_t index;
    double globalSampleRate;
    uint64_t counter;
    uint64_t deltaT;
    std::chrono::microseconds startTime;
    std::chrono::microseconds microSecondsFromEpochToStartTime;
    std::chrono::microseconds lastCollectTime;
    uint64_t samplesGenerated;
    std::default_random_engine re;
    std::normal_distribution<double> dist;
    bool needsSignalTypeChanged;
    bool fixedPacketSize;
    uint64_t packetSize;
    std::thread acqThread;

    void initProperties();
    void packetSizeChangedInternal();
    void packetSizeChanged();
    void waveformChanged();
    void waveformChangedInternal();
    void updateSamplesGenerated();
    void signalTypeChanged();
    void signalTypeChangedInternal();
    void resetCounter();
    uint64_t getSamplesSinceStart(std::chrono::microseconds time) const;
    void createSignals();
    //std::tuple<PacketPtr, PacketPtr, PacketPtr, PacketPtr> generateSamples(int64_t curTime, uint64_t samplesGenerated, uint64_t newSamples);
    [[nodiscard]] Int getDeltaT(const double sr) const;
    void buildSignalDescriptors();
    [[nodiscard]] double coerceSampleRate(const double wantedSampleRate) const;
    void signalTypeChangedIfNotUpdating(const PropertyValueEventArgsPtr& args);
    void delay(int counter, int times); 
};

END_NAMESPACE_WSDA_200_DEVICE_MODULE
