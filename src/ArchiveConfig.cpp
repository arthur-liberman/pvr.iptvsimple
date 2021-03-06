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

#include <string>
#include <regex>
#include <ctime>

#include "ArchiveConfig.h"

using namespace ADDON;

void CArchiveConfig::ReadSettings(CHelper_libXBMC_addon *XBMC)
{
    m_XBMC = XBMC;
    if (!XBMC->GetSetting("archType", &m_iArchiveType))
    {
        m_iArchiveType = CATCHUP_DEFAULT;
        bool legacyEnabled;
        if (XBMC->GetSetting("archEnable", &legacyEnabled) && legacyEnabled)
        {
            m_iArchiveType = CATCHUP_CUSTOM;
        }
    }
    m_bIsEnabled = GetArchiveType() > CATCHUP_DISABLED;
    char buffer[1024];
    if (XBMC->GetSetting("archUrlFormat", &buffer))
    {
        m_sUrlFormat = std::string(buffer);
    }
    int temp = 0;
    if (XBMC->GetSetting("archTimeshiftBuffer", &temp))
    {
        m_tTimeshiftBuffer = static_cast<time_t>(temp) * 24*60*60;
    }
    else
    {
        m_tTimeshiftBuffer = 24*60*60;    // Default to 1 day.
    }
    if (XBMC->GetSetting("archBeginBuffer", &temp))
    {
        m_tEpgBeginBuffer = GetEpgBufferFromSettings(temp);
    }
    else
    {
        m_tEpgBeginBuffer = 5*60;         // Default to 5 minutes.
    }
    if (XBMC->GetSetting("archEndBuffer", &temp))
    {
        m_tEpgEndBuffer = GetEpgBufferFromSettings(temp);
    }
    else
    {
        m_tEpgEndBuffer = 15*60;         // Default to 15 minutes.
    }
    if (!XBMC->GetSetting("archPlayEpgAsLive", &m_bPlayEpgAsLive))
    {
        m_bPlayEpgAsLive = false;
    }
}

std::string CArchiveConfig::FormatDateTime(time_t dateTimeEpg, time_t duration, const std::string &url) const
{
    const time_t dateTimeNow = std::time(0);
    struct tm dateTime = {0};
    localtime_r(&dateTimeEpg, &dateTime);
    std::string fmt(url);
    FormatTime('Y', &dateTime, fmt);
    FormatTime('m', &dateTime, fmt);
    FormatTime('d', &dateTime, fmt);
    FormatTime('H', &dateTime, fmt);
    FormatTime('M', &dateTime, fmt);
    FormatTime('S', &dateTime, fmt);
    FormatUtc("{utc}", dateTimeEpg, fmt);
    FormatUtc("${start}", dateTimeEpg, fmt);
    FormatUtc("{lutc}", dateTimeNow, fmt);
    FormatUtc("${timestamp}", dateTimeNow, fmt);
    FormatUtc("${offset}", dateTimeNow - dateTimeEpg, fmt);
    FormatUtc("${duration}", duration, fmt);
    FormatUnits(duration, "duration", fmt);
    FormatUnits(dateTimeNow - dateTimeEpg, "offset", fmt);
    m_XBMC->Log(LOG_DEBUG, "CArchiveConfig::FormatDateTime - \"%s\"", fmt.c_str());
    return fmt;
}

void CArchiveConfig::FormatTime(const char ch, const struct tm *pTime, std::string &fmt) const
{
    char str[] = { '{', ch, '}', 0 };
    auto pos = fmt.find(str);
    if (pos != std::string::npos)
    {
        char buff[256], timeFmt[3];
        snprintf(timeFmt, sizeof(timeFmt), "%%%c", ch);
        strftime(buff, sizeof(buff), timeFmt, pTime);
        if (strlen(buff) > 0)
            fmt.replace(pos, 3, buff);
    }
}

void CArchiveConfig::FormatUtc(const char *str, time_t tTime, std::string &fmt) const
{
    auto pos = fmt.find(str);
    if (pos != std::string::npos)
    {
        char buff[256];
        snprintf(buff, sizeof(buff), "%lu", tTime);
        fmt.replace(pos, strlen(str), buff);
    }
}

void CArchiveConfig::FormatUnits(time_t tTime, const std::string& name, std::string &fmt) const
{
    const std::string regexStr = ".*(\\{" + name + ":(\\d+)\\}).*";
    std::cmatch mr;
    std::regex rx(regexStr);
    if (std::regex_match(fmt.c_str(), mr, rx) && mr.length() >= 3)
    {
        std::string offsetExp = mr[1].first;
        std::string second = mr[1].second;
        if (second.length() > 0)
            offsetExp = offsetExp.erase(offsetExp.find(second));
        std::string dividerStr = mr[2].first;
        second = mr[2].second;
        if (second.length() > 0)
            dividerStr = dividerStr.erase(dividerStr.find(second));

        const time_t divider = stoi(dividerStr);
        if (divider != 0)
        {
            time_t offset = tTime / divider;
            if (offset < 0)
                offset = 0;
            fmt.replace(fmt.find(offsetExp), offsetExp.length(), std::to_string(offset));
        }
    }
}

time_t CArchiveConfig::GetEpgBufferFromSettings(int setting) const
{
    time_t minutes = 0;
    switch (setting)
    {
        case 1:
            minutes = 2;
        break;
        case 2:
            minutes = 5;
        break;
        case 3:
            minutes = 10;
        break;
        case 4:
            minutes = 15;
        break;
        case 5:
            minutes = 30;
        break;
        case 6:
            minutes = 60;
        break;
    }
    return minutes * 60;
}
