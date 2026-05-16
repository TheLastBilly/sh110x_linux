#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/kernel.h>
#include <linux/sysfs.h>
#include <linux/of_device.h>

#include <linux/minmax.h>

#include "font.h"

#define UNUSED(x) (void)(x)

#define SH1107_CMD_COLUMN_ADDR_LOW(_a)      (0x00 | ((uint8_t) (0x0f & (_a))))
#define SH1107_CMD_COLUMN_ADDR_HIGH(_a)     (0x10 | ((uint8_t) (0x07 & (_a))))
#define SH1107_CMD_SET_ADDR_MODE(_m)        (0x20 | ((uint8_t) ((_m) > 0 ? 0x01 : 0x00)))
#define SH1107_CMD_CONTRAST_SET             0x81
#define SH1107_CMD_CONTRAST_DATA(_c)        (0x00 | ((uint8_t) (0xff & (_c))))
#define SH1107_CMD_SET_ADC(_a)              (0xa0 | ((uint8_t) ((_a) > 0 ? 0x01 : 0x00)))
#define SH1107_CMD_MULTIPLEX_RATION_SET     0xa8
#define SH1107_CMD_MULTIPLEX_RATION_VAL(_v) ((uint8_t) (0x7f & (_v)))
#define SH1107_CMD_SET_ENT_DSP_ON_OFF(_v)   (0xa4 | ((uint8_t) ((_v) > 0 ? 0x01 : 0x00)))
#define SH1107_CMD_SET_NORM_REV_DSP(_v)     (0xa6 | ((uint8_t) ((_v) > 0 ? 0x01 : 0x00)))
#define SH1107_CMD_SET_DISPLAY_OFFSET       0xd3
#define SH1107_CMD_DISPLAY_OFFSET(_o)       ((uint8_t) (0x7f & (_o)))
#define SH1107_CMD_DC_DC_CONTROL_SET        0xad
#define SH1107_CMD_DC_DC_CONTROL_VAL(_c)    (0x80 | ((uint8_t) (0x0f & (_c))))
#define SH1107_CMD_SET_DSP_ON_OFF(_v)       (0xae | ((uint8_t) ((_v) > 0 ? 0x01 : 0x00)))
#define SH1107_CMD_SET_PAGE_ADDR(_a)        (0xb0 | ((uint8_t) (0x0f & (_a))))
#define SH1107_CMD_SET_COMN_OUT_DIR(_d)     (0xc0 | ((uint8_t) ((_d) > 0 ? 0x80 : 0x00)))
#define SH1107_CMD_RAT_OSC_SET              0xd5
#define SH1107_CMD_RAT_OSC_VAL(_f, _r)      ((((uint8_t) ((_f) & 0xf)) << 4) | ((uint8_t) ((_r) & 0xf)))
#define SH1107_CMD_DIS_PRE_CHARGE_SET       0xd9
#define SH1107_CMD_DIS_PRE_CHARGE(_d, _p)   ((((uint8_t) ((_d) & 0xf)) << 4) | ((uint8_t) ((_p) & 0xf)))
#define SH1107_CMD_VCOM_DESELECT_SET        0xdb
#define SH1107_CMD_VCOM_DESELECT_VAL(_v)    ((uint8_t) (0xff & (_v)))
#define SH1107_CMD_DISPLAY_START_SET        0xdc
#define SH1107_CMD_DISPLAY_START_VAL(_v)    ((uint8_t) (0x7f & (_v)))

#define DISPLAY_PAGE_MAX    16
#define DISPLAY_COLUMN_MAX  128
#define DISPLAY_MAX_BUFFER  (DISPLAY_PAGE_MAX * DISPLAY_COLUMN_MAX)
#define DISPLAY_MAX_LINES   ((DISPLAY_COLUMN_MAX+1)/(SH110X_FONT_HEIGHT+1))

struct sh1107_data {
    struct device *dev;
    struct i2c_client *client;
    struct mutex client_mutex;

    struct device_attribute * text;
    struct device_attribute * cursor;

    uint8_t line_num;
    uint8_t cursor_pos;
    bool invert;
};

static int i2c_command(struct i2c_client *client, const uint8_t cmd)
{
    struct sh1107_data * sh1107 = i2c_get_clientdata(client);
    int rc = 0;
    if (client == NULL) {
        return -EINVAL;
    }

    mutex_lock(&sh1107->client_mutex);
    rc = i2c_smbus_write_byte_data(client, 0x00, cmd);
    mutex_unlock(&sh1107->client_mutex);

    return rc;
}

