#include <dbBase.h>
#include <epicsExport.h>
#include <errlog.h>

#define EPICS_EXPORT_ADDRESS(...) epicsExportAddress(__VA_ARGS__);

static long init_device(int pass)
{
  if (pass == 1)
  {
    errlogPrintf("alarmPush init_device | pass %i\n", pass);
  }
  return 0;
}

static dset alarmPushDevice = { 4, 0, (DEVSUPFUN)init_device, 0, 0 };

EPICS_EXPORT_ADDRESS(dset, alarmPushDevice)
