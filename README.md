# epics-alarm-push

PV alarm pusher using Redis in an IOC support module

## Environment Variables

Set these environment variables to provide Redis connection information:

- `EPICS_ALARM_PUSH_REDIS_HOST`   (no default - alarm push disabled if not defined)
- `EPICS_ALARM_PUSH_REDIS_PORT`   (default `6379`)

The stream key may also be specified:

- `EPICS_ALARM_PUSH_REDIS_STREAM` (default `acorn:alarms`)

Logging must be explicitly enabled (set to a value from `errlogSevEnum`):

- `EPICS_ALARM_PUSH_LOG_SEVERITY` (default undefined)

## IOC Integration

In the IOC's `configure/RELEASE` or `configure/RELEASE.local`:

- Add the path for the AlarmPush top directory:
```
  ALARM_PUSH=$(EPICS_SUPPORT)/AlarmPush
```

In the IOC's `myIOCApp/src/Makefile`:

- Add the database definition file for AlarmPush:
```
  myIOC_DBD += AlarmPush.dbd
```

- Add the support module library for AlarmPush:
```
  myIOC_LIBS += AlarmPush
```

## Theory of Operation

A driver is implemented to provide an initialization function that is called during IOC initialization:
```
driver(AlarmPush)
```
The driver initialization function configures a Redis server connection using information from environment variables, then sets an `iocInit` hook for a second initialization function to be called by the IOC initialization process.

This second initialization function must wait until after the IOC database is initialized (`initHookAfterInitDatabase`) so that it can iterate through all database records and install an alarm event callback (`DBE_ALARM`).

This alarm event callback will post the information about any record's alarm state change to a Redis message queue for use by any observers.

### Redis Schema

#### Redis `STREAM` data type

Key       | Example Value           | Description
----------|-------------------------|------------
timestamp | 1774477060              | time_t (seconds) when alarm changed
device    | acq2106_347:SFP:1:TEMP  | PV name
source    | HIHI                    | EPICS [alarm status string](https://docs.epics-controls.org/projects/base/en/r7.0.9/menuAlarmStat.html)
severity  | MAJOR                   | EPICS [alarm severity string](https://docs.epics-controls.org/projects/base/en/r7.0.9/menuAlarmSevr.html)
message   | value has exceeded HIHI | Text describing the alarm specifics (optional)
