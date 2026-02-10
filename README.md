# dbracey-ioc-alarm-push-proto3
Prototype IOC alarm pusher using Redis in a support module

## Environment Variables
Set these environment variables to provide Redis connection information.

- EPICS_ALARM_PUSH_REDIS_HOST
- EPICS_ALARM_PUSH_REDIS_PORT

## IOC Integration
In the IOC's `myIOCApp/src/Makefile`:

- Add the database definition file for alarmPush:
```
  myIOC_DBD += alarmPush.dbd
```
- Add the support module library for alarmPush:
```
  myIOC_LIBS += alarmPush
```
