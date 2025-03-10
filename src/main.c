#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>

#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/sys/printk.h>

#include <zephyr/drivers/display.h>
#include <zephyr/display/cfb.h>
#include <string.h>

/* 1000 msec = 1 sec, 500 = 0.5 sec */
#define SLEEP_TIME_MS   1000

#define ONBOARD_LED_NODE    DT_ALIAS(led0)

#define EXTERNAL_RED_LED_NODE       DT_ALIAS(led1)
#define EXTERNAL_YELLOW_LED_NODE    DT_ALIAS(led2)
#define EXTERNAL_GREEN_LED_NODE     DT_ALIAS(led3)

#define ONBOARD_BUTTON_NODE DT_ALIAS(sw0)

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(ONBOARD_LED_NODE, gpios);

static const struct gpio_dt_spec ext_red_led    = GPIO_DT_SPEC_GET(EXTERNAL_RED_LED_NODE, gpios);
static const struct gpio_dt_spec ext_yellow_led = GPIO_DT_SPEC_GET(EXTERNAL_YELLOW_LED_NODE, gpios);
static const struct gpio_dt_spec ext_green_led  = GPIO_DT_SPEC_GET(EXTERNAL_GREEN_LED_NODE, gpios);

static const struct gpio_dt_spec onboard_button = GPIO_DT_SPEC_GET_OR(
    ONBOARD_BUTTON_NODE,
    gpios,
    {0});

static struct gpio_callback button_cb_data;

// static struct device *bme280 = DEVICE_DT_GET(DT_NODELABEL(bme280));

static const struct device *sensor_dev = DEVICE_DT_GET(DT_NODELABEL(bme280));

#define I2C_OLED_DISPLAY    DT_NODELABEL(ssd1306)

static const struct device *oled_display_dev = DEVICE_DT_GET(I2C_OLED_DISPLAY);

void button_pressed(
    const struct device *dev,
    struct gpio_callback *cb,
    uint32_t pins)
{
    // we need to debounce the button switch interrupt signal
    printf("CB INT: Button pressed at %" PRIu32 "\n", k_cycle_get_32());
}

