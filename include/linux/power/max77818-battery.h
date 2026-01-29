/*
 * Copyright (C) 2019 Maxim Integrated
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __MAX77818_BATTERY_H_
#define __MAX77818_BATTERY_H_

struct max77818_fg_platform_data {
	int	soc_alert_threshold;
};

struct max77818_chip {
	struct device           	*dev;
	struct max77818_dev     	*max77818;
	struct regmap			*regmap;
	int				fg_irq;
	struct delayed_work		work;
	struct power_supply		*battery;
	struct power_supply_desc	psy_batt_d;
	struct power_supply 		*charger_psy;
	struct power_supply 		*ac_psy;

	/* mutex */
	struct mutex			lock;

	/* alert */
	int alert_threshold;

	/* State Of Connect */
	int ac_online;
	int usb_online;
	/* battery voltage */
	int vcell;
	/* battery capacity */
	int soc;
	/* State Of Charge */
	int status;
	/* battery health */
	int health;
	/* battery capacity */
	int capacity_level;

	int lasttime_vcell;
	int lasttime_soc;
	int lasttime_status;

	struct max77818_fg_platform_data	*pdata;
};

#endif
