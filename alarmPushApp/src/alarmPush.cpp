#include <errlog.h>
#include <dbAccess.h>
#include <dbStaticLib.h>
#include <epicsExport.h>
#include <envDefs.h>

#include "hiredis.h"

#define EPICS_EXPORT_ADDRESS(...) epicsExportAddress(__VA_ARGS__);

static redisContext* redis = 0;

static void my_alarm_func(void *user_arg, struct dbChannel *chan, int eventsRemaining, struct db_field_log *pfl)
{
  errlogPrintf("\nredis %p | chan %s | recd %s | stat %i | sevr %i\n",
      redis, chan->name, chan->addr.precord->name, pfl->stat, pfl->sevr);

  if (redis)
  {
    redisReply* reply = (redisReply*)redisCommand(redis,
        "XADD acorn:epics:alarms MAXLEN 99999 * device %s status %i severity %i",
        chan->addr.precord->name, pfl->stat, pfl->sevr);

    if (reply == 0 || reply->type == REDIS_REPLY_ERROR)
    {
      // throttle reconnects
      static time_t prev = 0;
      time_t curr = time(0);
      if (curr > prev)
      {
        prev = curr;
        int result = redisReconnect(redis);
        errlogPrintf("error %s | %s | reconnect %i\n", reply ? reply->str : 0, redis->errstr, result);
      }
      else
      {
        errlogPrintf("error %s | %s | defer\n", reply ? reply->str : 0, redis->errstr);
      }
    }
    else
    {
      errlogPrintf("reply %s\n", reply->str);
    }
    freeReplyObject(reply);   //  ok to free 0
  }
}

static long init_device(int pass)
{
  if (pass == 1 && redis == 0)
  {
    char redis_host_key[] = "EPICS_ALARM_PUSH_REDIS_HOST";
    char redis_host_default[] = "127.0.0.1";
    char redis_host[64];
    ENV_PARAM envp = { redis_host_key, redis_host_default };
    envGetConfigParam(&envp, sizeof(redis_host), redis_host);

    char redis_port_key[] = "EPICS_ALARM_PUSH_REDIS_PORT";
    char redis_port_default[] = "6379";
    char redis_port_str[16];
    envp = { redis_port_key, redis_port_default };
    envGetConfigParam(&envp, sizeof(redis_port_str), redis_port_str);
    int redis_port = atoi(redis_port_str);

    redis = redisConnect(redis_host, redis_port);
    errlogPrintf("redis host %s | port %i | context %p | %s\n",
        redis_host, redis_port, redis, redis && redis->errstr[0] ? redis->errstr : "ok");

    auto ctx = db_init_events();
    auto dse = db_start_events(ctx, 0, 0, 0, epicsThreadPriorityMedium);
    errlogPrintf("ctx %p | dse %i\n", ctx, dse);

    DBENTRY ent; dbInitEntry(pdbbase, &ent);
    for (auto st1 = dbFirstRecordType(&ent); st1 == 0; st1 = dbNextRecordType(&ent))
    {
      for (auto st2 = dbFirstRecord(&ent); st2 == 0; st2 = dbNextRecord(&ent))
      {
          errlogPrintf("dbRecordType %p | typename %s\n", ent.precordType, ent.precordType ? ent.precordType->name : 0);
          errlogPrintf("dbRecordNode %p | recordname %s\n", ent.precnode, ent.precnode ? ent.precnode->recordname : 0);

          auto chn = dbChannelCreate(ent.precnode->recordname);
          auto alm = db_add_event(ctx, chn, my_alarm_func, 0, DBE_ALARM);
          errlogPrintf("chn %p | alm %p\n", chn, alm);

          db_event_enable(alm);
      }
    }
  }
  return 0;
}

static dset alarmPushDevice = { 4, 0, (DEVSUPFUN)init_device, 0, 0 };

EPICS_EXPORT_ADDRESS(dset, alarmPushDevice)
