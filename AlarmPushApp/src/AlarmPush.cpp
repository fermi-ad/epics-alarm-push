#include <errlog.h>
#include <dbAccess.h>
#include <dbStaticLib.h>
#include <epicsExport.h>
#include <envDefs.h>
#include <alarm.h>
#include <initHooks.h>

#include <map>
#include <string>
#include <cstring>
#include <ctime>

#include "hiredis/hiredis.h"

using namespace std;

static uint LOG_SEVERITY = 0;
static char REDIS_STREAM[64] = { 0 };
static redisContext* REDIS_CTX = 0;

struct PrevAlarm { unsigned short stat, sevr; };

//  defining our own version of errlogSevPrintf so that we can start with
//  no logging by default and only log at the level defined by our env var
#define LOGERR(fmt, ...) (LOG_SEVERITY <= (uint)errlogMajor ? errlogPrintf(ANSI_RED("AlarmPush: " fmt "\n"), __VA_ARGS__) : 0)
#define LOGWRN(fmt, ...) (LOG_SEVERITY <= (uint)errlogMinor ? errlogPrintf(ANSI_YELLOW("AlarmPush: " fmt "\n"), __VA_ARGS__) : 0)
#define LOGINF(fmt, ...) (LOG_SEVERITY <= (uint)errlogInfo  ? errlogPrintf("AlarmPush: " fmt "\n", __VA_ARGS__) : 0)

static void alarm_event(void* args, dbChannel* pch, int evts, db_field_log* pfl)
{
  if (pch == 0 || pch->name == 0 || pch->addr.precord == 0 || pfl == 0 || REDIS_CTX == 0) return;   //  dont crash

  LOGINF("redis %p | stream %s | chan %s | recd %s | stat %i | sevr %i",
      REDIS_CTX, REDIS_STREAM, pch->name, pch->addr.precord->name, pfl->stat, pfl->sevr);

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

  string detail = status;

  redisReply* reply = 0;
  if (pfl->stat == NO_ALARM && pfl->sevr == NO_ALARM)
  {
    reply = (redisReply*)redisCommand(REDIS_CTX,
        "XADD %s MAXLEN 99999 * device %s source %s severity %s timestamp %s detail %s",
        REDIS_STREAM, pvname.c_str(), status.c_str(), severity.c_str(), timestamp.c_str(), detail.c_str());
  }
  else
  {
    reply = (redisReply*)redisCommand(REDIS_CTX,
        "XADD %s MAXLEN 99999 * device %s source %s severity %s timestamp %s detail %s message %s",
        REDIS_STREAM, pvname.c_str(), status.c_str(), severity.c_str(), timestamp.c_str(), detail.c_str(), message.c_str());
  }

  if (reply == 0 || reply->type == REDIS_REPLY_ERROR)
  {
    static time_t PREV = 0;   //  throttle reconnects
    time_t curr = time(0);    //  once per second

    if (curr > PREV)
    {
      PREV = curr;
      int result = redisReconnect(REDIS_CTX);
      LOGERR("error %s | %s | reconnect %i", reply ? reply->str : 0, REDIS_CTX->errstr, result);
    }
    else
      { LOGWRN("error %s | %s | defer", reply ? reply->str : 0, REDIS_CTX->errstr); }
  }
  else
    { LOGINF("reply %s", reply->str); }

  freeReplyObject(reply);   //  ok to free 0
}

