/*
 *      Copyright (C) 2013-2015 Anton Fedchin
 *      http://github.com/afedchin/xbmc-addon-iptvsimple/
 *
 *      Copyright (C) 2011 Pulse-Eight
 *      http://www.pulse-eight.com/
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

#include "client.h"
#include "ArchiveConfig.h"
#include "kodi/xbmc_pvr_dll.h"
#include "PVRIptvData.h"
#include "p8-platform/util/util.h"

using namespace ADDON;

#ifdef TARGET_WINDOWS
#define snprintf _snprintf
#ifdef CreateDirectory
#undef CreateDirectory
#endif
#ifdef DeleteFile
#undef DeleteFile
#endif
#endif

bool           m_bCreated       = false;
ADDON_STATUS   m_CurStatus      = ADDON_STATUS_UNKNOWN;
PVRIptvData   *m_data           = NULL;
PVRIptvChannel m_currentChannel = {0};

/* User adjustable settings are saved here.
 * Default values are defined inside client.h
 * and exported to the other source files.
 */
std::string g_strUserPath   = "";
std::string g_strClientPath = "";

CHelper_libXBMC_addon *XBMC = NULL;
CHelper_libXBMC_pvr   *PVR  = NULL;

std::string g_strTvgPath    = "";
std::string g_strM3UPath    = "";
std::string g_strLogoPath   = "";
std::string g_userAgent     = "";
int         g_logoPathType  = 0;
int         g_iEPGTimeShift = 0;
int         g_iStartNumber  = 1;
bool        g_bTSOverride   = true;
bool        g_bCacheM3U     = false;
bool        g_bCacheEPG     = false;
int         g_iEPGLogos     = 0;

bool        g_bIsArchive    = false;
bool        g_bResetUrlOffset = false;
CArchiveConfig g_ArchiveConfig;

extern std::string PathCombine(const std::string &strPath, const std::string &strFileName)
{
  std::string strResult = strPath;
  if (strResult.at(strResult.size() - 1) == '\\' ||
      strResult.at(strResult.size() - 1) == '/')
  {
    strResult.append(strFileName);
  }
  else
  {
    strResult.append("/");
    strResult.append(strFileName);
  }

  return strResult;
}

extern std::string GetClientFilePath(const std::string &strFileName)
{
  return PathCombine(g_strClientPath, strFileName);
}

extern std::string GetUserFilePath(const std::string &strFileName)
{
  return PathCombine(g_strUserPath, strFileName);
}

