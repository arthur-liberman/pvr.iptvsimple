#pragma once
/*
 *      Copyright (C) 2018-now Arthur Liberman
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include <algorithm>

#include "p8-platform/os.h"
#include "libXBMC_addon.h"

class CArchiveConfig
{
public:
    void        ReadSettings(ADDON::CHelper_libXBMC_addon *XBMC);
    bool        IsEnabled() const { return m_bIsEnabled; }
    std::string FormatDateTime(time_t epgTime, const std::string &url) const;
    std::string GetArchiveUrlFormat() const { return m_sUrlFormat; }
    time_t      GetTimeshiftBuffer() const { return std::max(static_cast<time_t>(0), m_tTimeshiftBuffer); }
    time_t      GetEpgBeginBuffer() const { return std::max(static_cast<time_t>(0), m_tEpgBeginBuffer); }
    time_t      GetEpgEndBuffer() const { return std::max(static_cast<time_t>(0), m_tEpgEndBuffer); }
    bool        GetPlayEpgAsLive() const { return m_bPlayEpgAsLive; }

private:
    time_t      GetEpgBufferFromSettings(int setting) const;
    void        FormatTime(const char ch, const struct tm *pTime, std::string &fmt) const;
    void        FormatUtc(const char *str, time_t tTime, std::string &fmt) const;
    void        FormatOffset(time_t tTime, std::string &fmt) const;

    bool        m_bIsEnabled = false;
    std::string m_sUrlFormat;
    time_t      m_tTimeshiftBuffer = -1;
    time_t      m_tEpgBeginBuffer = -1;
    time_t      m_tEpgEndBuffer = -1;
    bool        m_bPlayEpgAsLive = false;

    ADDON::CHelper_libXBMC_addon *m_XBMC = nullptr;
};