static void init_after_db(initHookState state)
{
  if (state != initHookAfterInitDatabase) return;   //  only once

  //  hook DBE_ALARM for all records
  auto ctx = db_init_events();
  auto dse = db_start_events(ctx, 0, 0, 0, epicsThreadPriorityMedium);
  LOGINF("ctx %p | dse %i", ctx, dse);

  DBENTRY ent; dbInitEntry(pdbbase, &ent);
  for (auto st1 = dbFirstRecordType(&ent); st1 == 0; st1 = dbNextRecordType(&ent))
  {
    for (auto st2 = dbFirstRecord(&ent); st2 == 0; st2 = dbNextRecord(&ent))
    {
      LOGINF("dbRecordType %p | typename %s", ent.precordType, ent.precordType ? ent.precordType->name : 0);
      LOGINF("dbRecordNode %p | recordname %s", ent.precnode, ent.precnode ? ent.precnode->recordname : 0);

      //  TODO: further filtering based on record type? existence of alarm fields? other criteria?

        if (ent.precnode == 0 || ent.precnode->recordname == 0 || ent.precnode->recordname[0] == 0)
        {
          LOGWRN("skipping record with no recordname - precnode %p", ent.precnode);
          continue;
        }
        auto chn = dbChannelCreate(ent.precnode->recordname);
        if (chn == 0)
        {
          LOGERR("record %s | dbChannelCreate failed", ent.precnode ? ent.precnode->recordname : 0);
          continue;
        }
        auto alm = db_add_event(ctx, chn, alarm_event, 0, DBE_ALARM);
        if (alm == 0)
        {
          LOGERR("record %s | chn %p | db_add_event failed", ent.precnode ? ent.precnode->recordname : 0, chn);
          continue;
        }
        LOGWRN("record %s | chn %p | alm %p", ent.precnode ? ent.precnode->recordname : 0, chn, alm);
        db_event_enable(alm);
    }
  }
}

static long init_driver()
{
  if (REDIS_CTX) return 0;  //  only once

  char env_key[64], env_def[64];
  ENV_PARAM envp = { env_key, env_def };

  strncpy(env_key, "EPICS_ALARM_PUSH_LOG_SEVERITY", sizeof(env_key));
  strncpy(env_def, "-1", sizeof(env_def));
  char log_sev_str[16] = { 0 };
  envGetConfigParam(&envp, sizeof(log_sev_str), log_sev_str);
  LOG_SEVERITY = (uint)atoi(log_sev_str);

  errlogPrintf("EPICS_ALARM_PUSH_LOG_SEVERITY %u\n", LOG_SEVERITY);

  strncpy(env_key, "EPICS_ALARM_PUSH_REDIS_STREAM", sizeof(env_key));
  strncpy(env_def, "acorn:alarms", sizeof(env_def));
  envGetConfigParam(&envp, sizeof(REDIS_STREAM), REDIS_STREAM);

  strncpy(env_key, "EPICS_ALARM_PUSH_REDIS_HOST", sizeof(env_key));
  env_def[0] = 0;   //  no default
  char redis_host[64] = { 0 };
  envGetConfigParam(&envp, sizeof(redis_host), redis_host);

  //  if no EPICS_ALARM_PUSH_REDIS_HOST was found, go no further
  //  this will prevent the DB event hooks from being registered
  //  so no alarm push functionality will occur
  if (redis_host[0] == 0)
  {
    errlogPrintf("EPICS_ALARM_PUSH_REDIS_HOST not found - AlarmPush disabled\n");
    return 0;
  }
  strncpy(env_key, "EPICS_ALARM_PUSH_REDIS_PORT", sizeof(env_key));
  strncpy(env_def, "6379", sizeof(env_def));
  char redis_port_str[16] = { 0 };
  envGetConfigParam(&envp, sizeof(redis_port_str), redis_port_str);
  int redis_port = atoi(redis_port_str);

  REDIS_CTX = redisConnect(redis_host, redis_port);
  if (REDIS_CTX == 0)
    { LOGERR("redis host %s | port %i | context %p", redis_host, redis_port, REDIS_CTX); }
  else if (REDIS_CTX->errstr[0])
    { LOGWRN("redis host %s | port %i | context %p | %s", redis_host, redis_port, REDIS_CTX, REDIS_CTX->errstr); }
  else
    { LOGINF("redis host %s | port %i | context %p", redis_host, redis_port, REDIS_CTX); }

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
