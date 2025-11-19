// SPDX-License-Identifier: GPL-2.0+
/*
 *  drivers/soc/amlogic/bluetooth/bt_device.c
 *
 *  Minimal, Android-14 compatible version of the original Amlogic BT rfkill driver.
 *  All deprecated APIs removed, modern gpiod used, no external headers.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/rfkill.h>
#include <linux/gpio/consumer.h>
#include <linux/of.h>
#include <linux/random.h>

#define BT_RFKILL "bt_rfkill"

struct bt_dev_data {
    struct gpio_desc *reset_gpio;
    struct gpio_desc *en_gpio;
    struct gpio_desc *hostwake_gpio;
    bool power_low_level;
    bool power_on_pin_OD;
    u32 power_off_flag;
    u32 power_down_disable;
};

struct bt_dev_runtime_data {
    struct rfkill *bt_rfk;
    struct bt_dev_data *pdata;
};

/* --------------------------------------------------------------------- */
/*  Power control helpers (reset / enable GPIOs)                         */
static void bt_device_off(struct bt_dev_data *d)
{
    if (d->power_down_disable || !d->power_off_flag)
        return;

    if (d->reset_gpio) {
        if (d->power_on_pin_OD && d->power_low_level)
            gpiod_direction_input(d->reset_gpio);
        else
            gpiod_direction_output(d->reset_gpio, d->power_low_level);
    }
    if (d->en_gpio) {
        if (d->power_on_pin_OD && d->power_low_level)
            gpiod_direction_input(d->en_gpio);
        else
            gpiod_direction_output(d->en_gpio, d->power_low_level);
    }
    msleep(20);
}

static void bt_device_on(struct bt_dev_data *d)
{
    if (!d->power_down_disable) {
        if (d->reset_gpio) {
            if (d->power_on_pin_OD && d->power_low_level)
                gpiod_direction_input(d->reset_gpio);
            else
                gpiod_direction_output(d->reset_gpio, d->power_low_level);
        }
        if (d->en_gpio) {
            if (d->power_on_pin_OD && d->power_low_level)
                gpiod_direction_input(d->en_gpio);
            else
                gpiod_direction_output(d->en_gpio, d->power_low_level);
        }
        msleep(200);
    }

    if (d->reset_gpio) {
        if (d->power_on_pin_OD && !d->power_low_level)
            gpiod_direction_input(d->reset_gpio);
        else
            gpiod_direction_output(d->reset_gpio, !d->power_low_level);
    }
    if (d->en_gpio) {
        if (d->power_on_pin_OD && !d->power_low_level)
            gpiod_direction_input(d->en_gpio);
        else
            gpiod_direction_output(d->en_gpio, !d->power_low_level);
    }
    msleep(200);
}

/* --------------------------------------------------------------------- */
static int bt_set_block(void *data, bool blocked)
{
    struct bt_dev_data *d = data;

    pr_error("BT_RADIO going: %s\n", blocked ? "off" : "on");

    if (!blocked)
        bt_device_on(d);
    else
        bt_device_off(d);

    return 0;
}

static const struct rfkill_ops bt_rfkill_ops = {
    .set_block = bt_set_block,
};

/* --------------------------------------------------------------------- */
static int bt_probe(struct platform_device *pdev)
{
    struct bt_dev_data *d;
    struct bt_dev_runtime_data *rtd;
    struct rfkill *rfk;
    int ret;

    d = devm_kzalloc(&pdev->dev, sizeof(*d), GFP_KERNEL);
    if (!d)
        return -ENOMEM;

    /* ---- GPIO acquisition (modern gpiod) ---- */
    d->reset_gpio = devm_gpiod_get_optional(&pdev->dev, "reset", GPIOD_OUT_LOW);
    if (IS_ERR(d->reset_gpio))
        return dev_err_probe(&pdev->dev, PTR_ERR(d->reset_gpio),
                             "failed to get reset gpio\n");

    d->en_gpio = devm_gpiod_get_optional(&pdev->dev, "bt-en", GPIOD_OUT_LOW);
    if (IS_ERR(d->en_gpio))
        return dev_err_probe(&pdev->dev, PTR_ERR(d->en_gpio),
                             "failed to get bt-en gpio\n");

    d->hostwake_gpio = devm_gpiod_get_optional(&pdev->dev, "hostwake", GPIOD_OUT_HIGH);
    if (IS_ERR(d->hostwake_gpio))
        return dev_err_probe(&pdev->dev, PTR_ERR(d->hostwake_gpio),
                             "failed to get hostwake gpio\n");

    /* ---- DT properties ---- */
    d->power_low_level = of_property_read_bool(pdev->dev.of_node, "power_low_level");
    of_property_read_u32(pdev->dev.of_node, "power_on_pin_OD", &d->power_on_pin_OD);
    of_property_read_u32(pdev->dev.of_node, "power_off_flag", &d->power_off_flag);
    of_property_read_u32(pdev->dev.of_node, "power_down_disable", &d->power_down_disable);

    if (!d->power_off_flag)
        d->power_off_flag = 1;
    if (!d->power_on_pin_OD)
        d->power_on_pin_OD = 0;

    /* ---- rfkill registration ---- */
    rfk = rfkill_alloc("bt-dev", &pdev->dev, RFKILL_TYPE_BLUETOOTH,
                       &bt_rfkill_ops, d);
    if (!rfk)
        return -ENOMEM;

    rfkill_init_sw_state(rfk, true);   /* default off */
    ret = rfkill_register(rfk);
    if (ret) {
        rfkill_destroy(rfk);
        return ret;
    }

    rtd = devm_kzalloc(&pdev->dev, sizeof(*rtd), GFP_KERNEL);
    if (!rtd) {
        rfkill_unregister(rfk);
        rfkill_destroy(rfk);
        return -ENOMEM;
    }
    rtd->bt_rfk = rfk;
    rtd->pdata  = d;
    platform_set_drvdata(pdev, rtd);

    pr_error("Amlogic BT rfkill registered\n");
    return 0;
}

static int bt_remove(struct platform_device *pdev)
{
    struct bt_dev_runtime_data *rtd = platform_get_drvdata(pdev);

    if (rtd) {
        if (rtd->bt_rfk) {
            rfkill_unregister(rtd->bt_rfk);
            rfkill_destroy(rtd->bt_rfk);
        }
        /* gpiod are devm-managed, no explicit free */
    }
    return 0;
}

/* --------------------------------------------------------------------- */
static const struct of_device_id bt_dev_dt_match[] = {
    { .compatible = "amlogic,aml-bt" },
    { }
};
MODULE_DEVICE_TABLE(of, bt_dev_dt_match);

static struct platform_driver bt_driver = {
    .probe  = bt_probe,
    .remove = bt_remove,
    .driver = {
        .name           = "aml_bt",
        .of_match_table = bt_dev_dt_match,
    },
};

module_platform_driver(bt_driver);

MODULE_DESCRIPTION("Amlogic BT rfkill driver (Android-14 compatible)");
MODULE_LICENSE("GPL");