static int i2c_command_arg(struct i2c_client *client, const uint8_t cmd, const uint8_t arg)
{
    int rc = 0;

    rc = i2c_command(client, cmd);
    if (rc == 0) {
        rc = i2c_command(client, arg);
        if (rc != 0) {
            dev_info(&client->dev, "failed to send arg (0x%02x): %d\n", arg, rc);
        }
    } else {
        dev_info(&client->dev, "failed to send cmd (0x%02x): %d\n", cmd, rc);
    }

    return rc;
}

static int sh1107_set_column_address(struct i2c_client *client, uint8_t addr)
{
    int rc = 0;

    rc = i2c_command_arg(client, SH1107_CMD_COLUMN_ADDR_LOW(addr), SH1107_CMD_COLUMN_ADDR_HIGH(addr >> 4));
    if (rc != 0) {
        dev_info(&client->dev, "failed to set column address to 0x%02x: %d\n", addr, rc);
    }

    return rc;
}


static int sh1107_set_addressing_mode(struct i2c_client *client, uint8_t mode)
{
    int rc = 0;

    rc = i2c_command(client, SH1107_CMD_SET_ADDR_MODE(mode));
    if (rc != 0) {
        dev_info(&client->dev, "failed to set addressing mode to 0x%02x: %d\n", mode, rc);
    }

    return rc;
}

static int sh1107_set_contrast(struct i2c_client *client, uint8_t contrast)
{
    int rc = 0;

    rc = i2c_command_arg(client, SH1107_CMD_CONTRAST_SET, SH1107_CMD_CONTRAST_DATA(contrast));
    if (rc != 0) {
        dev_info(&client->dev, "failed to set contrast to 0x%02x: %d\n", contrast, rc);
    }

    return rc;
}

static int sh1107_set_adc(struct i2c_client *client, uint8_t adc)
{
    int rc = 0;

    rc = i2c_command(client, SH1107_CMD_SET_ADC(adc));
    if (rc != 0) {
        dev_info(&client->dev, "failed to set adc to 0x%02x: %d\n", adc, rc);
    }

    return rc;
}

static int sh1107_set_multiplex_ration(struct i2c_client *client, uint8_t ration)
{
    int rc = 0;

    rc = i2c_command_arg(client, SH1107_CMD_MULTIPLEX_RATION_SET, SH1107_CMD_MULTIPLEX_RATION_VAL(ration));
    if (rc != 0) {
        dev_info(&client->dev, "failed to set ration to 0x%02x: %d\n", ration, rc);
    }

    return rc;
}

static int sh1107_set_entire_display_on_off(struct i2c_client *client, bool on_off)
{
    int rc = 0;

    rc = i2c_command(client, SH1107_CMD_SET_ENT_DSP_ON_OFF(on_off));
    if (rc != 0) {
        dev_info(&client->dev, "failed to set entire display on/off to 0x%02x: %d\n", on_off, rc);
    }

    return rc;
}

static int sh1107_set_display_offset(struct i2c_client *client, uint8_t offset)
{
    int rc = 0;

    rc = i2c_command_arg(client, SH1107_CMD_SET_DISPLAY_OFFSET, SH1107_CMD_DISPLAY_OFFSET(offset));
    if (rc != 0) {
        dev_info(&client->dev, "failed to set display offset to 0x%02x: %d\n", offset, rc);
    }

    return rc;
}

static int sh1107_set_dc_dc_control(struct i2c_client *client, uint8_t control)
{
    int rc = 0;

    rc = i2c_command_arg(client, SH1107_CMD_DC_DC_CONTROL_SET, SH1107_CMD_DC_DC_CONTROL_VAL(control));
    if (rc != 0) {
        dev_info(&client->dev, "failed to set DC DC control mode to 0x%02x: %d\n", control, rc);
    }

    return rc;
}

static int sh1107_set_display_on_off(struct i2c_client *client, bool on_off)
{
    int rc = 0;

    rc = i2c_command(client, SH1107_CMD_SET_DSP_ON_OFF(on_off));
    if (rc != 0) {
        dev_info(&client->dev, "failed to set display on/off to 0x%02x: %d\n", on_off, rc);
    }

    return rc;
}

