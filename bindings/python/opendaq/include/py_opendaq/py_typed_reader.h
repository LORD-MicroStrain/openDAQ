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

#include <cstddef>
#include <type_traits>

#include "coretypes/exceptions.h"
#include "opendaq/block_reader_ptr.h"
#include "opendaq/multi_reader_ptr.h"
#include "opendaq/reader_config_ptr.h"
#include "opendaq/reader_status_ptr.h"
#include "opendaq/time_reader.h"
#include "py_core_types/py_converter.h"

#include <pybind11/chrono.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "py_opendaq/py_reader_traits.h"

using SampleTypeVariant = std::variant<py::array_t<daq::SampleTypeToType<daq::SampleType::Float32>::Type>,
                                       py::array_t<daq::SampleTypeToType<daq::SampleType::Float64>::Type>,
                                       py::array_t<daq::SampleTypeToType<daq::SampleType::UInt32>::Type>,
                                       py::array_t<daq::SampleTypeToType<daq::SampleType::Int32>::Type>,
                                       py::array_t<daq::SampleTypeToType<daq::SampleType::UInt64>::Type>,
                                       py::array_t<daq::SampleTypeToType<daq::SampleType::Int64>::Type>,
                                       py::array_t<daq::SampleTypeToType<daq::SampleType::UInt8>::Type>,
                                       py::array_t<daq::SampleTypeToType<daq::SampleType::Int8>::Type>,
                                       py::array_t<daq::SampleTypeToType<daq::SampleType::UInt16>::Type>,
                                       py::array_t<daq::SampleTypeToType<daq::SampleType::Int16>::Type>>;

using DomainTypeVariant = SampleTypeVariant;

struct PyTypedReader
{
    template <typename ReaderType>
    static inline typename daq::ReaderStatusType<ReaderType>::Type readZeroValues(const ReaderType& reader, size_t timeoutMs)
    {
        using StatusType = typename daq::ReaderStatusType<ReaderType>::Type;
        StatusType status;
        size_t tmpCount = 0;
        if constexpr (ReaderHasReadWithTimeout<ReaderType, void>::value)
        {
            reader->read(nullptr, &tmpCount, timeoutMs, &status);
        }
        else
        {
            reader->read(nullptr, &tmpCount, &status);
        }
        return status;
    }

    template <typename ReaderType>
    static inline std::tuple<SampleTypeVariant, typename daq::ReaderStatusType<ReaderType>::IType*> readValues(const ReaderType& reader, size_t count, size_t timeoutMs)
    {
        if (count == 0)
        {
            auto status = readZeroValues(reader, timeoutMs);
            return {SampleTypeVariant{}, status.detach()};
        }

        daq::SampleType valueType = daq::SampleType::Undefined;
        reader->getValueReadType(&valueType);
        switch (valueType)
        {
            case daq::SampleType::Float32:
                return read<daq::SampleTypeToType<daq::SampleType::Float32>::Type>(reader, count, timeoutMs);
            case daq::SampleType::Float64:
                return read<daq::SampleTypeToType<daq::SampleType::Float64>::Type>(reader, count, timeoutMs);
            case daq::SampleType::UInt32:
                return read<daq::SampleTypeToType<daq::SampleType::UInt32>::Type>(reader, count, timeoutMs);
            case daq::SampleType::Int32:
                return read<daq::SampleTypeToType<daq::SampleType::Int32>::Type>(reader, count, timeoutMs);
            case daq::SampleType::UInt64:
                return read<daq::SampleTypeToType<daq::SampleType::UInt64>::Type>(reader, count, timeoutMs);
            case daq::SampleType::Int64:
                return read<daq::SampleTypeToType<daq::SampleType::Int64>::Type>(reader, count, timeoutMs);
            case daq::SampleType::UInt8:
                return read<daq::SampleTypeToType<daq::SampleType::UInt8>::Type>(reader, count, timeoutMs);
            case daq::SampleType::Int8:
                return read<daq::SampleTypeToType<daq::SampleType::Int8>::Type>(reader, count, timeoutMs);
            case daq::SampleType::UInt16:
                return read<daq::SampleTypeToType<daq::SampleType::UInt16>::Type>(reader, count, timeoutMs);
            case daq::SampleType::Int16:
                return read<daq::SampleTypeToType<daq::SampleType::Int16>::Type>(reader, count, timeoutMs);
            case daq::SampleType::RangeInt64:
            case daq::SampleType::ComplexFloat64:
            case daq::SampleType::ComplexFloat32:
            case daq::SampleType::Undefined:
            case daq::SampleType::Binary:
            case daq::SampleType::String:
            default:
                throw std::runtime_error("Unsupported values sample type: " + convertSampleTypeToString(valueType));
        }
    }

