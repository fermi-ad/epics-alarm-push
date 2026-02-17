#include <errlog.h>
#include <dbAccess.h>
#include <dbStaticLib.h>
#include <epicsExport.h>
#include <envDefs.h>
#include <alarm.h>

#include <string.h>

#include "hiredis.h"

static uint LOG_SEVERITY = 0;
static char REDIS_STREAM[64] = { 0 };
static redisContext* REDIS_CTX = 0;

#define ERRLOG(sev, ...) ((uint)sev < LOG_SEVERITY ? 0 : errlogSevPrintf(sev, __VA_ARGS__))

static void my_alarm_func(void* user_arg, dbChannel* chan, int eventsRemaining, db_field_log* pfl)
{
  if (chan && chan->addr.precord && pfl)
  {
    ERRLOG(errlogInfo, "\nredis %p | stream %s | chan %s | recd %s | stat %i | sevr %i",
        REDIS_CTX, REDIS_STREAM, chan->name, chan->addr.precord->name, pfl->stat, pfl->sevr);
    if (pfl->amsg[0])
      { ERRLOG(errlogInfo, " | amsg %s\n", pfl->amsg); }
    else
      { ERRLOG(errlogInfo, "\n"); }

    if (REDIS_CTX)
    {
      const char* stat_str = (pfl->stat < ALARM_NSTATUS)
          ? epicsAlarmConditionStrings[pfl->stat]
          : "INVALID";
      const char* sevr_str = (pfl->sevr < ALARM_NSEV)
          ? epicsAlarmSeverityStrings[pfl->sevr]
          : epicsAlarmSeverityStrings[epicsSevInvalid];

      char cmd[1024];
      sprintf(cmd, "XADD %s MAXLEN 99999 * device '%s' type %s severity %s",
          REDIS_STREAM, chan->addr.precord->name, stat_str, sevr_str);

      //  TODO: add a message if none is present on pfl->amsg
      if (pfl->amsg[0])
        { strcat(cmd, " message '"); strcat(cmd, pfl->amsg); strcat(cmd, "'"); }

      redisReply* reply = (redisReply*)redisCommand(REDIS_CTX, cmd);

      if (reply == 0 || reply->type == REDIS_REPLY_ERROR)
      {
        static time_t prev = 0;   // throttle reconnects
        time_t curr = time(0);

        if (curr > prev)
        {
          prev = curr;
          int result = redisReconnect(REDIS_CTX);
          ERRLOG(errlogMajor, "error %s | %s | reconnect %i\n", reply ? reply->str : 0, REDIS_CTX->errstr, result);
        }
        else
          { ERRLOG(errlogMinor, "error %s | %s | defer\n", reply ? reply->str : 0, REDIS_CTX->errstr); }
      }
      else
        { ERRLOG(errlogInfo, "reply %s\n", reply->str); }

      freeReplyObject(reply);   //  ok to free 0
    }
  }
}

static long init_device(int pass)
{
  if (pass == 1 && REDIS_CTX == 0)
  {
    char env_key[64], env_def[64];
    ENV_PARAM envp = { env_key, env_def };

    strncpy(env_key, "EPICS_ALARM_PUSH_LOG_SEVERITY", sizeof(env_key));
    strncpy(env_def, "-1", sizeof(env_def));
    char log_sev_str[16];
    envGetConfigParam(&envp, sizeof(log_sev_str), log_sev_str);
    LOG_SEVERITY = (uint)atoi(log_sev_str);

    strncpy(env_key, "EPICS_ALARM_PUSH_REDIS_STREAM", sizeof(env_key));
    strncpy(env_def, "acorn:alarms", sizeof(env_def));
    envGetConfigParam(&envp, sizeof(REDIS_STREAM), REDIS_STREAM);

    strncpy(env_key, "EPICS_ALARM_PUSH_REDIS_HOST", sizeof(env_key));
    strncpy(env_def, "localhost", sizeof(env_def));
    char redis_host[64];
    envGetConfigParam(&envp, sizeof(redis_host), redis_host);

    strncpy(env_key, "EPICS_ALARM_PUSH_REDIS_PORT", sizeof(env_key));
    strncpy(env_def, "6379", sizeof(env_def));
    char redis_port_str[16];
    envGetConfigParam(&envp, sizeof(redis_port_str), redis_port_str);
    int redis_port = atoi(redis_port_str);

    REDIS_CTX = redisConnect(redis_host, redis_port);
    if (REDIS_CTX == 0)
      { ERRLOG(errlogMajor, "redis host %s | port %i | context %p", redis_host, redis_port, REDIS_CTX); }
    else if (REDIS_CTX->errstr[0])
      { ERRLOG(errlogMajor, "redis host %s | port %i | context %p | %s", redis_host, redis_port, REDIS_CTX, REDIS_CTX->errstr); }
    else
      { ERRLOG(errlogInfo, "redis host %s | port %i | context %p", redis_host, redis_port, REDIS_CTX); }

    auto ctx = db_init_events();
    auto dse = db_start_events(ctx, 0, 0, 0, epicsThreadPriorityMedium);
    ERRLOG(errlogInfo, "ctx %p | dse %i\n", ctx, dse);

    DBENTRY ent; dbInitEntry(pdbbase, &ent);
    for (auto st1 = dbFirstRecordType(&ent); st1 == 0; st1 = dbNextRecordType(&ent))
    {
      for (auto st2 = dbFirstRecord(&ent); st2 == 0; st2 = dbNextRecord(&ent))
      {
          ERRLOG(errlogInfo, "dbRecordType %p | typename %s\n", ent.precordType, ent.precordType ? ent.precordType->name : 0);
          ERRLOG(errlogInfo, "dbRecordNode %p | recordname %s\n", ent.precnode, ent.precnode ? ent.precnode->recordname : 0);

          auto chn = dbChannelCreate(ent.precnode->recordname);
          auto alm = db_add_event(ctx, chn, my_alarm_func, 0, DBE_ALARM);
          ERRLOG(errlogInfo, "chn %p | alm %p\n", chn, alm);

          db_event_enable(alm);
      }
    }
  }
  return 0;
}

static dset alarmPushDevice = { 4, 0, (DEVSUPFUN)init_device, 0, 0 };

#define EPICS_EXPORT_ADDRESS(...) epicsExportAddress(__VA_ARGS__);

EPICS_EXPORT_ADDRESS(dset, alarmPushDevice)