static int sh1107_set_page_address(struct i2c_client *client, uint8_t address)
{
    int rc = 0;

    rc = i2c_command(client, SH1107_CMD_SET_PAGE_ADDR(address));
    if (rc != 0) {
        dev_info(&client->dev, "failed to set page address to 0x%02x: %d\n", address, rc);
    }

    return rc;
}

static int sh1107_set_output_scan_direction(struct i2c_client *client, uint8_t dir)
{
    int rc = 0;

    rc = i2c_command(client, SH1107_CMD_SET_COMN_OUT_DIR(dir));
    if (rc != 0) {
        dev_info(&client->dev, "failed to set common output directory to 0x%02x: %d\n", dir, rc);
    }

    return rc;
}

static int sh1107_set_frequency_divide(struct i2c_client *client, uint8_t freq, uint8_t div)
{
    int rc = 0;

    rc = i2c_command_arg(client, SH1107_CMD_RAT_OSC_SET, SH1107_CMD_RAT_OSC_VAL(freq, div));
    if (rc != 0) {
        dev_info(&client->dev, "failed to set frequency divide to 0x%02x and 0x%02x: %d\n", freq, div, rc);
    }

    return rc;
}

static int sh1107_set_charge_duration(struct i2c_client *client, uint8_t dis, uint8_t pre)
{
    int rc = 0;

    rc = i2c_command_arg(client, SH1107_CMD_DIS_PRE_CHARGE_SET, SH1107_CMD_DIS_PRE_CHARGE(dis, pre));
    if (rc != 0) {
        dev_info(&client->dev, "failed to set dis and pre charge to 0x%02x and 0x%02x: %d\n", dis, pre, rc);
    }

    return rc;
}

static int sh1107_set_vcom(struct i2c_client *client, uint8_t vcom)
{
    int rc = 0;

    rc = i2c_command_arg(client, SH1107_CMD_VCOM_DESELECT_SET, SH1107_CMD_VCOM_DESELECT_VAL(vcom));
    if (rc != 0) {
        dev_info(&client->dev, "failed to set vcom to 0x%02x: %d\n", vcom, rc);
    }

    return rc;
}

static int sh1107_set_normal_reverse(struct i2c_client *client, uint8_t mode)
{
    int rc = 0;

    rc = i2c_command(client, SH1107_CMD_SET_NORM_REV_DSP(mode));
    if (rc != 0) {
        dev_info(&client->dev, "failed to set normal/reverse to 0x%02x: %d\n", mode, rc);
    }

    return rc;
}

static int sh1107_set_address_start_line(struct i2c_client *client, uint8_t line)
{
    int rc = 0;

    rc = i2c_command_arg(client, SH1107_CMD_DISPLAY_START_SET, SH1107_CMD_DISPLAY_START_VAL(line));
    if (rc != 0) {
        dev_info(&client->dev, "failed to set display start line to 0x%02x: %d\n", line, rc);
    }

    return rc;
}

static int sh1107_set_cursor(struct i2c_client *client, uint16_t line_num, uint16_t cursor_pos)
{
    struct sh1107_data *sh1107 = i2c_get_clientdata(client);
    int rc = -EINVAL;

    if ((cursor_pos >= DISPLAY_PAGE_MAX) || (line_num >= DISPLAY_MAX_LINES)) {
        dev_err(sh1107->dev, "Cursor out of bounds.");
        return rc;
    }

    sh1107->line_num   = line_num;            // Save the specified line number
    sh1107->cursor_pos = cursor_pos;          // Save the specified cursor position

    uint16_t cursor = DISPLAY_PAGE_MAX - cursor_pos - 1;
    uint16_t line = line_num * (SH110X_FONT_HEIGHT);// + 32;

    // set column start and end address
    rc = sh1107_set_column_address(client, line);
    if (rc == 0) {
        rc = sh1107_set_page_address(client, cursor);
    }
    
    return rc;
}

static int sh1107_invert_font(struct i2c_client *client, bool invert)
{
    struct sh1107_data *sh1107 = i2c_get_clientdata(client);
    int rc = -EINVAL;

    if (sh1107 != NULL) {
        rc = 0;
        sh1107->invert = invert;
    }
    
    return rc;
}