int main(void)
{
    int ret_value = 0;
    int ret = 0;
    uint32_t counter = 0;
    bool onboard_led_state = true;
    struct sensor_value temp, pressure, humidity;
    char display_buf[32] = {0};

    printf("Blackpill firmware is running!\n");

    if (!device_is_ready(DEVICE_DT_GET(DT_NODELABEL(i2c1))))
    {
        printf("I2C is not ready\n");
        return 0;
    }
    else
    {
        printf("I2C is ready\n");
    }

    if (!gpio_is_ready_dt(&led))
    {
        return 0;
    }

    if ((!gpio_is_ready_dt(&ext_red_led)) &&
        (!gpio_is_ready_dt(&ext_yellow_led)) &&
        (!gpio_is_ready_dt(&ext_green_led)) &&
        (!gpio_is_ready_dt(&onboard_button))
    )
    {
        return 0;
    }

    if (!device_is_ready(sensor_dev))
    {
        printf("BME280 device is not ready\n");
        return 0;
    }

    if (!device_is_ready(oled_display_dev))
    {
        printf("SSD1306 device is not ready\n");
        return 0;
    }

    if (cfb_framebuffer_init(oled_display_dev) < 0)
    {
        printf("Framebuffer initialization failed\n");
        return 0;
    }

    ret_value = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);

    if (ret_value < 0)
    {
        return 0;
    }

    ret_value = gpio_pin_configure_dt(&ext_red_led, GPIO_OUTPUT_ACTIVE);

    if (ret_value < 0)
    {
        return 0;
    }

    ret_value = gpio_pin_configure_dt(&ext_yellow_led, GPIO_OUTPUT_ACTIVE);

    if (ret_value < 0)
    {
        return 0;
    }

    ret_value = gpio_pin_configure_dt(&ext_green_led, GPIO_OUTPUT_ACTIVE);

    if (ret_value < 0)
    {
        return 0;
    }

    ret_value = gpio_pin_configure_dt(&onboard_button, GPIO_INPUT);

    if (ret_value < 0)
    {
        return 0;
    }

    // we enable gpio interrupts
	ret_value = gpio_pin_interrupt_configure_dt(&onboard_button,
        GPIO_INT_EDGE_TO_ACTIVE);

    if (ret_value != 0)
    {
        printf("Error %d: failed to configure interrupt on %s pin %d\n",
            ret_value,
            onboard_button.port->name,
            onboard_button.pin);

        return 0;
    }

    gpio_init_callback(&button_cb_data, button_pressed, BIT(onboard_button.pin));
    gpio_add_callback(onboard_button.port, &button_cb_data);

    printf("Set up button at %s pin %d\n", onboard_button.port->name, onboard_button.pin);

    printf("BME280 device is ready\n");

    // Clear the display
    display_blanking_off(oled_display_dev);
    cfb_framebuffer_clear(oled_display_dev, true);

    // set font (8x16 is included by default)
    cfb_framebuffer_set_font(oled_display_dev, 0);

    while (true)
    {
        switch (counter)
        {
            case 0:
                // gpio_pin_toggle_dt(&ext_red_led);
                gpio_pin_set_dt(&ext_red_led, 1);
                gpio_pin_set_dt(&ext_yellow_led, 0);
                gpio_pin_set_dt(&ext_green_led, 0);
                counter = 1;
                break;
            case 1:
                // gpio_pin_toggle_dt(&ext_yellow_led);
                gpio_pin_set_dt(&ext_red_led, 0);
                gpio_pin_set_dt(&ext_yellow_led, 1);
                gpio_pin_set_dt(&ext_green_led, 0);
                counter = 2;
                break;
            case 2:
                // gpio_pin_toggle_dt(&ext_green_led);
                gpio_pin_set_dt(&ext_red_led, 0);
                gpio_pin_set_dt(&ext_yellow_led, 0);
                gpio_pin_set_dt(&ext_green_led, 1);
                counter = 0;
                break;
            default:
                counter = 1;
                break;
        }

        ret_value = gpio_pin_toggle_dt(&led);

        if (ret_value < 0)
        {
            return 0;
        }
        onboard_led_state = !onboard_led_state;

        // printk("k LED state %d: %s\n", counter, onboard_led_state ? "ON" : "OFF");
        printf("LED state: %s\n", onboard_led_state ? "ON" : "OFF");

        // onboard button polling example
        // if (gpio_pin_get_dt(&onboard_button))
        // {
        //     printf("Button is pressed\n");
        // }

        // k_msleep(SLEEP_TIME_MS);

        sensor_sample_fetch(sensor_dev);

        sensor_channel_get(sensor_dev, SENSOR_CHAN_AMBIENT_TEMP, &temp);
        sensor_channel_get(sensor_dev, SENSOR_CHAN_PRESS, &pressure);
        sensor_channel_get(sensor_dev, SENSOR_CHAN_HUMIDITY, &humidity);

        printf("Temperature: %.2f C\n", sensor_value_to_double(&temp));
        // printf("Temperature: %d.%06d °C\n", temp.val1, temp.val1);
        printf("Pressure: %.2f kPa\n", sensor_value_to_double(&pressure) / 1000);
        printf("Humidity: %.2f %%\n", sensor_value_to_double(&humidity));

        snprintf(display_buf, sizeof(display_buf), "T: %.2f C", sensor_value_to_double(&temp));
        // display_print(oled_display_dev, 0, 0, display_buf);
        cfb_print(oled_display_dev, display_buf, 10, 10);

        snprintf(display_buf, sizeof(display_buf), "P: %.2f kPa", sensor_value_to_double(&pressure) / 1000);
        // display_print(oled_display_dev, 0, 16, display_buf);
        cfb_print(oled_display_dev, display_buf, 10, 30);

        snprintf(display_buf, sizeof(display_buf), "H: %.2f %%", sensor_value_to_double(&humidity));
        // display_print(oled_display_dev, 0, 32, display_buf);
        cfb_print(oled_display_dev, display_buf, 10, 50);

        cfb_framebuffer_finalize(oled_display_dev);

        k_sleep(K_MSEC(SLEEP_TIME_MS));
    }

    return 0;
}
 