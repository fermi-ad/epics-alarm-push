#include <errlog.h>
#include <dbAccess.h>
#include <dbStaticLib.h>
#include <epicsExport.h>
#include <envDefs.h>
#include <alarm.h>
#include <initHooks.h>

#include <map>
#include <cstring>

#include "hiredis/hiredis.h"

using namespace std;

struct PrevAlarm { unsigned short stat, sevr; };

static string REDIS_STREAM;
static redisContext* REDIS_CTX = 0;

//  per errlog.h: errlogInfo = 0, errlogMinor = 1, errlogMajor = 2, errlogFatal = 3
static uint LOG_SEVERITY = errlogFatal;
#define LOGERR(fmt, ...) do { if (LOG_SEVERITY <= errlogMajor) errlogPrintf(ANSI_RED("AlarmPush: " fmt), ##__VA_ARGS__);    } while(0)
#define LOGWRN(fmt, ...) do { if (LOG_SEVERITY <= errlogMinor) errlogPrintf(ANSI_YELLOW("AlarmPush: " fmt), ##__VA_ARGS__); } while(0)
#define LOGINF(fmt, ...) do { if (LOG_SEVERITY <= errlogInfo)  errlogPrintf("AlarmPush: " fmt, ##__VA_ARGS__);              } while(0)

static string get_env_str(const string& key, const string& def = "")
{
  char key_buf[128] = { 0 }, def_buf[128] = { 0 }, val_buf[128] = { 0 };
  strncpy(key_buf, key.c_str(), sizeof(key_buf) - 1);
  strncpy(def_buf, def.c_str(), sizeof(def_buf) - 1);

  ENV_PARAM envp = { key_buf, def_buf };
  envGetConfigParam(&envp, sizeof(val_buf), val_buf);
  return val_buf;
}

static void alarm_event(void* args, dbChannel* pch, int evts, db_field_log* pfl)
{
  if (pch == 0 || pch->name == 0 || pch->addr.precord == 0 || pfl == 0 || REDIS_CTX == 0) return;   //  dont crash

  LOGINF("redis %p | stream %s | chan %s | recd %s | stat %i | sevr %i\n",
      REDIS_CTX, REDIS_STREAM.c_str(), pch->name, pch->addr.precord->name, pfl->stat, pfl->sevr);

  //  return early if not an alarm
  if ((pfl->mask & DBE_ALARM) == 0)
  {
    LOGWRN("mask does not include DBE_ALARM %x\n", pfl->mask);
    return;
  }

  //  return early if alarm would be INVALID, INVALID
  if (pfl->stat >= ALARM_NSTATUS && pfl->sevr >= epicsSevInvalid)
  {
    LOGWRN("out of range stat %u and sevr %u\n", pfl->stat, pfl->sevr);
    return;
  }

  string pvname = pch->name;

  static map<const string, PrevAlarm> PREV_ALARM;
  PrevAlarm prev;
  if (PREV_ALARM.count(pvname))
  {
    //  return early if new alarm is same stat/sevr as prev
    prev = PREV_ALARM.at(pvname);
    if (pfl->stat == prev.stat && pfl->sevr == prev.sevr) return;
  }
  else
  {
    //  return early if new alarm is NO_ALARM and no prev
    if (pfl->stat == NO_ALARM && pfl->sevr == NO_ALARM) return;
  }
  //  remember new stat/sevr to test for dupes next time
  prev.stat = pfl->stat; prev.sevr = pfl->sevr;
  PREV_ALARM[pvname] = prev;

  string status = (pfl->stat < ALARM_NSTATUS)
      ? epicsAlarmConditionStrings[pfl->stat]
      : "INVALID";

  string severity = (pfl->sevr < ALARM_NSEV)
      ? epicsAlarmSeverityStrings[pfl->sevr]
      : epicsAlarmSeverityStrings[epicsSevInvalid];

  string message = (pfl->amsg[0])
      ? pfl->amsg
      : status + " alarm is " + severity;

  string timestamp = to_string(time(0));

  redisReply* reply = 0;
  if (pfl->stat == NO_ALARM && pfl->sevr == NO_ALARM)
  {
    reply = (redisReply*)redisCommand(REDIS_CTX,
        "XADD %s MAXLEN 99999 * device %s source EPICS severity %s timestamp %s detail %s",
        REDIS_STREAM.c_str(), pvname.c_str(), severity.c_str(), timestamp.c_str(), status.c_str());
  }
  else
  {
    reply = (redisReply*)redisCommand(REDIS_CTX,
        "XADD %s MAXLEN 99999 * device %s source EPICS severity %s timestamp %s detail %s message %s",
        REDIS_STREAM.c_str(), pvname.c_str(), severity.c_str(), timestamp.c_str(), status.c_str(), message.c_str());
  }

  if (reply == 0 || reply->type == REDIS_REPLY_ERROR)
  {
    static time_t PREV = 0;   //  throttle reconnects
    time_t curr = time(0);    //  once per second

    if (curr > PREV)
    {
      PREV = curr;
      int result = redisReconnect(REDIS_CTX);
      LOGERR("error %s | %s | reconnect %i\n", reply ? reply->str : 0, REDIS_CTX->errstr, result);
    }
    else
      { LOGWRN("error %s | %s | defer\n", reply ? reply->str : 0, REDIS_CTX->errstr); }
  }
  else
    { LOGINF("reply %s\n", reply->str); }

  freeReplyObject(reply);   //  ok to free 0
}