extern "C" {

void ADDON_ReadSettings(void)
{
  char buffer[1024];
  int iPathType = 0;
  if (!XBMC->GetSetting("m3uPathType", &iPathType))
  {
    iPathType = 1;
  }
  if (iPathType)
  {
    if (XBMC->GetSetting("m3uUrl", &buffer))
    {
      g_strM3UPath = buffer;
    }
    if (!XBMC->GetSetting("m3uCache", &g_bCacheM3U))
    {
      g_bCacheM3U = true;
    }
  }
  else
  {
    if (XBMC->GetSetting("m3uPath", &buffer))
    {
      g_strM3UPath = buffer;
    }
    g_bCacheM3U = false;
  }
  if (!XBMC->GetSetting("startNum", &g_iStartNumber))
  {
    g_iStartNumber = 1;
  }
  if (!XBMC->GetSetting("epgPathType", &iPathType))
  {
    iPathType = 1;
  }
  if (iPathType)
  {
    if (XBMC->GetSetting("epgUrl", &buffer))
    {
      g_strTvgPath = buffer;
    }
    if (!XBMC->GetSetting("epgCache", &g_bCacheEPG))
    {
      g_bCacheEPG = true;
    }
  }
  else
  {
    if (XBMC->GetSetting("epgPath", &buffer))
    {
      g_strTvgPath = buffer;
    }
    g_bCacheEPG = false;
  }
  float fShift;
  if (XBMC->GetSetting("epgTimeShift", &fShift))
  {
    g_iEPGTimeShift = (int)(fShift * 3600.0); // hours to seconds
  }
  if (!XBMC->GetSetting("epgTSOverride", &g_bTSOverride))
  {
    g_bTSOverride = true;
  }
  if (!XBMC->GetSetting("logoPathType", &iPathType))
  {
    iPathType = 1;
  }
  g_logoPathType = iPathType;
  if (XBMC->GetSetting(iPathType ? "logoBaseUrl" : "logoPath", &buffer))
  {
    g_strLogoPath = buffer;
  }
  if (XBMC->GetSetting("userAgent", &buffer))
  {
    g_userAgent = buffer;
  }

  // Logos from EPG
  if (!XBMC->GetSetting("logoFromEpg", &g_iEPGLogos))
    g_iEPGLogos = 0;

  g_ArchiveConfig.ReadSettings(XBMC);
}

ADDON_STATUS ADDON_Create(void* hdl, void* props)
{
  if (!hdl || !props)
  {
    return ADDON_STATUS_UNKNOWN;
  }

  PVR_PROPERTIES* pvrprops = (PVR_PROPERTIES*)props;

  XBMC = new CHelper_libXBMC_addon;
  if (!XBMC->RegisterMe(hdl))
  {
    SAFE_DELETE(XBMC);
    return ADDON_STATUS_PERMANENT_FAILURE;
  }

  PVR = new CHelper_libXBMC_pvr;
  if (!PVR->RegisterMe(hdl))
  {
    SAFE_DELETE(PVR);
    SAFE_DELETE(XBMC);
    return ADDON_STATUS_PERMANENT_FAILURE;
  }

  XBMC->Log(LOG_DEBUG, "%s - Creating the PVR IPTV Archive add-on", __FUNCTION__);

  m_CurStatus     = ADDON_STATUS_UNKNOWN;
  g_strUserPath   = pvrprops->strUserPath;
  g_strClientPath = pvrprops->strClientPath;

  if (!XBMC->DirectoryExists(g_strUserPath.c_str()))
  {
    XBMC->CreateDirectory(g_strUserPath.c_str());
  }

  ADDON_ReadSettings();

  m_data = new PVRIptvData;
  m_CurStatus = ADDON_STATUS_OK;
  m_bCreated = true;

  return m_CurStatus;
}

ADDON_STATUS ADDON_GetStatus()
{
  return m_CurStatus;
}

void ADDON_Destroy()
{
  delete m_data;
  m_bCreated = false;
  m_CurStatus = ADDON_STATUS_UNKNOWN;
}

ADDON_STATUS ADDON_SetSetting(const char *settingName, const void *settingValue)
{
  // reset cache and restart addon

  std::string strFile = GetUserFilePath(M3U_FILE_NAME);
  if (XBMC->FileExists(strFile.c_str(), false))
  {
    XBMC->DeleteFile(strFile.c_str());
  }

  strFile = GetUserFilePath(TVG_FILE_NAME);
  if (XBMC->FileExists(strFile.c_str(), false))
  {
    XBMC->DeleteFile(strFile.c_str());
  }

  return ADDON_STATUS_NEED_RESTART;
}

/***********************************************************
 * PVR Client AddOn specific public library functions
 ***********************************************************/

void OnSystemSleep()
{
}

void OnSystemWake()
{
}

void OnPowerSavingActivated()
{
}

void OnPowerSavingDeactivated()
{
}

PVR_ERROR GetAddonCapabilities(PVR_ADDON_CAPABILITIES* pCapabilities)
{
  pCapabilities->bSupportsEPG             = true;
  pCapabilities->bSupportsTV              = true;
  pCapabilities->bSupportsRadio           = true;
  pCapabilities->bSupportsChannelGroups   = true;
  pCapabilities->bSupportsRecordings      = false;
  pCapabilities->bSupportsRecordingsRename = false;
  pCapabilities->bSupportsRecordingsLifetimeChange = false;
  pCapabilities->bSupportsDescrambleInfo = false;

  return PVR_ERROR_NO_ERROR;
}

const char *GetBackendName(void)
{
  static const char *strBackendName = "IPTV Archive PVR Add-on";
  return strBackendName;
}

const char *GetBackendVersion(void)
{
  static std::string strBackendVersion = STR(IPTV_VERSION);
  return strBackendVersion.c_str();
}

const char *GetConnectionString(void)
{
  static std::string strConnectionString = "connected";
  return strConnectionString.c_str();
}

const char *GetBackendHostname(void)
{
  return "";
}

PVR_ERROR GetDriveSpace(long long *iTotal, long long *iUsed)
{
  *iTotal = 0;
  *iUsed  = 0;
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR GetEPGForChannel(ADDON_HANDLE handle, const PVR_CHANNEL &channel, time_t iStart, time_t iEnd)
{
  if (m_data)
    return m_data->GetEPGForChannel(handle, channel, iStart, iEnd);

  return PVR_ERROR_SERVER_ERROR;
}

int GetChannelsAmount(void)
{
  if (m_data)
    return m_data->GetChannelsAmount();

  return -1;
}

PVR_ERROR GetChannels(ADDON_HANDLE handle, bool bRadio)
{
  if (m_data)
    return m_data->GetChannels(handle, bRadio);

  return PVR_ERROR_SERVER_ERROR;
}

void SetStreamProperty(PVR_NAMED_VALUE* properties, unsigned int* iPropertiesCount,std::string name, std::string value)
{
  strncpy(properties[*iPropertiesCount].strName, name.c_str(), sizeof(properties[*iPropertiesCount].strName) - 1);
  strncpy(properties[*iPropertiesCount].strValue, value.c_str(), sizeof(properties[*iPropertiesCount].strValue) - 1);
  (*iPropertiesCount)++;
}

PVR_ERROR GetChannelStreamProperties(const PVR_CHANNEL* channel, PVR_NAMED_VALUE* properties, unsigned int* iPropertiesCount)
{
  if (!m_data || !channel || !properties || !iPropertiesCount)
    return PVR_ERROR_SERVER_ERROR;

  if (*iPropertiesCount < 1)
    return PVR_ERROR_INVALID_PARAMETERS;

  *iPropertiesCount = 0;
  if (m_data->GetChannel(*channel, m_currentChannel))
  {
    g_bIsArchive = false;
    if (g_bResetUrlOffset)
    {
      g_bResetUrlOffset = false;
      if (m_data->IsArchiveSupportedOnChannel(m_currentChannel))
      {
        m_data->SetEpgUrlTimeOffset(m_currentChannel.iCatchupLength);
        m_currentChannel.timeshiftStartTime = time(0) - m_currentChannel.iCatchupLength;
      }
      else
      {
        m_data->SetEpgUrlTimeOffset(0);
        m_currentChannel.timeshiftStartTime = 0;
      }
    }
    m_currentChannel.epgTag.startTime = m_currentChannel.timeshiftStartTime;
    std::string epgStrUrl = m_data->GetEpgTagUrl(nullptr, m_currentChannel);
    if (!epgStrUrl.empty())
      SetStreamProperty(properties, iPropertiesCount, PVR_STREAM_PROPERTY_STREAMURL, epgStrUrl.c_str());
    else
      SetStreamProperty(properties, iPropertiesCount, PVR_STREAM_PROPERTY_STREAMURL, m_currentChannel.strStreamURL.c_str());
    if (!m_currentChannel.properties.empty())
    {
      for (auto& prop : m_currentChannel.properties)
        SetStreamProperty(properties, iPropertiesCount, prop.first, prop.second);
    }

    XBMC->Log(LOG_DEBUG, "GetChannelStreamProperties - url: %s", properties[0].strValue);
    return PVR_ERROR_NO_ERROR;
  }

  return PVR_ERROR_SERVER_ERROR;
}

int GetChannelGroupsAmount(void)
{
  if (m_data)
    return m_data->GetChannelGroupsAmount();

  return -1;
}

PVR_ERROR GetChannelGroups(ADDON_HANDLE handle, bool bRadio)
{
  if (m_data)
    return m_data->GetChannelGroups(handle, bRadio);

  return PVR_ERROR_SERVER_ERROR;
}

PVR_ERROR GetChannelGroupMembers(ADDON_HANDLE handle, const PVR_CHANNEL_GROUP &group)
{
  if (m_data)
    return m_data->GetChannelGroupMembers(handle, group);

  return PVR_ERROR_SERVER_ERROR;
}

PVR_ERROR SignalStatus(PVR_SIGNAL_STATUS &signalStatus)
{
  snprintf(signalStatus.strAdapterName, sizeof(signalStatus.strAdapterName), "IPTV Archive Adapter 1");
  snprintf(signalStatus.strAdapterStatus, sizeof(signalStatus.strAdapterStatus), "OK");

  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR IsEPGTagPlayable(const EPG_TAG* tag, bool* bIsPlayable)
{
  if (!m_data)
    return PVR_ERROR_SERVER_ERROR;

  const time_t now = time(nullptr);
  PVRIptvChannel channel;
  *bIsPlayable = (g_ArchiveConfig.IsEnabled() && tag->startTime < now && m_data->GetChannel(tag, channel) &&
    m_data->IsArchiveSupportedOnChannel(channel) && tag->startTime >= (now - static_cast<time_t>(channel.iCatchupLength)));
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR GetEPGTagStreamProperties(const EPG_TAG* tag,
    PVR_NAMED_VALUE* properties, unsigned int* iPropertiesCount)
{
  XBMC->Log(LOG_DEBUG, "GetEPGTagStreamProperties - Tag startTime: %ld \tendTime: %ld", tag->startTime, tag->endTime);

  if (!m_data || !tag || !properties || !iPropertiesCount)
    return PVR_ERROR_SERVER_ERROR;

  if (*iPropertiesCount < 1)
    return PVR_ERROR_INVALID_PARAMETERS;

  *iPropertiesCount = 0;
  XBMC->Log(LOG_DEBUG, "GetEPGTagStreamProperties - GetPlayEpgAsLive is %s",
            g_ArchiveConfig.GetPlayEpgAsLive() ? "enabled" : "disabled");
  std::string epgStrUrl;
  if (g_ArchiveConfig.GetPlayEpgAsLive())
  {
    g_bResetUrlOffset = false;
    epgStrUrl = m_data->GetEpgTagUrl(tag, m_currentChannel);
    time_t timeNow = time(0);
    time_t programOffset = timeNow - m_currentChannel.epgTag.startTime;
    time_t timeshiftBuffer = std::max(programOffset, static_cast<time_t>(m_currentChannel.iCatchupLength));
    m_currentChannel.timeshiftStartTime = timeNow - timeshiftBuffer;
    m_currentChannel.epgTag.startTime = m_currentChannel.timeshiftStartTime;
    m_currentChannel.epgTag.endTime = timeNow;
    m_data->SetEpgUrlTimeOffset(timeshiftBuffer - programOffset);
  }
  else if (g_bResetUrlOffset)
  {
    g_bResetUrlOffset = false;
    epgStrUrl = m_data->GetEpgTagUrl(tag, m_currentChannel);
    const time_t beginBuffer = g_ArchiveConfig.GetEpgBeginBuffer();
    const time_t endBuffer = g_ArchiveConfig.GetEpgEndBuffer();
    m_currentChannel.timeshiftStartTime = m_currentChannel.epgTag.startTime - beginBuffer;
    m_currentChannel.epgTag.startTime = m_currentChannel.timeshiftStartTime;
    m_currentChannel.epgTag.endTime += endBuffer;
    m_data->SetEpgUrlTimeOffset(beginBuffer);
  }
  else
  {
    epgStrUrl = m_data->GetEpgTagUrl(nullptr, m_currentChannel);
  }

  if (!epgStrUrl.empty())
  {
    g_bIsArchive = true;
    SetStreamProperty(properties, iPropertiesCount, PVR_STREAM_PROPERTY_STREAMURL, epgStrUrl.c_str());
    if (!m_currentChannel.properties.empty())
    {
      for (auto& prop : m_currentChannel.properties)
        SetStreamProperty(properties, iPropertiesCount, prop.first, prop.second);
    }

    XBMC->Log(LOG_DEBUG, "GetEPGTagStreamProperties - url: %s", epgStrUrl.c_str());
    return g_ArchiveConfig.GetPlayEpgAsLive() ? PVR_ERROR_NOT_IMPLEMENTED : PVR_ERROR_NO_ERROR;
  }

  return PVR_ERROR_FAILED;
}

PVR_ERROR GetStreamTimes(PVR_STREAM_TIMES *times)
{
  if (!times)
    return PVR_ERROR_INVALID_PARAMETERS;
  if (!g_ArchiveConfig.IsEnabled() || m_currentChannel.timeshiftStartTime == 0)
    return PVR_ERROR_NOT_IMPLEMENTED;

  *times = {0};
  const time_t dateTimeNow = time(0);
  const EPG_TAG &epgTag = m_currentChannel.epgTag;
  times->startTime = m_currentChannel.timeshiftStartTime;
  if (g_bIsArchive)
    times->ptsEnd = static_cast<int64_t>(std::min(dateTimeNow, epgTag.endTime) - times->startTime) * DVD_TIME_BASE;
  else
    times->ptsEnd = static_cast<int64_t>(dateTimeNow - times->startTime) * DVD_TIME_BASE;

  XBMC->Log(LOG_DEBUG, "GetStreamTimes - Ch = %u \tTitle = \"%s\" \tepgTag->startTime = %ld \tepgTag->endTime = %ld",
            m_currentChannel.epgTag.iUniqueChannelId, m_currentChannel.epgTag.strTitle, m_currentChannel.epgTag.startTime, m_currentChannel.epgTag.endTime);
  XBMC->Log(LOG_DEBUG, "GetStreamTimes - startTime = %ld \tptsStart = %lld \tptsBegin = %lld \tptsEnd = %lld",
            times->startTime, times->ptsStart, times->ptsBegin, times->ptsEnd);
  return PVR_ERROR_NO_ERROR;
}

long long LengthLiveStream(void)
{
  long long ret = -1;
  const EPG_TAG &epgTag = m_currentChannel.epgTag;
  if (epgTag.startTime > 0 && epgTag.endTime >= epgTag.startTime)
    ret = (epgTag.endTime - epgTag.startTime) * DVD_TIME_BASE;
  return ret;
}

long long SeekLiveStream(long long iPosition, int iWhence /* = SEEK_SET */)
{
  long long ret = -1;
  const EPG_TAG &epgTag = m_currentChannel.epgTag;
  if (epgTag.startTime > 0)
  {
    XBMC->Log(LOG_DEBUG, "SeekLiveStream - iPosition = %lld, iWhence = %d", iPosition, iWhence);
    const time_t timeNow = time(0);
    switch (iWhence)
    {
      case SEEK_SET:
      {
        iPosition += 500;
        iPosition /= 1000;
        if (epgTag.startTime + iPosition < timeNow - 10)
        {
          ret = iPosition;
          m_data->SetEpgUrlTimeOffset(iPosition);
        }
        else
        {
          ret = timeNow - epgTag.startTime;
          m_data->SetEpgUrlTimeOffset(ret);
        }
        ret *= DVD_TIME_BASE;
      }
      break;
      case SEEK_CUR:
      {
        long long offset = m_data->GetEpgUrlTimeOffset();
        XBMC->Log(LOG_DEBUG, "SeekLiveStream - timeNow = %d, startTime = %d, iTvgShift = %d, offset = %d", timeNow, epgTag.startTime, m_currentChannel.iTvgShift, offset);
        ret = offset * DVD_TIME_BASE;
      }
      break;
      default:
        XBMC->Log(LOG_NOTICE, "SeekLiveStream - Unsupported SEEK command (%d)", iWhence);
      break;
    }
  }
  return ret;
}

void CloseLiveStream(void)
{
  g_bResetUrlOffset = true;
}

/** UNUSED API FUNCTIONS */
bool CanPauseStream(void) { return true; }
int GetRecordingsAmount(bool deleted) { return -1; }
PVR_ERROR GetRecordings(ADDON_HANDLE handle, bool deleted) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR GetRecordingStreamProperties(const PVR_RECORDING*, PVR_NAMED_VALUE*, unsigned int*) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR OpenDialogChannelScan(void) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR CallMenuHook(const PVR_MENUHOOK &menuhook, const PVR_MENUHOOK_DATA &item) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR DeleteChannel(const PVR_CHANNEL &channel) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR RenameChannel(const PVR_CHANNEL &channel) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR OpenDialogChannelSettings(const PVR_CHANNEL &channel) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR OpenDialogChannelAdd(const PVR_CHANNEL &channel) { return PVR_ERROR_NOT_IMPLEMENTED; }
bool OpenRecordedStream(const PVR_RECORDING &recording) { return false; }
bool OpenLiveStream(const PVR_CHANNEL &channel) { return false; }
void CloseRecordedStream(void) {}
int ReadRecordedStream(unsigned char *pBuffer, unsigned int iBufferSize) { return 0; }
long long SeekRecordedStream(long long iPosition, int iWhence /* = SEEK_SET */) { return 0; }
long long LengthRecordedStream(void) { return 0; }
void DemuxReset(void) {}
void DemuxFlush(void) {}
int ReadLiveStream(unsigned char *pBuffer, unsigned int iBufferSize) { return 0; }
PVR_ERROR DeleteRecording(const PVR_RECORDING &recording) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR RenameRecording(const PVR_RECORDING &recording) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR SetRecordingPlayCount(const PVR_RECORDING &recording, int count) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR SetRecordingLastPlayedPosition(const PVR_RECORDING &recording, int lastplayedposition) { return PVR_ERROR_NOT_IMPLEMENTED; }
int GetRecordingLastPlayedPosition(const PVR_RECORDING &recording) { return -1; }
PVR_ERROR GetRecordingEdl(const PVR_RECORDING&, PVR_EDL_ENTRY[], int*) { return PVR_ERROR_NOT_IMPLEMENTED; };
PVR_ERROR GetTimerTypes(PVR_TIMER_TYPE types[], int *size) { return PVR_ERROR_NOT_IMPLEMENTED; }
int GetTimersAmount(void) { return -1; }
PVR_ERROR GetTimers(ADDON_HANDLE handle) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR AddTimer(const PVR_TIMER &timer) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR DeleteTimer(const PVR_TIMER &timer, bool bForceDelete) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR UpdateTimer(const PVR_TIMER &timer) { return PVR_ERROR_NOT_IMPLEMENTED; }
void DemuxAbort(void) {}
DemuxPacket* DemuxRead(void) { return NULL; }
bool IsTimeshifting(void) { return false; }
bool IsRealTimeStream(void) { return true; }
void PauseStream(bool bPaused) {}
bool CanSeekStream(void) { return true; }
bool SeekTime(double,bool,double*) { return false; }
void SetSpeed(int) {};
PVR_ERROR UndeleteRecording(const PVR_RECORDING& recording) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR DeleteAllRecordingsFromTrash() { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR SetEPGTimeFrame(int) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR GetDescrambleInfo(PVR_DESCRAMBLE_INFO*) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR SetRecordingLifetime(const PVR_RECORDING*) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR GetStreamProperties(PVR_STREAM_PROPERTIES*) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR IsEPGTagRecordable(const EPG_TAG*, bool*) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR GetEPGTagEdl(const EPG_TAG* epgTag, PVR_EDL_ENTRY edl[], int *size) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR GetStreamReadChunkSize(int* chunksize) { return PVR_ERROR_NOT_IMPLEMENTED; }

} // extern "C"
