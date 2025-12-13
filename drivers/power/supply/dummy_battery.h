#ifndef _DUMMY_BATTERY_H_
#define _DUMMY_BATTERY_H_

#include <linux/types.h>

#ifdef __KERNEL__
void dummy_battery_set_usb_online(bool online);
#endif

#endif /* _DUMMY_BATTERY_H_ */

