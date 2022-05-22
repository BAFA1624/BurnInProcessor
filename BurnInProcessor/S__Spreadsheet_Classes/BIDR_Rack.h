#pragma once

#include <chrono>
#include <vector>
#include <iostream>

#include "../BIDR_Defines.h"


namespace burn_in_data_report 
{
    enum class Status
    {
        Active,
        Inactive,
        Storage
    };

    typedef struct RackStatus
    {
        Status _cs;
        uinteger _sn;
        uinteger _pr;
        std::chrono::sys_time<nano> _st;

        RackStatus(
            const Status& cs,
            const uinteger& sn,
            const uinteger& pr,
            std::chrono::sys_time<nano> st
        ) : _cs{ cs }, _sn{ sn }, _pr{ pr }, _st{ st }
        {}
    } RackStatus;

    class Rack
    {
    private:

        Status _cs;                                                                             // Current status
        uinteger _sn;                                                                           // Serial number
        uinteger _pr;                                                                           // Power rating
        std::chrono::sys_time<nano> _st;                                                        // Most recent start time
        std::vector<std::pair<std::chrono::sys_time<nano>, std::chrono::sys_time<nano>>> _rtp;  // Timepoint pairs

    public:

        Rack(
            const uinteger& _serial_number,
            const uinteger& _power_rating
        )
            : _cs{ Status::Inactive }, _sn{ _serial_number }, _pr{ _power_rating }, _st{ nano::zero() }, _rtp{}
        {}

    protected:

        Status GetStatus() const noexcept { return _cs; }
        uinteger GetSerialNumber() const noexcept { return _sn; }
        uinteger GetPowerRating() const noexcept { return _pr; }
        std::chrono::sys_time<nano> GetStart() const noexcept
        {
            if (_rtp.size() == 0)
                return _st;
            else
                return _rtp[0].first;
        }
        nano GetCurrentRuntime() const noexcept
        {
            return nano{ std::chrono::system_clock::now().time_since_epoch() - _st.time_since_epoch() };
        }
        nano GetTotalRuntime() const noexcept
        {
            nano runtime{ nano::zero() };
            for (const auto& [start, end] : _rtp)
            {
                runtime += (end.time_since_epoch() - start.time_since_epoch());
            }
            runtime += (std::chrono::system_clock::now().time_since_epoch() - _st.time_since_epoch());
            return runtime;
        }
        RackStatus GetCurrentInfo() const noexcept { return RackStatus(_cs, _sn, _pr, _st); }
        bool IsActive() const noexcept { return (_st != std::chrono::sys_time<nano>{ nano::zero() }); }

        bool StartOperation() noexcept
        {
            if (_cs == Status::Active)
            {
                write_err_log( std::runtime_error("DLL: <Rack::StartOperation> Rack is already active".) );
                return false;
            }
            else
            {
                _st = std::chrono::sys_time<nano>{ std::chrono::system_clock::now() };
                _cs = Status::Active;
                return true;
            }
        }
        bool EndOperation()
        {
            if (_cs == Status::Inactive)
            {
                std::cerr << "ERROR: <Rack::EndOperation> Rack is already inactive." << std::endl;
            }
            else
            {
                _rtp.push_back({ _st, std::chrono::sys_time<nano>{ std::chrono::system_clock::now() } });
                _st = std::chrono::sys_time<nano>{ nano::zero() };
                _cs = Status::Inactive;
            }
            return true;
        }

        bool ChangeLaser(const uinteger& sn, const uinteger& pr) noexcept
        {
            if (_cs == Status::Inactive)
            {
                _sn = sn;
                _pr = pr;
            }
            else
            {
                std::cerr << "ERROR: <Rack::ChangeLaser> Can't change an active laser. End current operation first." << std::endl;
                return false;
            }
        }

    };

}