    template <typename ReaderType>
    static inline std::tuple<SampleTypeVariant, DomainTypeVariant, typename daq::ReaderStatusType<ReaderType>::IType*> readValuesWithDomain(const ReaderType& reader,
                                                                                                             size_t count,
                                                                                                             size_t timeoutMs)
    {
        if (count == 0)
        {
            auto status = readZeroValues(reader, timeoutMs);
            return {SampleTypeVariant{}, DomainTypeVariant{}, status.detach()};
        }

        daq::SampleType valueType = daq::SampleType::Undefined;
        reader->getValueReadType(&valueType);

        switch (valueType)
        {
            case daq::SampleType::Float32:
                return readWithDomain<daq::SampleTypeToType<daq::SampleType::Float32>::Type>(reader, count, timeoutMs);
            case daq::SampleType::Float64:
                return readWithDomain<daq::SampleTypeToType<daq::SampleType::Float64>::Type>(reader, count, timeoutMs);
            case daq::SampleType::UInt32:
                return readWithDomain<daq::SampleTypeToType<daq::SampleType::UInt32>::Type>(reader, count, timeoutMs);
            case daq::SampleType::Int32:
                return readWithDomain<daq::SampleTypeToType<daq::SampleType::Int32>::Type>(reader, count, timeoutMs);
            case daq::SampleType::UInt64:
                return readWithDomain<daq::SampleTypeToType<daq::SampleType::UInt64>::Type>(reader, count, timeoutMs);
            case daq::SampleType::Int64:
                return readWithDomain<daq::SampleTypeToType<daq::SampleType::Int64>::Type>(reader, count, timeoutMs);
            case daq::SampleType::UInt8:
                return readWithDomain<daq::SampleTypeToType<daq::SampleType::UInt8>::Type>(reader, count, timeoutMs);
            case daq::SampleType::Int8:
                return readWithDomain<daq::SampleTypeToType<daq::SampleType::Int8>::Type>(reader, count, timeoutMs);
            case daq::SampleType::UInt16:
                return readWithDomain<daq::SampleTypeToType<daq::SampleType::UInt16>::Type>(reader, count, timeoutMs);
            case daq::SampleType::Int16:
                return readWithDomain<daq::SampleTypeToType<daq::SampleType::Int16>::Type>(reader, count, timeoutMs);
            case daq::SampleType::RangeInt64:
            case daq::SampleType::ComplexFloat64:
            case daq::SampleType::ComplexFloat32:
            case daq::SampleType::Undefined:
            case daq::SampleType::Binary:
            case daq::SampleType::String:
            default:
                throw std::runtime_error("Unsupported values sample type: " + convertSampleTypeToString(valueType));
        }
    }

