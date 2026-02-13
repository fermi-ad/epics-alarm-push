# dbracey-ioc-alarm-push-proto3

Prototype IOC alarm pusher using Redis in a support module

## Environment Variables

Set these environment variables to provide Redis connection information.

- `EPICS_ALARM_PUSH_REDIS_HOST`   (default `127.0.0.1`)
- `EPICS_ALARM_PUSH_REDIS_PORT`   (default `6379`)

The stream key may also be specified.

- `EPICS_ALARM_PUSH_REDIS_KEY`    (default `acorn:alarms`)

## IOC Integration

In the IOC's `configure/RELEASE` or `configure/RELEASE.local`:

- Add the path for the alarmPush's top directory:
```
  ALARM_PUSH=/home/dbracey/dbracey-ioc-alarm-push-proto3
```

In the IOC's `myIOCApp/src/Makefile`:

- Add the database definition file for alarmPush:
```
  myIOC_DBD += alarmPush.dbd
```
- Add the support module library for alarmPush:
```
  myIOC_LIBS += alarmPush
```

## Theory of Operation

### Redis Schema

Key      | Example Value           | Description
---------|-------------------------|------------
device   | acq2106_347:SFP:1:TEMP  | PV name
type     | HIHI                    | EPICS [alarm status choice string](https://docs.epics-controls.org/projects/base/en/r7.0.9/menuAlarmStat.html) or ANALOG (ACNET) or DIGITAL (ACNET)
severity | MAJOR                   | EPICS [alarm severity choice string](https://docs.epics-controls.org/projects/base/en/r7.0.9/menuAlarmSevr.html)
message  | value has exceeded HIHI | Text describing the alarm specifics