static int sh1107_set_data(struct i2c_client *client, uint8_t data)
{
    struct sh1107_data * sh1107 = i2c_get_clientdata(client);
    int rc = 0;

    if (client == NULL) {
        return -EINVAL;
    }

    mutex_lock(&sh1107->client_mutex);
    rc = i2c_smbus_write_byte_data(client, 0x40, data);
    mutex_unlock(&sh1107->client_mutex);

    return rc;
}

static int blank_screen(struct i2c_client *client, uint8_t pattern)
{
    int rc = 0;
    
    rc = sh1107_set_column_address(client, 0);
    if (rc == 0) {
        rc = sh1107_set_page_address(client, 0);
        if (rc == 0) {
            for (uint8_t s = 0; s < DISPLAY_PAGE_MAX; s++) {
                sh1107_set_page_address(client, s);
                for (uint16_t i = 0; i < DISPLAY_COLUMN_MAX; i++) {
                    sh1107_set_data(client, pattern);
                }
            }
        }
    }

    if (rc == 0) {
        rc = sh1107_set_cursor(client, 0, 0);
    }

    return rc;
}

static int init_display(struct i2c_client *client)
{
    if (client == NULL) {
        return -EINVAL;
    }

    int rc = 0;
    do {
        rc = sh1107_set_display_on_off(client, false);
        if (rc != 0) {
            break;
        }
        rc = sh1107_set_frequency_divide(client, 0x05, 0);
        if (rc != 0) {
            break;
        }
        rc = sh1107_set_addressing_mode(client, false);
        if (rc != 0) {
            break;
        }
        rc = sh1107_set_contrast(client, 0x5f);
        if (rc != 0) {
            break;
        }
        rc = sh1107_set_dc_dc_control(client, 1);
        if (rc != 0) {
            break;
        }
        rc = sh1107_set_adc(client, false);
        if (rc != 0) {
            break;
        }
        rc = sh1107_set_output_scan_direction(client, false);
        if (rc != 0) {
            break;
        }
        rc = sh1107_set_address_start_line(client, 0x00);
        if (rc != 0) {
            break;
        }
        rc = sh1107_set_display_offset(client, 0x00);
        if (rc != 0) {
            break;
        }
        rc = sh1107_set_charge_duration(client, 0x02, 0x02);
        if (rc != 0) {
            break;
        }
        rc = sh1107_set_vcom(client, 0x35);
        if (rc != 0) {
            break;
        }
        rc = sh1107_set_multiplex_ration(client, 0x7F);
        if (rc != 0) {
            break;
        }
        rc = sh1107_set_entire_display_on_off(client, false);
        if (rc != 0) {
            break;
        }
        rc = sh1107_set_normal_reverse(client, false);
        if (rc != 0) {
            break;
        }
        rc = sh1107_set_display_on_off(client, true);
        if (rc != 0) {
            break;
        }
    } while(false);

    if (rc == 0) {
        rc = blank_screen(client, 0x00);
    }

    return rc;
}

static const struct of_device_id sh1107_of_match[] = {
    { .compatible = "sinowealth,sh110x" },
    {}
};
MODULE_DEVICE_TABLE(of, sh1107_of_match);

static void sh1107_print_char(struct i2c_client *client, unsigned char c) {
    struct sh1107_data *sh1107 = i2c_get_clientdata(client);
    uint8_t data_byte = 0;
    uint8_t col_i = 0;

    if ((sh1107->cursor_pos >= DISPLAY_PAGE_MAX) ||
        ( c == '\n' )) {

        sh1107->line_num = sh1107->line_num < DISPLAY_MAX_LINES-1 ? sh1107->line_num + 1: 0;
        sh1107_set_cursor(client, sh1107->line_num, 0);
    }
    if ( c != '\n' ) {
        c -= 0x20;
        do {
            data_byte = sh110x_12x8[c][col_i];
            if (sh1107->invert) {
                data_byte = ~data_byte;
            }
            sh1107_set_data(client, data_byte);

            col_i++;
        } while (col_i < SH110X_FONT_HEIGHT);

        sh1107_set_cursor(client, sh1107->line_num, ++sh1107->cursor_pos);
    }
}

static ssize_t sh1107_write_text(struct device * dev, const char * const buf, size_t count)
{
    struct sh1107_data *sh1107;
    int i = 0;

    sh1107 = dev_get_drvdata(dev);

    if (count == 1 && buf[0] == ' ') {
        blank_screen(sh1107->client, 0x00);
        return count;
    }

    for(i = 0; i < count; i++) {
        sh1107_print_char(sh1107->client, buf[i]);
    }

    return count;
}