    static inline void checkTypes(daq::SampleType valueType, daq::SampleType domainType)
    {
        checkSampleType(valueType);
        checkSampleType(domainType);
    }

private:
    template <typename ValueType, typename ReaderType>
    static inline std::tuple<SampleTypeVariant, DomainTypeVariant, typename daq::ReaderStatusType<ReaderType>::IType*> readWithDomain(const ReaderType& reader,
                                                                                                       size_t count,
                                                                                                       size_t timeoutMs)
    {
        if constexpr (std::is_base_of<daq::TimeReaderBase, ReaderType>::value)
        {
            return read<ValueType, std::chrono::system_clock::time_point>(reader, count, timeoutMs);
        }
        else
        {
            daq::SampleType domainType = daq::SampleType::Undefined;
            reader->getDomainReadType(&domainType);
            switch (domainType)
            {
                case daq::SampleType::Float32:
                    return read<ValueType, daq::SampleTypeToType<daq::SampleType::Float32>::Type>(reader, count, timeoutMs);
                case daq::SampleType::Float64:
                    return read<ValueType, daq::SampleTypeToType<daq::SampleType::Float64>::Type>(reader, count, timeoutMs);
                case daq::SampleType::UInt32:
                    return read<ValueType, daq::SampleTypeToType<daq::SampleType::UInt32>::Type>(reader, count, timeoutMs);
                case daq::SampleType::Int32:
                    return read<ValueType, daq::SampleTypeToType<daq::SampleType::Int32>::Type>(reader, count, timeoutMs);
                case daq::SampleType::UInt64:
                    return read<ValueType, daq::SampleTypeToType<daq::SampleType::UInt64>::Type>(reader, count, timeoutMs);
                case daq::SampleType::Int64:
                    return read<ValueType, daq::SampleTypeToType<daq::SampleType::Int64>::Type>(reader, count, timeoutMs);
                case daq::SampleType::UInt8:
                    return read<ValueType, daq::SampleTypeToType<daq::SampleType::UInt8>::Type>(reader, count, timeoutMs);
                case daq::SampleType::Int8:
                    return read<ValueType, daq::SampleTypeToType<daq::SampleType::Int8>::Type>(reader, count, timeoutMs);
                case daq::SampleType::UInt16:
                    return read<ValueType, daq::SampleTypeToType<daq::SampleType::UInt16>::Type>(reader, count, timeoutMs);
                case daq::SampleType::Int16:
                    return read<ValueType, daq::SampleTypeToType<daq::SampleType::Int16>::Type>(reader, count, timeoutMs);
                case daq::SampleType::RangeInt64:
                case daq::SampleType::ComplexFloat64:
                case daq::SampleType::ComplexFloat32:
                case daq::SampleType::Undefined:
                case daq::SampleType::Binary:
                case daq::SampleType::String:
                default:
                    throw std::runtime_error("Unsupported domain sample type: " + convertSampleTypeToString(domainType));
            }
        }
    }

    template <typename ValueType, typename ReaderType>
    static inline std::tuple<SampleTypeVariant, typename daq::ReaderStatusType<ReaderType>::IType*> read(const ReaderType& reader,
                                                                          size_t count,
                                                                          [[maybe_unused]] size_t timeoutMs)
    {
        size_t blockSize = 1, initialCount = count;
        constexpr const bool isMultiReader = std::is_base_of_v<daq::MultiReaderPtr, ReaderType>;

        if constexpr (std::is_same_v<ReaderType, daq::BlockReaderPtr>)
        {
            reader->getBlockSize(&blockSize);
        }
        if constexpr (isMultiReader)
        {
            daq::ReaderConfigPtr readerConfig = reader.template asPtr<daq::IReaderConfig>();
            blockSize = readerConfig.getInputPorts().getCount();
        }

        using StatusType = typename daq::ReaderStatusType<ReaderType>::Type;
        StatusType status;
        std::vector<ValueType> values(count * blockSize);
        if constexpr (ReaderHasReadWithTimeout<ReaderType, ValueType>::value)
        {
            if constexpr (isMultiReader)
            {
                std::vector<void*> ptrs(blockSize);
                for (size_t i = 0; i < blockSize; i++)
                {
                    ptrs[i] = values.data() + i * count;
                }
                reader->read(ptrs.data(), &count, timeoutMs, &status);
            }
            else
            {
                reader->read(values.data(), &count, timeoutMs, &status);
            }
        }
        else
        {
            reader->read(values.data(), &count, &status);
        }

        py::array::ShapeContainer shape;
        if (blockSize > 1)
        {
            if (!isMultiReader)
            {
                shape = {count, blockSize};
            }
            else
            {
                shape = {blockSize, count};
            }
        }
        else
            shape = {count};

        py::array::StridesContainer strides;
        if (blockSize > 1 && isMultiReader)
            strides = {sizeof(ValueType) * initialCount, sizeof(ValueType)};

        return {toPyArray(std::move(values), shape, strides), status.detach()};
    }