static void init_after_db(initHookState state)
{
  if (state != initHookAfterInitDatabase) return;   //  only once

  int no_filter = stoi(get_env_str("EPICS_ALARM_PUSH_NO_FILTER", "0"));

  //  hook DBE_ALARM for all records
  auto ctx = db_init_events();
  auto dse = db_start_events(ctx, 0, 0, 0, epicsThreadPriorityMedium);
  LOGINF("ctx %p | dse %i\n", ctx, dse);

  DBENTRY ent; dbInitEntry(pdbbase, &ent);
  for (auto st1 = dbFirstRecordType(&ent); st1 == 0; st1 = dbNextRecordType(&ent))
  {
    for (auto st2 = dbFirstRecord(&ent); st2 == 0; st2 = dbNextRecord(&ent))
    {
      LOGINF("dbRecordType %p | typename %s\n", ent.precordType, ent.precordType ? ent.precordType->name : 0);
      LOGINF("dbRecordNode %p | recordname %s\n", ent.precnode, ent.precnode ? ent.precnode->recordname : 0);

      if (ent.precnode == 0 || ent.precnode->recordname == 0 || ent.precnode->recordname[0] == 0)
      {
        LOGWRN("skipping record with no recordname - precnode %p\n", ent.precnode);
        continue;
      }
      size_t rec_name_len = strlen(ent.precnode->recordname);
      if (no_filter == 0 && rec_name_len > 0 && ent.precnode->recordname[rec_name_len - 1] == '_')
      {
        LOGWRN("skipping record with trailing underscore - precnode %p\n", ent.precnode);
        continue;
      }
      auto chn = dbChannelCreate(ent.precnode->recordname);
      if (chn == 0)
      {
        LOGERR("record %s | dbChannelCreate failed\n", ent.precnode ? ent.precnode->recordname : 0);
        continue;
      }
      auto alm = db_add_event(ctx, chn, alarm_event, 0, DBE_ALARM);
      if (alm == 0)
      {
        LOGERR("record %s | chn %p | db_add_event failed\n", ent.precnode ? ent.precnode->recordname : 0, chn);
        continue;
      }
      LOGWRN("record %s | chn %p | alm %p\n", ent.precnode ? ent.precnode->recordname : 0, chn, alm);
      db_event_enable(alm);
    }
  }
}

static long init_driver()
{
  if (REDIS_CTX) return 0;  //  only once

  //  if no EPICS_ALARM_PUSH_REDIS_HOST was found go no further - this will prevent
  //  the DB event hooks from being registered so no alarm push functionality will occur
  string redis_host = get_env_str("EPICS_ALARM_PUSH_REDIS_HOST");   //  no default
  if (redis_host.size() == 0)
  {
    errlogPrintf("EPICS_ALARM_PUSH_REDIS_HOST not found - AlarmPush disabled\n");
    return 0;
  }
  LOG_SEVERITY = (uint)stoi(get_env_str("EPICS_ALARM_PUSH_LOG_SEVERITY", to_string(LOG_SEVERITY)));
  errlogPrintf("EPICS_ALARM_PUSH_LOG_SEVERITY %u\n", LOG_SEVERITY);

  REDIS_STREAM = get_env_str("EPICS_ALARM_PUSH_REDIS_STREAM", "acorn:alarms");

  int redis_port = stoi(get_env_str("EPICS_ALARM_PUSH_REDIS_PORT", "6379"));

  REDIS_CTX = redisConnect(redis_host.c_str(), redis_port);
  if (REDIS_CTX == 0)
    { LOGERR("redis host %s | port %i | context %p\n", redis_host.c_str(), redis_port, REDIS_CTX); }
  else if (REDIS_CTX->errstr[0])
    { LOGWRN("redis host %s | port %i | context %p | %s\n", redis_host.c_str(), redis_port, REDIS_CTX, REDIS_CTX->errstr); }
  else
    { LOGINF("redis host %s | port %i | context %p\n", redis_host.c_str(), redis_port, REDIS_CTX); }

  //  if we didnt create a redis connection context then dont register DB event hooks
  //  its ok if we got a context but theres no connection yet - we will reconnect above
  //  but if we didnt even get a context object something is wrong so dont hook DB events
  if (REDIS_CTX == 0)
  {
    errlogPrintf("REDIS_CTX not created - AlarmPush disabled\n");
    return 0;
  }
  //  set IOC init hook here to do stuff that needs DB to be initialized
  return initHookRegister(init_after_db);
}

static drvet AlarmPush = { 2, 0, init_driver };

epicsExportAddress(drvet, AlarmPush);