static ssize_t store_text(struct device *dev, struct device_attribute *attr,
                        const char *buf, size_t count)
{
    return sh1107_write_text(dev, buf, count);
}

static int update_cursor_from_string(struct i2c_client *client, const char * const buf, size_t count)
{
    if (client == NULL || buf == NULL || count < 3 || count > 8) {
        return -EINVAL;
    }

    size_t i = 0;
    uint16_t line = 0, column = 0;
    unsigned long v = 0;

    for (i = 0; i < count; i++) {
        if (buf[i] == ' ') {
            break;
        }
    }

    if (i >= count) {
        return -EINVAL;
    }

    v = strtoul(buf, NULL, 10);
    line = min(v, (unsigned long) DISPLAY_PAGE_MAX);
    v = strtoul(&buf[i+1], NULL, 10);
    column = min(v, (unsigned long) DISPLAY_MAX_LINES);

    return sh1107_set_cursor(client, column, line);
}

static ssize_t store_cursor(struct device *dev, struct device_attribute *attr,
                        const char *buf, size_t count)
{
    struct sh1107_data *sh1107;
    int rc = 0;

    sh1107 = dev_get_drvdata(dev);
    if (count > 0) {
        char c = buf[0];
        switch(c) {
            case 'r':
                rc = sh1107_set_cursor(sh1107->client, 0,0);
                break;
            case 'i':
                rc = sh1107_invert_font(sh1107->client, true);
                break;
            case 'n':
                rc = sh1107_invert_font(sh1107->client, false);
                break;
            default:
                rc = update_cursor_from_string(sh1107->client, buf, count);
                break;
        }
    }

    if (rc != 0) {
        return rc;
    }

    return count;
}

static DEVICE_ATTR(text, S_IWUSR, NULL, store_text);
static DEVICE_ATTR(cursor, S_IWUSR, NULL, store_cursor);

static int sh1107_probe(struct i2c_client *client) {
    struct sh1107_data * sh1107 = NULL;
    int rc = 0;

    sh1107 = devm_kzalloc(&client->dev, sizeof(struct sh1107_data), GFP_KERNEL);
    if (!sh1107) {
        return -ENOMEM;
    }

    sh1107->dev = &client->dev;
    sh1107->line_num = 0;
    sh1107->cursor_pos = 0;
    sh1107->client = client;
    mutex_init(&sh1107->client_mutex);
    i2c_set_clientdata(client, sh1107);

    rc = init_display(client);
    if (rc != 0) {
        dev_info(&client->dev, "Failed to initialize display %s on bus %s (%d)", client->name, client->adapter->name, rc);
        return rc;
    }

    rc = device_create_file(&client->dev, &dev_attr_text);
    if (rc != 0)
    {
        dev_info(&client->dev, "Failed to create \"text\" sysfs file (%d)", rc);
        return rc;
    }
    sh1107->text = &dev_attr_text;

    rc = device_create_file(&client->dev, &dev_attr_cursor);
    if (rc != 0)
    {
        dev_info(&client->dev, "Failed to create \"cursor\" sysfs file (%d)", rc);
        return rc;
    }
    sh1107->cursor = &dev_attr_cursor;

    dev_info(&client->dev, "Done initializing screen %s on bus %s", client->name, client->adapter->name);
    
    return rc;
}

static void sh1107_remove(struct i2c_client *client)
{
    device_remove_file(&client->dev, &dev_attr_text);
    device_remove_file(&client->dev, &dev_attr_cursor);

    (void)sh1107_set_column_address(client, 0);
    (void)sh1107_set_page_address(client, 0);
    blank_screen(client, 0x00);

    sh1107_set_display_on_off(client, false);
}

static const struct i2c_device_id sh1107_id[] = {
    { "SH1107-OLED", 0 },
    {}
};
MODULE_DEVICE_TABLE(i2c, sh1107_id);

static struct i2c_driver sh1107_driver = {
    .driver = {
        .name = "i2c-sh110x",
        .of_match_table = sh1107_of_match,
    },
    .probe = sh1107_probe,
    .remove = sh1107_remove,
    .id_table = sh1107_id,
};
module_i2c_driver(sh1107_driver);

MODULE_AUTHOR("drevil drevil@blackram.works");
MODULE_DESCRIPTION("SH1107 I2C Driver");
MODULE_LICENSE("GPL");