    template <typename ValueType, typename DomainType, typename ReaderType>
    static inline std::tuple<SampleTypeVariant, DomainTypeVariant, typename daq::ReaderStatusType<ReaderType>::IType*> read(const ReaderType& reader,
                                                                                             size_t count,
                                                                                             [[maybe_unused]] size_t timeoutMs)
    {
        static_assert(sizeof(std::chrono::system_clock::time_point::rep) == sizeof(int64_t));
        using DomainVectorType = typename std::conditional<std::is_same<DomainType, std::chrono::system_clock::time_point>::value,
                                                           std::vector<int64_t>,
                                                           std::vector<DomainType>>::type;

        size_t blockSize = 1, initialCount = count;
        constexpr const bool isMultiReader = std::is_base_of_v<daq::MultiReaderPtr, ReaderType>;
        if constexpr (std::is_base_of_v<daq::BlockReaderPtr, ReaderType>)
        {
            reader->getBlockSize(&blockSize);
        }
        if constexpr (isMultiReader)
        {
            daq::ReaderConfigPtr readerConfig = reader.template asPtr<daq::IReaderConfig>();
            blockSize = readerConfig.getInputPorts().getCount();
        }

        using StatusType = typename daq::ReaderStatusType<ReaderType>::Type;
        StatusType status;
        std::vector<ValueType> values(count * blockSize);
        DomainVectorType domain(count * blockSize);
        if constexpr (ReaderHasReadWithTimeout<ReaderType, ValueType>::value)
        {
            if constexpr (isMultiReader)
            {
                std::vector<void*> valuesPtrs(blockSize), domainPtrs(blockSize);
                for (size_t i = 0; i < blockSize; i++)
                {
                    valuesPtrs[i] = values.data() + i * count;
                    domainPtrs[i] = domain.data() + i * count;
                }
                reader->readWithDomain(valuesPtrs.data(), domainPtrs.data(), &count, timeoutMs, &status);
            }
            else
            {
                reader->readWithDomain(values.data(), domain.data(), &count, timeoutMs, &status);
            }
        }
        else
        {
            reader->readWithDomain(values.data(), domain.data(), &count, &status);
        }

        py::array::ShapeContainer shape;
        if (blockSize > 1)
        {
            if (!isMultiReader)
            {
                shape = {count, blockSize};
            }
            else
            {
                shape = {blockSize, count};
            }
        }
        else
            shape = {count};

        py::array::StridesContainer strides;
        if (blockSize > 1 && isMultiReader)
        {
            strides = {sizeof(ValueType) * initialCount, sizeof(ValueType)};
        }

        std::string domainDtype;
        if constexpr (std::is_same_v<DomainType, std::chrono::system_clock::time_point>)
        {
            domainDtype = "datetime64[ns]";
            std::transform(domain.begin(),
                           domain.end(),
                           domain.begin(),
                           [](int64_t timestamp)
                           {
                               const auto t = std::chrono::system_clock::time_point(std::chrono::system_clock::duration(timestamp));
                               return std::chrono::duration_cast<std::chrono::nanoseconds>(t.time_since_epoch()).count();
                           });
        }

        auto valuesArray = toPyArray(std::move(values), shape, strides);
        auto domainArray = toPyArray(std::move(domain), shape, strides, domainDtype);

        // WA for datetime64
        if (!domainDtype.empty())
            domainArray.attr("dtype") = domainDtype;

        return {std::move(valuesArray), std::move(domainArray), status.detach()};
    }

    static inline void checkSampleType(daq::SampleType type)
    {
        switch (type)
        {
            case daq::SampleType::Float32:
            case daq::SampleType::Float64:
            case daq::SampleType::UInt32:
            case daq::SampleType::Int32:
            case daq::SampleType::UInt64:
            case daq::SampleType::Int64:
            case daq::SampleType::UInt8:
            case daq::SampleType::Int8:
            case daq::SampleType::UInt16:
            case daq::SampleType::Int16:
            case daq::SampleType::Invalid:  // for default value
                break;
            default:
                throw daq::InvalidParameterException("Unsupported sample type: " + convertSampleTypeToString(type));
        }
    }
};
