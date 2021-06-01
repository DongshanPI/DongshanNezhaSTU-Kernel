/*
 * Allwinner SoCs display driver.
 *
 * Copyright (C) 2016 Allwinner.
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include "disp_lcd.h"
#include "disp_display.h"

#define LCD_SPI_MAX_TRANSFER_BYTE (100*PAGE_SIZE)

struct disp_lcd_private_data {
	struct disp_lcd_flow open_flow;
	struct disp_lcd_flow close_flow;
	struct disp_panel_para panel_info;
	struct panel_extend_para panel_extend_info;
	struct panel_extend_para panel_extend_info_set;
	u32    panel_extend_dirty;
	struct disp_lcd_cfg lcd_cfg;
	struct disp_lcd_panel_fun lcd_panel_fun;
	bool enabling;
	bool disabling;
	bool bl_enabled;
	u32 enabled;
	u32 power_enabled;
	u32 bl_need_enabled;
	s32 color_temperature;
	u32 color_inverse;
	struct {
		uintptr_t dev;
		u32 channel;
		u32 polarity;
		u32 period_ns;
		u32 duty_ns;
		u32 enabled;
	} pwm_info;
	struct mutex layer_mlock;
};
static spinlock_t lcd_data_lock;

static struct lcd_fb_device *lcds;
static struct disp_lcd_private_data *lcd_private;

struct lcd_fb_device *disp_get_lcd(u32 disp)
{
	if (disp > SUPPORT_MAX_LCD - 1)
		return NULL;
	return &lcds[disp];
}
static struct disp_lcd_private_data *disp_lcd_get_priv(struct lcd_fb_device *lcd)
{
	if (lcd == NULL) {
		lcd_fb_wrn("param is NULL!\n");
		return NULL;
	}

	return (struct disp_lcd_private_data *)lcd->priv_data;
}

static s32 disp_lcd_is_used(struct lcd_fb_device *lcd)
{
	struct disp_lcd_private_data *lcdp = disp_lcd_get_priv(lcd);
	s32 ret = 0;

	if ((lcd == NULL) || (lcdp == NULL))
		ret = 0;
	else
		ret = (s32)lcdp->lcd_cfg.lcd_used;

	return ret;
}

static s32 lcd_parse_panel_para(u32 disp, struct disp_panel_para *info)
{
	s32 ret = 0;
	char primary_key[25];
	s32 value = 0;
	int lines = 0, Bpp = 2;

	sprintf(primary_key, "lcd_fb%d", disp);
	memset(info, 0, sizeof(struct disp_panel_para));

	ret = lcd_fb_script_get_item(primary_key, "lcd_x", &value, 1);
	if (ret == 1)
		info->lcd_x = value;

	ret = lcd_fb_script_get_item(primary_key, "lcd_y", &value, 1);
	if (ret == 1)
		info->lcd_y = value;

	ret = lcd_fb_script_get_item(primary_key, "lcd_width", &value, 1);
	if (ret == 1)
		info->lcd_width = value;

	ret = lcd_fb_script_get_item(primary_key, "lcd_height", &value, 1);
	if (ret == 1)
		info->lcd_height = value;

	ret = lcd_fb_script_get_item(primary_key, "lcd_dclk_freq", &value, 1);
	if (ret == 1)
		info->lcd_dclk_freq = value;

	ret = lcd_fb_script_get_item(primary_key, "lcd_pwm_used", &value, 1);
	if (ret == 1)
		info->lcd_pwm_used = value;

	if (info->lcd_pwm_used) {
		ret = lcd_fb_script_get_item(primary_key, "lcd_pwm_ch", &value, 1);
		if (ret == 1)
			info->lcd_pwm_ch = value;

		ret = lcd_fb_script_get_item(primary_key, "lcd_pwm_freq", &value, 1);
		if (ret == 1)
			info->lcd_pwm_freq = value;

		ret = lcd_fb_script_get_item(primary_key, "lcd_pwm_pol", &value, 1);
		if (ret == 1)
			info->lcd_pwm_pol = value;
	}

	ret = lcd_fb_script_get_item(primary_key, "lcd_if", &value, 1);
	if (ret == 1)
		info->lcd_if = value;

	ret = lcd_fb_script_get_item(primary_key, "lcd_pixel_fmt", &value, 1);
	if (ret == 1)
		info->lcd_pixel_fmt = value;

	ret = lcd_fb_script_get_item(primary_key, "fb_buffer_num", &value, 1);
	if (ret == 1)
		info->fb_buffer_num = value;
	else
		info->fb_buffer_num = 2;

	ret = lcd_fb_script_get_item(primary_key, "lcd_hbp", &value, 1);
	if (ret == 1)
		info->lcd_hbp = value;

	ret = lcd_fb_script_get_item(primary_key, "lcd_ht", &value, 1);
	if (ret == 1)
		info->lcd_ht = value;

	ret = lcd_fb_script_get_item(primary_key, "lcd_vbp", &value, 1);
	if (ret == 1)
		info->lcd_vbp = value;

	ret = lcd_fb_script_get_item(primary_key, "lcd_vt", &value, 1);
	if (ret == 1)
		info->lcd_vt = value;


	ret = lcd_fb_script_get_item(primary_key, "lcd_vspw", &value, 1);
	if (ret == 1)
		info->lcd_vspw = value;

	ret = lcd_fb_script_get_item(primary_key, "lcd_hspw", &value, 1);
	if (ret == 1)
		info->lcd_hspw = value;

	ret = lcd_fb_script_get_item(primary_key, "lcd_fps", &value, 1);
	if (ret == 1)
		info->lcd_fps = value;
	else
		info->lcd_fps = 25;


	ret = lcd_fb_script_get_item(primary_key, "lcd_frm", &value, 1);
	if (ret == 1)
		info->lcd_frm = value;


	ret = lcd_fb_script_get_item(primary_key, "lcd_rb_swap", &value, 1);
	if (ret == 1)
		info->lcd_rb_swap = value;


	ret = lcd_fb_script_get_item(primary_key, "lcd_gamma_en", &value, 1);
	if (ret == 1)
		info->lcd_gamma_en = value;

	ret =
	    lcd_fb_script_get_item(primary_key, "lcd_model_name",
				     (int *)info->lcd_model_name, 2);

	Bpp = (info->lcd_pixel_fmt <= LCDFB_FORMAT_BGR_888) ? 3 : 2;
	info->lines_per_transfer = 1;
	lines = LCD_SPI_MAX_TRANSFER_BYTE / (info->lcd_x * Bpp);
	for (; lines > 1; --lines) {
		if (!(info->lcd_y % lines))
			break;
	}

	if (lines >= 1)
		info->lines_per_transfer = lines;

	return 0;
}

static void lcd_get_sys_config(u32 disp, struct disp_lcd_cfg *lcd_cfg)
{
	struct disp_gpio_set_t *gpio_info;
	int value = 1;
	char primary_key[20], sub_name[25];
	int i = 0;
	int ret;

	sprintf(primary_key, "lcd_fb%d", disp);
	/* lcd_used */
	ret = lcd_fb_script_get_item(primary_key, "lcd_used", &value, 1);
	if (ret == 1)
		lcd_cfg->lcd_used = value;

	if (lcd_cfg->lcd_used == 0)
		return;

	/* lcd_bl_en */
	lcd_cfg->lcd_bl_en_used = 0;
	gpio_info = &(lcd_cfg->lcd_bl_en);
	ret =
	    lcd_fb_script_get_item(primary_key, "lcd_bl_en", (int *)gpio_info,
				     3);
	if (ret == 3)
		lcd_cfg->lcd_bl_en_used = 1;

	sprintf(sub_name, "lcd_bl_en_power");
	ret =
	    lcd_fb_script_get_item(primary_key, sub_name,
				     (int *)lcd_cfg->lcd_bl_en_power, 2);

	/* lcd fix power */
	for (i = 0; i < LCD_POWER_NUM; i++) {
		if (i == 0)
			sprintf(sub_name, "lcd_fix_power");
		else
			sprintf(sub_name, "lcd_fix_power%d", i);
		lcd_cfg->lcd_power_used[i] = 0;
		ret =
		    lcd_fb_script_get_item(primary_key, sub_name,
					     (int *)(lcd_cfg->lcd_fix_power[i]),
					     2);
		if (ret == 2)
			/* str */
			lcd_cfg->lcd_fix_power_used[i] = 1;
	}

	/* lcd_power */
	for (i = 0; i < LCD_POWER_NUM; i++) {
		if (i == 0)
			sprintf(sub_name, "lcd_power");
		else
			sprintf(sub_name, "lcd_power%d", i);
		lcd_cfg->lcd_power_used[i] = 0;
		ret =
		    lcd_fb_script_get_item(primary_key, sub_name,
					     (int *)(lcd_cfg->lcd_power[i]), 2);
		if (ret == 2)
			/* str */
			lcd_cfg->lcd_power_used[i] = 1;
	}

	/* lcd_gpio */
	for (i = 0; i < 6; i++) {
		sprintf(sub_name, "lcd_gpio_%d", i);

		gpio_info = &(lcd_cfg->lcd_gpio[i]);
		ret =
		    lcd_fb_script_get_item(primary_key, sub_name,
					     (int *)gpio_info, 3);
		if (ret == 3)
			lcd_cfg->lcd_gpio_used[i] = 1;
	}


	for (i = 0; i < LCD_GPIO_REGU_NUM; i++) {
		sprintf(sub_name, "lcd_gpio_power%d", i);

		ret =
		    lcd_fb_script_get_item(primary_key, sub_name,
					     (int *)lcd_cfg->lcd_gpio_power[i],
					     2);
	}

	for (i = 0; i < LCD_GPIO_REGU_NUM; i++) {
		if (i == 0)
			sprintf(sub_name, "lcd_pin_power");
		else
			sprintf(sub_name, "lcd_pin_power%d", i);
		ret =
		    lcd_fb_script_get_item(primary_key, sub_name,
					     (int *)lcd_cfg->lcd_pin_power[i],
					     2);
	}

	/* backlight adjust */
	value = 0;
	lcd_fb_script_get_item(primary_key, "lcd_pwm_used", &value, 1);
	if (value == 1) {
		for (i = 0; i < 101; i++) {
			sprintf(sub_name, "lcd_bl_%d_percent", i);
			lcd_cfg->backlight_curve_adjust[i] = 0;

			if (i == 100)
				lcd_cfg->backlight_curve_adjust[i] = 255;

			ret =
				lcd_fb_script_get_item(primary_key, sub_name, &value, 1);
			if (ret == 1) {
				value = (value > 100) ? 100 : value;
				value = value * 255 / 100;
				lcd_cfg->backlight_curve_adjust[i] = value;
			}
		}
		sprintf(sub_name, "lcd_backlight");
		ret = lcd_fb_script_get_item(primary_key, sub_name, &value, 1);
		if (ret == 1) {
			value = (value > 256) ? 256 : value;
			lcd_cfg->backlight_bright = value;
		} else {
			lcd_cfg->backlight_bright = 197;
		}
	}


}

static s32 disp_lcd_pin_cfg(struct lcd_fb_device *lcd, u32 bon)
{
	int i;
	struct disp_lcd_private_data *lcdp = disp_lcd_get_priv(lcd);
	char dev_name[25];

	if ((lcd == NULL) || (lcdp == NULL)) {
		lcd_fb_wrn("NULL hdl!\n");
		return -1;
	}
	lcd_fb_inf("lcd %d pin config, state %s, %d\n", lcd->disp,
	       (bon) ? "on" : "off", bon);

	/* io-pad */
	if (bon == 1) {
		for (i = 0; i < LCD_GPIO_REGU_NUM; i++) {
			if (!
			    ((!strcmp(lcdp->lcd_cfg.lcd_pin_power[i], ""))
			     ||
			     (!strcmp(lcdp->lcd_cfg.lcd_pin_power[i], "none"))))
				lcd_fb_power_enable(lcdp->lcd_cfg.
						      lcd_pin_power[i]);
		}
	}

	sprintf(dev_name, "lcd_fb%d", lcd->disp);
	lcd_fb_pin_set_state(dev_name,
			       (bon == 1) ?
			       DISP_PIN_STATE_ACTIVE : DISP_PIN_STATE_SLEEP);

	if (bon == 0) {
		for (i = LCD_GPIO_REGU_NUM - 1; i >= 0; i--) {
			if (!
			    ((!strcmp(lcdp->lcd_cfg.lcd_pin_power[i], ""))
			     ||
			     (!strcmp(lcdp->lcd_cfg.lcd_pin_power[i], "none"))))
				lcd_fb_power_disable(lcdp->lcd_cfg.
						       lcd_pin_power[i]);
		}
	}

	return 0;
}

static s32 disp_lcd_pwm_enable(struct lcd_fb_device *lcd)
{
	struct disp_lcd_private_data *lcdp = disp_lcd_get_priv(lcd);

	if ((lcd == NULL) || (lcdp == NULL)) {
		lcd_fb_wrn("NULL hdl!\n");
		return -1;
	}

	if (disp_lcd_is_used(lcd) && lcdp->pwm_info.dev)
		return lcd_fb_pwm_enable(lcdp->pwm_info.dev);
	lcd_fb_wrn("pwm device hdl is NULL\n");

	return -1;
}

static s32 disp_lcd_pwm_disable(struct lcd_fb_device *lcd)
{
	struct disp_lcd_private_data *lcdp = disp_lcd_get_priv(lcd);
	s32 ret = -1;
	struct pwm_device *pwm_dev;

	if ((lcd == NULL) || (lcdp == NULL)) {
		lcd_fb_wrn("NULL hdl!\n");
		return -1;
	}

	if (disp_lcd_is_used(lcd) && lcdp->pwm_info.dev) {
		ret = lcd_fb_pwm_disable(lcdp->pwm_info.dev);
		pwm_dev = (struct pwm_device *)lcdp->pwm_info.dev;
		/*following is for reset pwm state purpose*/
		lcd_fb_pwm_config(lcdp->pwm_info.dev,
				    pwm_dev->state.duty_cycle - 1,
				    pwm_dev->state.period);
		lcd_fb_pwm_set_polarity(lcdp->pwm_info.dev,
					  !lcdp->pwm_info.polarity);
		return ret;
	}
	lcd_fb_wrn("pwm device hdl is NULL\n");

	return -1;
}

static s32 disp_lcd_backlight_enable(struct lcd_fb_device *lcd)
{
	struct disp_gpio_set_t gpio_info[1];
	struct disp_lcd_private_data *lcdp = disp_lcd_get_priv(lcd);
	unsigned long flags;

	if ((lcd == NULL) || (lcdp == NULL)) {
		lcd_fb_wrn("NULL hdl!\n");
		return -1;
	}

	spin_lock_irqsave(&lcd_data_lock, flags);
	if (lcdp->bl_enabled) {
		spin_unlock_irqrestore(&lcd_data_lock, flags);
		return -EBUSY;
	}

	lcdp->bl_need_enabled = 1;
	lcdp->bl_enabled = true;
	spin_unlock_irqrestore(&lcd_data_lock, flags);

	if (disp_lcd_is_used(lcd)) {
		unsigned bl;

		if (lcdp->lcd_cfg.lcd_bl_en_used) {
			/* io-pad */
			if (!
			    ((!strcmp(lcdp->lcd_cfg.lcd_bl_en_power, ""))
			     ||
			     (!strcmp(lcdp->lcd_cfg.lcd_bl_en_power, "none"))))
				lcd_fb_power_enable(lcdp->lcd_cfg.
						      lcd_bl_en_power);

			memcpy(gpio_info, &(lcdp->lcd_cfg.lcd_bl_en),
			       sizeof(struct disp_gpio_set_t));

			lcdp->lcd_cfg.lcd_bl_gpio_hdl =
			    lcd_fb_gpio_request(gpio_info, 1);
		}
		bl = disp_lcd_get_bright(lcd);
		disp_lcd_set_bright(lcd, bl);
	}

	return 0;
}

static s32 disp_lcd_backlight_disable(struct lcd_fb_device *lcd)
{
	struct disp_lcd_private_data *lcdp = disp_lcd_get_priv(lcd);
	unsigned long flags;

	if ((lcd == NULL) || (lcdp == NULL)) {
		lcd_fb_wrn("NULL hdl!\n");
		return -1;
	}

	spin_lock_irqsave(&lcd_data_lock, flags);
	if (!lcdp->bl_enabled) {
		spin_unlock_irqrestore(&lcd_data_lock, flags);
		return -EBUSY;
	}

	lcdp->bl_enabled = false;
	spin_unlock_irqrestore(&lcd_data_lock, flags);

	if (disp_lcd_is_used(lcd)) {
		if (lcdp->lcd_cfg.lcd_bl_en_used) {
			lcd_fb_gpio_release(lcdp->lcd_cfg.lcd_bl_gpio_hdl, 2);

			/* io-pad */
			if (!
			    ((!strcmp(lcdp->lcd_cfg.lcd_bl_en_power, ""))
			     ||
			     (!strcmp(lcdp->lcd_cfg.lcd_bl_en_power, "none"))))
				lcd_fb_power_disable(lcdp->lcd_cfg.
						       lcd_bl_en_power);
		}
	}

	return 0;
}

static s32 disp_lcd_power_enable(struct lcd_fb_device *lcd, u32 power_id)
{
	struct disp_lcd_private_data *lcdp = disp_lcd_get_priv(lcd);

	if ((lcd == NULL) || (lcdp == NULL)) {
		lcd_fb_wrn("NULL hdl!\n");
		return -1;
	}

	if (disp_lcd_is_used(lcd)) {
		if (lcdp->lcd_cfg.lcd_power_used[power_id] == 1) {
			/* regulator type */
			lcd_fb_power_enable(lcdp->lcd_cfg.
					      lcd_power[power_id]);
		}
	}

	return 0;
}

static s32 disp_lcd_power_disable(struct lcd_fb_device *lcd, u32 power_id)
{
	struct disp_lcd_private_data *lcdp = disp_lcd_get_priv(lcd);

	if ((lcd == NULL) || (lcdp == NULL)) {
		lcd_fb_wrn("NULL hdl!\n");
		return -1;
	}

	if (disp_lcd_is_used(lcd)) {
		if (lcdp->lcd_cfg.lcd_power_used[power_id] == 1) {
			/* regulator type */
			lcd_fb_power_disable(lcdp->lcd_cfg.
					       lcd_power[power_id]);
		}
	}

	return 0;
}

static s32 disp_lcd_bright_get_adjust_value(struct lcd_fb_device *lcd, u32 bright)
{
	struct disp_lcd_private_data *lcdp = disp_lcd_get_priv(lcd);

	if ((lcd == NULL) || (lcdp == NULL)) {
		lcd_fb_wrn("NULL hdl!\n");
		return -1;
	}
	bright = (bright > 255) ? 255 : bright;
	return lcdp->panel_extend_info.lcd_bright_curve_tbl[bright];
}

static s32 disp_lcd_bright_curve_init(struct lcd_fb_device *lcd)
{
	struct disp_lcd_private_data *lcdp = disp_lcd_get_priv(lcd);
	u32 i = 0, j = 0;
	u32 items = 0;
	u32 lcd_bright_curve_tbl[101][2];

	if ((lcd == NULL) || (lcdp == NULL)) {
		lcd_fb_wrn("NULL hdl!\n");
		return -1;
	}

	for (i = 0; i < 101; i++) {
		if (lcdp->lcd_cfg.backlight_curve_adjust[i] == 0) {
			if (i == 0) {
				lcd_bright_curve_tbl[items][0] = 0;
				lcd_bright_curve_tbl[items][1] = 0;
				items++;
			}
		} else {
			lcd_bright_curve_tbl[items][0] = 255 * i / 100;
			lcd_bright_curve_tbl[items][1] =
			    lcdp->lcd_cfg.backlight_curve_adjust[i];
			items++;
		}
	}

	for (i = 0; i < items - 1; i++) {
		u32 num =
		    lcd_bright_curve_tbl[i + 1][0] - lcd_bright_curve_tbl[i][0];

		for (j = 0; j < num; j++) {
			u32 value = 0;

			value =
			    lcd_bright_curve_tbl[i][1] +
			    ((lcd_bright_curve_tbl[i + 1][1] -
			      lcd_bright_curve_tbl[i][1]) * j) / num;
			lcdp->panel_extend_info.
			    lcd_bright_curve_tbl[lcd_bright_curve_tbl[i][0] +
						 j] = value;
		}
	}
	lcdp->panel_extend_info.lcd_bright_curve_tbl[255] =
	    lcd_bright_curve_tbl[items - 1][1];

	return 0;
}

s32 disp_lcd_set_bright(struct lcd_fb_device *lcd, u32 bright)
{
	u32 duty_ns;
	__u64 backlight_bright = bright;
	__u64 backlight_dimming;
	__u64 period_ns;
	struct disp_lcd_private_data *lcdp = disp_lcd_get_priv(lcd);
	unsigned long flags;
	bool bright_update = false;

	if ((lcd == NULL) || (lcdp == NULL)) {
		lcd_fb_wrn("NULL hdl!\n");
		return -1;
	}

	spin_lock_irqsave(&lcd_data_lock, flags);
	backlight_bright = (backlight_bright > 255) ? 255 : backlight_bright;
	if (lcdp->lcd_cfg.backlight_bright != backlight_bright) {
		bright_update = true;
		lcdp->lcd_cfg.backlight_bright = backlight_bright;
	}
	spin_unlock_irqrestore(&lcd_data_lock, flags);

	if (lcdp->pwm_info.dev) {
		if (backlight_bright != 0)
			backlight_bright += 1;
		backlight_bright =
		    disp_lcd_bright_get_adjust_value(lcd, backlight_bright);

		lcdp->lcd_cfg.backlight_dimming =
		    (lcdp->lcd_cfg.backlight_dimming ==
		     0) ? 256 : lcdp->lcd_cfg.backlight_dimming;
		backlight_dimming = lcdp->lcd_cfg.backlight_dimming;
		period_ns = lcdp->pwm_info.period_ns;
		duty_ns =
		    (backlight_bright * backlight_dimming * period_ns / 256 +
		     128) / 256;
		lcdp->pwm_info.duty_ns = duty_ns;
		lcd_fb_pwm_config(lcdp->pwm_info.dev, duty_ns, period_ns);
	}

	if (lcdp->lcd_panel_fun.set_bright && lcdp->enabled) {
		lcdp->lcd_panel_fun.set_bright(lcd->disp,
					       disp_lcd_bright_get_adjust_value
					       (lcd, bright));
	}

	return 0;
}

s32 disp_lcd_get_bright(struct lcd_fb_device *lcd)
{
	struct disp_lcd_private_data *lcdp = disp_lcd_get_priv(lcd);

	if ((lcd == NULL) || (lcdp == NULL)) {
		lcd_fb_wrn("NULL hdl!\n");
		return -1;
	}

	return lcdp->lcd_cfg.backlight_bright;
}

static s32 disp_lcd_set_bright_dimming(struct lcd_fb_device *lcd, u32 dimming)
{
	struct disp_lcd_private_data *lcdp = disp_lcd_get_priv(lcd);
	u32 bl = 0;

	if ((lcd == NULL) || (lcdp == NULL)) {
		lcd_fb_wrn("NULL hdl!\n");
		return -1;
	}

	dimming = dimming > 256 ? 256 : dimming;
	lcdp->lcd_cfg.backlight_dimming = dimming;
	bl = disp_lcd_get_bright(lcd);
	disp_lcd_set_bright(lcd, bl);

	return 0;
}

static s32 disp_lcd_get_panel_info(struct lcd_fb_device *lcd,
				   struct disp_panel_para *info)
{
	struct disp_lcd_private_data *lcdp = disp_lcd_get_priv(lcd);

	if ((lcd == NULL) || (lcdp == NULL)) {
		lcd_fb_wrn("NULL hdl!\n");
		return -1;
	}

	memcpy(info, (struct disp_panel_para *) (&(lcdp->panel_info)),
	       sizeof(struct disp_panel_para));
	return 0;
}



/* lcd enable except for backlight */
static s32 disp_lcd_fake_enable(struct lcd_fb_device *lcd)
{
	lcd_fb_wrn("To be implement!\n");
	return 0;
}

static s32 disp_lcd_enable(struct lcd_fb_device *lcd)
{
	unsigned long flags;
	struct disp_lcd_private_data *lcdp = disp_lcd_get_priv(lcd);
	int i;
	unsigned bl;

	if ((lcd == NULL) || (lcdp == NULL)) {
		lcd_fb_wrn("NULL hdl!\n");
		return -1;
	}
	lcd_fb_inf("lcd %d\n", lcd->disp);

	if (disp_lcd_is_enabled(lcd) == 1)
		return 0;

	/* init fix power */
	for (i = 0; i < LCD_POWER_NUM; i++) {
		if (lcdp->lcd_cfg.lcd_fix_power_used[i] == 1)
			lcd_fb_power_enable(lcdp->lcd_cfg.lcd_fix_power[i]);
	}

	spin_lock_irqsave(&lcd_data_lock, flags);
	lcdp->enabling = 1;
	lcdp->bl_need_enabled = 0;
	spin_unlock_irqrestore(&lcd_data_lock, flags);

	lcdp->panel_extend_info.lcd_gamma_en = lcdp->panel_info.lcd_gamma_en;
	disp_lcd_gpio_init(lcd);

	lcd_fb_pwm_config(lcdp->pwm_info.dev, lcdp->pwm_info.duty_ns,
			    lcdp->pwm_info.period_ns);
	lcd_fb_pwm_set_polarity(lcdp->pwm_info.dev, lcdp->pwm_info.polarity);

	lcdp->open_flow.func_num = 0;

	if (lcdp->lcd_panel_fun.cfg_open_flow)
		lcdp->lcd_panel_fun.cfg_open_flow(lcd->disp);
	else
		lcd_fb_wrn("lcd_panel_fun[%d].cfg_open_flow is NULL\n", lcd->disp);

	for (i = 0; i < lcdp->open_flow.func_num; i++) {
		if (lcdp->open_flow.func[i].func) {
			lcdp->open_flow.func[i].func(lcd->disp);
			lcd_fb_inf("open flow:step %d finish, to delay %d\n", i,
			       lcdp->open_flow.func[i].delay);
			if (lcdp->open_flow.func[i].delay != 0)
				disp_delay_ms(lcdp->open_flow.func[i].delay);
		}
	}

	spin_lock_irqsave(&lcd_data_lock, flags);
	lcdp->enabled = 1;
	lcdp->enabling = 0;
	spin_unlock_irqrestore(&lcd_data_lock, flags);
	bl = disp_lcd_get_bright(lcd);
	disp_lcd_set_bright(lcd, bl);

	return 0;
}

static s32 disp_lcd_disable(struct lcd_fb_device *lcd)
{
	unsigned long flags;
	struct disp_lcd_private_data *lcdp = disp_lcd_get_priv(lcd);
	int i;

	if ((lcd == NULL) || (lcdp == NULL)) {
		lcd_fb_wrn("NULL hdl!\n");
		return -1;
	}

	lcd_fb_inf("lcd %d\n", lcd->disp);
	if (disp_lcd_is_enabled(lcd) == 0)
		return 0;

	spin_lock_irqsave(&lcd_data_lock, flags);
	lcdp->enabled = 0;
	spin_unlock_irqrestore(&lcd_data_lock, flags);

	lcdp->bl_need_enabled = 0;
	lcdp->close_flow.func_num = 0;
	if (lcdp->lcd_panel_fun.cfg_close_flow)
		lcdp->lcd_panel_fun.cfg_close_flow(lcd->disp);
	else
		lcd_fb_wrn("lcd_panel_fun[%d].cfg_close_flow is NULL\n", lcd->disp);

	for (i = 0; i < lcdp->close_flow.func_num; i++) {
		if (lcdp->close_flow.func[i].func) {
			lcdp->close_flow.func[i].func(lcd->disp);
			lcd_fb_inf("close flow:step %d finish, to delay %d\n", i,
			       lcdp->close_flow.func[i].delay);
			if (lcdp->close_flow.func[i].delay != 0)
				disp_delay_ms(lcdp->close_flow.func[i].delay);
		}
	}

	disp_lcd_gpio_exit(lcd);

	for (i = LCD_POWER_NUM - 1; i >= 0; i--) {
		if (lcdp->lcd_cfg.lcd_fix_power_used[i] == 1)
			lcd_fb_power_disable(lcdp->lcd_cfg.lcd_fix_power[i]);
	}

	return 0;
}

s32 disp_lcd_is_enabled(struct lcd_fb_device *lcd)
{
	struct disp_lcd_private_data *lcdp = disp_lcd_get_priv(lcd);

	if ((lcd == NULL) || (lcdp == NULL)) {
		lcd_fb_wrn("NULL hdl!\n");
		return -1;
	}

	return (s32)lcdp->enabled;
}

/**
 * disp_lcd_check_if_enabled - check lcd if be enabled status
 *
 * this function only be used by bsp_disp_sync_with_hw to check
 * the device enabled status when driver init
 */
s32 disp_lcd_check_if_enabled(struct lcd_fb_device *lcd)
{
	struct disp_lcd_private_data *lcdp = disp_lcd_get_priv(lcd);
	int ret = 1;

	if ((lcd == NULL) || (lcdp == NULL)) {
		lcd_fb_wrn("NULL hdl!\n");
		return -1;
	}

	return ret;
}

static s32 disp_lcd_set_open_func(struct lcd_fb_device *lcd, LCD_FUNC func,
				  u32 delay)
{
	struct disp_lcd_private_data *lcdp = disp_lcd_get_priv(lcd);

	if ((lcd == NULL) || (lcdp == NULL)) {
		lcd_fb_wrn("NULL hdl!\n");
		return -1;
	}

	if (func) {
		lcdp->open_flow.func[lcdp->open_flow.func_num].func = func;
		lcdp->open_flow.func[lcdp->open_flow.func_num].delay = delay;
		lcdp->open_flow.func_num++;
	}

	return 0;
}

static s32 disp_lcd_set_close_func(struct lcd_fb_device *lcd, LCD_FUNC func,
				   u32 delay)
{
	struct disp_lcd_private_data *lcdp = disp_lcd_get_priv(lcd);

	if ((lcd == NULL) || (lcdp == NULL)) {
		lcd_fb_wrn("NULL hdl!\n");
		return -1;
	}

	if (func) {
		lcdp->close_flow.func[lcdp->close_flow.func_num].func = func;
		lcdp->close_flow.func[lcdp->close_flow.func_num].delay = delay;
		lcdp->close_flow.func_num++;
	}

	return 0;
}

static s32 disp_lcd_set_panel_funs(struct lcd_fb_device *lcd, char *name,
				   struct disp_lcd_panel_fun *lcd_cfg)
{
	char primary_key[20], drv_name[32];
	s32 ret;
	struct disp_lcd_private_data *lcdp = disp_lcd_get_priv(lcd);

	if ((lcd == NULL) || (lcdp == NULL)) {
		lcd_fb_wrn("NULL hdl!\n");
		return -1;
	}

	sprintf(primary_key, "lcd_fb%d", lcd->disp);

	ret =
	    lcd_fb_script_get_item(primary_key, "lcd_driver_name",
				     (int *)drv_name, 2);
	lcd_fb_inf("lcd %d, driver_name %s,  panel_name %s\n", lcd->disp, drv_name,
	       name);
	if ((ret == 2) && !strcmp(drv_name, name)) {
		memset(&lcdp->lcd_panel_fun,
		       0,
		       sizeof(struct disp_lcd_panel_fun));
		lcdp->lcd_panel_fun.cfg_panel_info = lcd_cfg->cfg_panel_info;
		lcdp->lcd_panel_fun.cfg_open_flow = lcd_cfg->cfg_open_flow;
		lcdp->lcd_panel_fun.cfg_close_flow = lcd_cfg->cfg_close_flow;
		lcdp->lcd_panel_fun.lcd_user_defined_func =
		    lcd_cfg->lcd_user_defined_func;
		lcdp->lcd_panel_fun.set_bright = lcd_cfg->set_bright;
		lcdp->lcd_panel_fun.set_layer = lcd_cfg->set_layer;
		lcdp->lcd_panel_fun.blank = lcd_cfg->blank;
		lcdp->lcd_panel_fun.set_var = lcd_cfg->set_var;
		lcdp->lcd_panel_fun.set_addr_win = lcd_cfg->set_addr_win;
		if (lcdp->lcd_panel_fun.cfg_panel_info) {
			lcdp->lcd_panel_fun.cfg_panel_info(&lcdp->panel_extend_info);
			memcpy(&lcdp->panel_extend_info_set,
				&lcdp->panel_extend_info, sizeof(struct panel_extend_para));
		} else {
			lcd_fb_wrn("lcd_panel_fun[%d].cfg_panel_info is NULL\n", lcd->disp);
		}

		return 0;
	}

	return -1;
}

s32 disp_lcd_gpio_init(struct lcd_fb_device *lcd)
{
	struct disp_lcd_private_data *lcdp = disp_lcd_get_priv(lcd);
	int i = 0;

	if ((lcd == NULL) || (lcdp == NULL)) {
		lcd_fb_wrn("NULL hdl!\n");
		return -1;
	}

	/* io-pad */
	for (i = 0; i < LCD_GPIO_REGU_NUM; i++) {
		if (!
		    ((!strcmp(lcdp->lcd_cfg.lcd_gpio_power[i], ""))
		     || (!strcmp(lcdp->lcd_cfg.lcd_gpio_power[i], "none"))))
			lcd_fb_power_enable(lcdp->lcd_cfg.lcd_gpio_power[i]);
	}

	for (i = 0; i < LCD_GPIO_NUM; i++) {
		lcdp->lcd_cfg.gpio_hdl[i] = 0;

		if (lcdp->lcd_cfg.lcd_gpio_used[i]) {
			struct disp_gpio_set_t gpio_info[1];

			memcpy(gpio_info, &(lcdp->lcd_cfg.lcd_gpio[i]),
			       sizeof(struct disp_gpio_set_t));
			lcdp->lcd_cfg.gpio_hdl[i] =
			    lcd_fb_gpio_request(gpio_info, 1);
		}
	}

	return 0;
}

s32 disp_lcd_gpio_exit(struct lcd_fb_device *lcd)
{
	struct disp_lcd_private_data *lcdp = disp_lcd_get_priv(lcd);
	int i = 0;

	if ((lcd == NULL) || (lcdp == NULL)) {
		lcd_fb_wrn("NULL hdl!\n");
		return -1;
	}

	for (i = 0; i < LCD_GPIO_NUM; i++) {
		if (lcdp->lcd_cfg.gpio_hdl[i]) {
			struct disp_gpio_set_t gpio_info[1];

			lcd_fb_gpio_release(lcdp->lcd_cfg.gpio_hdl[i], 2);

			memcpy(gpio_info, &(lcdp->lcd_cfg.lcd_gpio[i]),
			       sizeof(struct disp_gpio_set_t));
			gpio_info->mul_sel = 7;
			lcdp->lcd_cfg.gpio_hdl[i] =
			    lcd_fb_gpio_request(gpio_info, 1);
			lcd_fb_gpio_release(lcdp->lcd_cfg.gpio_hdl[i], 2);
			lcdp->lcd_cfg.gpio_hdl[i] = 0;
		}
	}

	/* io-pad */
	for (i = LCD_GPIO_REGU_NUM - 1; i >= 0; i--) {
		if (!
		    ((!strcmp(lcdp->lcd_cfg.lcd_gpio_power[i], ""))
		     || (!strcmp(lcdp->lcd_cfg.lcd_gpio_power[i], "none"))))
			lcd_fb_power_disable(lcdp->lcd_cfg.lcd_gpio_power[i]);
	}

	return 0;
}

/* direction: input(0), output(1) */
s32 disp_lcd_gpio_set_direction(struct lcd_fb_device *lcd, u32 io_index,
				u32 direction)
{
	struct disp_lcd_private_data *lcdp = disp_lcd_get_priv(lcd);
	char gpio_name[20];

	if ((lcd == NULL) || (lcdp == NULL)) {
		lcd_fb_wrn("NULL hdl!\n");
		return -1;
	}

	sprintf(gpio_name, "lcd_gpio_%d", io_index);
	return lcd_fb_gpio_set_direction(lcdp->lcd_cfg.gpio_hdl[io_index],
					   direction, gpio_name);
}

s32 disp_lcd_gpio_get_value(struct lcd_fb_device *lcd, u32 io_index)
{
	struct disp_lcd_private_data *lcdp = disp_lcd_get_priv(lcd);
	char gpio_name[20];

	if ((lcd == NULL) || (lcdp == NULL)) {
		lcd_fb_wrn("NULL hdl!\n");
		return -1;
	}

	sprintf(gpio_name, "lcd_gpio_%d", io_index);
	return lcd_fb_gpio_get_value(lcdp->lcd_cfg.gpio_hdl[io_index],
				       gpio_name);
}

s32 disp_lcd_gpio_set_value(struct lcd_fb_device *lcd, u32 io_index, u32 data)
{
	struct disp_lcd_private_data *lcdp = disp_lcd_get_priv(lcd);
	char gpio_name[20];

	if ((lcd == NULL) || (lcdp == NULL)) {
		lcd_fb_wrn("NULL hdl!\n");
		return -1;
	}

	if (io_index >= LCD_GPIO_NUM) {
		lcd_fb_wrn("gpio num out of range\n");
		return -1;
	}
	sprintf(gpio_name, "lcd_gpio_%d", io_index);
	return lcd_fb_gpio_set_value(lcdp->lcd_cfg.gpio_hdl[io_index], data,
				       gpio_name);
}

static s32 disp_lcd_get_dimensions(struct lcd_fb_device *lcd, u32 *width,
				   u32 *height)
{
	struct disp_lcd_private_data *lcdp = disp_lcd_get_priv(lcd);

	if ((lcd == NULL) || (lcdp == NULL)) {
		lcd_fb_wrn("NULL hdl!\n");
		return -1;
	}

	if (width)
		*width = lcdp->panel_info.lcd_width;

	if (height)
		*height = lcdp->panel_info.lcd_height;

	return 0;
}


static s32 disp_lcd_update_gamma_tbl_set(struct lcd_fb_device *lcd)
{
	struct disp_lcd_private_data *lcdp = disp_lcd_get_priv(lcd);
	int i;
	unsigned int *gamma, *gamma_set;
	unsigned int r, g, b;
	s32 color_temperature;
	u32 color_inverse;

	color_temperature = lcdp->color_temperature;
	color_inverse = lcdp->color_inverse;
	memcpy(&lcdp->panel_extend_info_set, &lcdp->panel_extend_info,
		sizeof(struct panel_extend_para));
	gamma = lcdp->panel_extend_info.lcd_gamma_tbl;
	gamma_set = lcdp->panel_extend_info_set.lcd_gamma_tbl;
	if (color_temperature > 0) {
		/* warm color */
		for (i = 0; i < 256; i++) {
			r = (gamma[i] >> 16) & 0xff;
			g = (gamma[i] >> 8) & 0xff;
			b = gamma[i] & 0xff;

			g = g * (512 - color_temperature) / 512;
			b = b * (256 - color_temperature) / 256;
			r = r << 16;

			g = g << 8;
			gamma_set[i] = r | g | b;
		}
	} else if (color_temperature < 0) {
		/* cool color */
		for (i = 0; i < 256; i++) {
			r = (gamma[i] >> 16) & 0xff;
			g = (gamma[i] >> 8) & 0xff;
			b = gamma[i] & 0xff;

			r = r * (256 + color_temperature) / 256;
			g = g * (512 + color_temperature) / 512;

			r = r << 16;
			g = g << 8;

			gamma_set[i] = r | g | b;
		}
	}
	if (color_inverse == 1) {
		for (i = 0; i < 256; i++)
			gamma_set[i] = 0xffffffff -  gamma_set[i];
	}
	if (color_inverse != 0)
		lcdp->panel_extend_info_set.lcd_gamma_en = 1;
	if (color_temperature != 0)
		lcdp->panel_extend_info_set.lcd_gamma_en = 1;

	return 0;
}


static s32 disp_lcd_set_gamma_tbl(struct lcd_fb_device *lcd,
			unsigned int *gamma_table, unsigned int size)
{
	struct disp_lcd_private_data *lcdp = disp_lcd_get_priv(lcd);
	unsigned long flags;

	if ((lcd == NULL) || (lcdp == NULL)
	    || (gamma_table == NULL)) {
		lcd_fb_wrn("NULL hdl!\n");
		return 0;
	}

	size = (size > LCD_GAMMA_TABLE_SIZE) ?
	    LCD_GAMMA_TABLE_SIZE : size;
	spin_lock_irqsave(&lcd_data_lock, flags);
	memcpy(lcdp->panel_extend_info.lcd_gamma_tbl, gamma_table, size);
	disp_lcd_update_gamma_tbl_set(lcd);
	lcdp->panel_extend_dirty = 1;
	spin_unlock_irqrestore(&lcd_data_lock, flags);

	return 0;
}

static s32 disp_lcd_enable_gamma(struct lcd_fb_device *lcd)
{
	struct disp_lcd_private_data *lcdp = disp_lcd_get_priv(lcd);
	unsigned long flags;

	if ((lcd == NULL) || (lcdp == NULL)) {
		lcd_fb_wrn("NULL hdl!\n");
		return 0;
	}

	spin_lock_irqsave(&lcd_data_lock, flags);
	if (lcdp->panel_extend_info.lcd_gamma_en == 0) {
		lcdp->panel_extend_info.lcd_gamma_en = 1;
		disp_lcd_update_gamma_tbl_set(lcd);
		lcdp->panel_extend_dirty = 1;
	}
	spin_unlock_irqrestore(&lcd_data_lock, flags);

	return 0;
}

static s32 disp_lcd_disable_gamma(struct lcd_fb_device *lcd)
{
	struct disp_lcd_private_data *lcdp = disp_lcd_get_priv(lcd);
	int ret = -1;

	if ((lcd == NULL) || (lcdp == NULL)) {
		lcd_fb_wrn("NULL hdl!\n");
		return 0;
	}

	/*if (lcdp->panel_extend_info.lcd_gamma_en == 1) {*/
		/*lcdp->panel_extend_info.lcd_gamma_en = 0;*/
	/*} else {*/
		/*ret = 0;*/
	/*}*/

	return ret;
}

static s32 disp_lcd_set_color_temperature(struct lcd_fb_device *lcd,
	s32 color_temperature)
{
	struct disp_lcd_private_data *lcdp = disp_lcd_get_priv(lcd);
	unsigned long flags;

	if ((NULL == lcd) || (NULL == lcdp)) {
		lcd_fb_wrn("NULL hdl!\n");
		return -1;
	}

	spin_lock_irqsave(&lcd_data_lock, flags);
	lcdp->color_temperature = color_temperature;
	disp_lcd_update_gamma_tbl_set(lcd);
	lcdp->panel_extend_dirty = 1;
	spin_unlock_irqrestore(&lcd_data_lock, flags);

	return 0;
}

static s32 disp_lcd_get_color_temperature(struct lcd_fb_device *lcd)
{
	struct disp_lcd_private_data *lcdp = disp_lcd_get_priv(lcd);
	unsigned long flags;
	s32 color_temperature = 0;

	if ((NULL == lcd) || (NULL == lcdp)) {
		lcd_fb_wrn("NULL hdl!\n");
		return 0;
	}

	spin_lock_irqsave(&lcd_data_lock, flags);
	color_temperature = lcdp->color_temperature;
	spin_unlock_irqrestore(&lcd_data_lock, flags);

	return color_temperature;
}

static s32 disp_lcd_init(struct lcd_fb_device *lcd)
{
	struct disp_lcd_private_data *lcdp = disp_lcd_get_priv(lcd);
	int i;

	if ((lcd == NULL) || (lcdp == NULL)) {
		lcd_fb_wrn("NULL hdl!\n");
		return -1;
	}
	lcd_fb_inf("lcd %d\n", lcd->disp);
	mutex_init(&lcdp->layer_mlock);

	lcd_get_sys_config(lcd->disp, &lcdp->lcd_cfg);
	if (disp_lcd_is_used(lcd)) {
		struct disp_video_timings *timmings;
		struct disp_panel_para *panel_info;

		lcd_parse_panel_para(lcd->disp, &lcdp->panel_info);
		timmings = &lcd->timings;
		panel_info = &lcdp->panel_info;
		timmings->pixel_clk = panel_info->lcd_dclk_freq * 1000;
		timmings->x_res = panel_info->lcd_x;
		timmings->y_res = panel_info->lcd_y;
		timmings->hor_total_time = panel_info->lcd_ht;
		timmings->hor_sync_time = panel_info->lcd_hspw;
		timmings->hor_back_porch =
		    panel_info->lcd_hbp - panel_info->lcd_hspw;
		timmings->hor_front_porch =
		    panel_info->lcd_ht - panel_info->lcd_hbp -
		    panel_info->lcd_x;
		timmings->ver_total_time = panel_info->lcd_vt;
		timmings->ver_sync_time = panel_info->lcd_vspw;
		timmings->ver_back_porch =
		    panel_info->lcd_vbp - panel_info->lcd_vspw;
		timmings->ver_front_porch =
		    panel_info->lcd_vt - panel_info->lcd_vbp -
		    panel_info->lcd_y;
	}
	disp_lcd_bright_curve_init(lcd);

	if (disp_lcd_is_used(lcd)) {
		__u64 backlight_bright;
		__u64 period_ns, duty_ns;

		if (lcdp->panel_info.lcd_pwm_used) {
			lcdp->pwm_info.channel = lcdp->panel_info.lcd_pwm_ch;
			lcdp->pwm_info.polarity = lcdp->panel_info.lcd_pwm_pol;
			lcdp->pwm_info.dev =
			    lcd_fb_pwm_request(lcdp->panel_info.lcd_pwm_ch);

			if (lcdp->panel_info.lcd_pwm_freq != 0) {
				period_ns =
				    1000 * 1000 * 1000 /
				    lcdp->panel_info.lcd_pwm_freq;
			} else {
				lcd_fb_wrn("lcd%d.lcd_pwm_freq is ZERO\n",
				       lcd->disp);
				/* default 1khz */
				period_ns = 1000 * 1000 * 1000 / 1000;
			}

			backlight_bright = lcdp->lcd_cfg.backlight_bright;

			duty_ns = (backlight_bright * period_ns) / 256;
			lcdp->pwm_info.duty_ns = duty_ns;
			lcdp->pwm_info.period_ns = period_ns;
		}
		for (i = 0; i < 256; i++) {
			lcdp->panel_extend_info.lcd_gamma_tbl[i] =
				(i << 24) | (i << 16) | (i << 8) | (i);
		}
	}

	return 0;
}



static s32 disp_lcd_exit(struct lcd_fb_device *lcd)
{
	struct disp_lcd_private_data *lcdp = disp_lcd_get_priv(lcd);

	if ((lcd == NULL) || (lcdp == NULL)) {
		lcd_fb_wrn("NULL hdl!\n");
		return -1;
	}


	return 0;
}

s32 disp_lcd_get_resolution(struct lcd_fb_device *dispdev, u32 *xres,
			       u32 *yres)
{
	if (dispdev == NULL) {
		lcd_fb_wrn("NULL hdl!\n");
		return -1;
	}

	if (xres)
		*xres = dispdev->timings.x_res;

	if (yres)
		*yres = dispdev->timings.y_res;

	return 0;
}

static void rgb32_to_rgb24(unsigned char *dst, unsigned char *src,
			   unsigned int len)
{
	int i, j;

	for (i = 0, j = 0; i < len; i += 4, j += 3)
		memcpy(&dst[j], &src[i], 3);
}

int disp_lcd_set_layer(struct lcd_fb_device *lcd, struct fb_info *p_info)
{
	int ret = -1;
	unsigned int i = 0, len = 0;
	unsigned char *addr = NULL, *end_addr = NULL, *line_buf = NULL;
	struct disp_lcd_private_data *lcdp = disp_lcd_get_priv(lcd);

	mutex_lock(&lcdp->layer_mlock);

	len = (p_info->var.bits_per_pixel == 32) ? p_info->var.xres * 3
						 : p_info->fix.line_length;
	len *= lcdp->panel_info.lines_per_transfer;

	if (lcdp->lcd_panel_fun.set_layer && p_info) {
		addr = (unsigned char *)p_info->screen_base +
			p_info->var.yoffset * p_info->fix.line_length;
		end_addr = (unsigned char *)addr + p_info->fix.smem_len;
		if (p_info->var.bits_per_pixel == 32) {
			line_buf = kmalloc(p_info->var.xres * 3, GFP_KERNEL | __GFP_ZERO);
			if (!line_buf)
				goto OUT;
			for (i = 0; i < p_info->var.yres &&
			     addr <= end_addr - p_info->fix.line_length;
			     ++i) {
				rgb32_to_rgb24(line_buf, addr, p_info->fix.line_length);
				ret = lcdp->lcd_panel_fun.set_layer(
				    lcd->disp, (void *)line_buf, len);
				addr = (unsigned char *)addr +
				       p_info->fix.line_length *
					   lcdp->panel_info.lines_per_transfer;
			}
		} else {
			for (i = 0; i < p_info->var.yres &&
				    addr <= end_addr - p_info->fix.line_length;
			     i += lcdp->panel_info.lines_per_transfer) {
				ret = lcdp->lcd_panel_fun.set_layer(
				    lcd->disp, (void *)addr, len);
				addr = (unsigned char *)addr +
				       p_info->fix.line_length *
					   lcdp->panel_info.lines_per_transfer;
			}
		}
	}

OUT:
	if (line_buf)
		kfree(line_buf);
	mutex_unlock(&lcdp->layer_mlock);
	return ret;
}

int disp_lcd_set_var(struct lcd_fb_device *lcd, struct fb_info *p_info)
{
	int ret = -1;
	struct disp_lcd_private_data *lcdp = disp_lcd_get_priv(lcd);

	mutex_lock(&lcdp->layer_mlock);
	if (lcdp->lcd_panel_fun.set_var && p_info)
		ret = lcdp->lcd_panel_fun.set_var(lcd->disp, p_info);
	mutex_unlock(&lcdp->layer_mlock);
	return ret;
}

int disp_lcd_blank(struct lcd_fb_device *lcd, unsigned int en)
{
	int ret = -1;
	struct disp_lcd_private_data *lcdp = disp_lcd_get_priv(lcd);

	if (lcdp->lcd_panel_fun.blank)
		ret = lcdp->lcd_panel_fun.blank(lcd->disp, en);
	return ret;
}

s32 disp_init_lcd(struct dev_lcd_fb_t *p_info)
{
	u32 disp = 0;
	struct lcd_fb_device *lcd;
	struct disp_lcd_private_data *lcdp;
	u32 hwdev_index = 0;
	u32 num_devices_support_lcd = SUPPORT_MAX_LCD;
	char primary_key[20];
	int ret = 0, value = 1;

	lcd_fb_inf("disp_init_lcd\n");

	spin_lock_init(&lcd_data_lock);

	lcds =
	    kmalloc_array(num_devices_support_lcd, sizeof(struct lcd_fb_device),
			  GFP_KERNEL | __GFP_ZERO);
	if (lcds == NULL) {
		lcd_fb_wrn("malloc memory(%d bytes) fail!\n",
		       (unsigned int)sizeof(struct lcd_fb_device) *
		       num_devices_support_lcd);
		goto malloc_err;
	}
	lcd_private =
	    (struct disp_lcd_private_data *)
	    kmalloc(sizeof(struct disp_lcd_private_data)
		    * num_devices_support_lcd, GFP_KERNEL | __GFP_ZERO);
	if (lcd_private == NULL) {
		lcd_fb_wrn("malloc memory(%d bytes) fail!\n",
		       (unsigned int)sizeof(struct disp_lcd_private_data) *
		       num_devices_support_lcd);
		goto malloc_err;
	}

	disp = 0;
	for (hwdev_index = 0; hwdev_index < num_devices_support_lcd; hwdev_index++) {

		sprintf(primary_key, "lcd_fb%d", disp);
		ret = lcd_fb_script_get_item(primary_key, "lcd_used", &value,
					       1);
		if (ret != 1 || value != 1)
			continue;
		lcd = &lcds[disp];
		lcdp = &lcd_private[disp];
		lcd->priv_data = (void *)lcdp;
		++p_info->lcd_fb_num;

		sprintf(lcd->name, "lcd%d", disp);
		lcd->disp = disp;

		lcd->get_resolution = disp_lcd_get_resolution;
		lcd->enable = disp_lcd_enable;
		lcd->fake_enable = disp_lcd_fake_enable;
		lcd->disable = disp_lcd_disable;
		lcd->is_enabled = disp_lcd_is_enabled;
		lcd->set_bright = disp_lcd_set_bright;
		lcd->get_bright = disp_lcd_get_bright;
		lcd->set_bright_dimming = disp_lcd_set_bright_dimming;
		lcd->get_panel_info = disp_lcd_get_panel_info;

		lcd->set_panel_func = disp_lcd_set_panel_funs;
		lcd->set_open_func = disp_lcd_set_open_func;
		lcd->set_close_func = disp_lcd_set_close_func;
		lcd->backlight_enable = disp_lcd_backlight_enable;
		lcd->backlight_disable = disp_lcd_backlight_disable;
		lcd->pwm_enable = disp_lcd_pwm_enable;
		lcd->pwm_disable = disp_lcd_pwm_disable;
		lcd->power_enable = disp_lcd_power_enable;
		lcd->power_disable = disp_lcd_power_disable;
		lcd->pin_cfg = disp_lcd_pin_cfg;
		lcd->gpio_set_value = disp_lcd_gpio_set_value;
		lcd->gpio_set_direction = disp_lcd_gpio_set_direction;
		lcd->get_dimensions = disp_lcd_get_dimensions;
		lcd->set_gamma_tbl = disp_lcd_set_gamma_tbl;
		lcd->enable_gamma = disp_lcd_enable_gamma;
		lcd->disable_gamma = disp_lcd_disable_gamma;
		lcd->set_color_temperature = disp_lcd_set_color_temperature;
		lcd->get_color_temperature = disp_lcd_get_color_temperature;
		lcd->set_layer = disp_lcd_set_layer;
		lcd->blank = disp_lcd_blank;
		lcd->set_var = disp_lcd_set_var;


		lcd->init = disp_lcd_init;
		lcd->exit = disp_lcd_exit;

		lcd->init(lcd);
		lcd_fb_device_register(lcd);

		disp++;
	}

	return 0;

malloc_err:
	kfree(lcds);
	kfree(lcd_private);
	lcds = NULL;
	lcd_private = NULL;

	return -1;
}

s32 disp_exit_lcd(void)
{
	u32 hwdev_index = 0, disp = 0;
	char primary_key[20];
	int ret = 0, value = 1;
	struct lcd_fb_device *lcd;

	if (!lcds)
		return 0;

	for (hwdev_index = 0; hwdev_index < SUPPORT_MAX_LCD; hwdev_index++) {
		sprintf(primary_key, "lcd_fb%d", disp);
		ret =
		    lcd_fb_script_get_item(primary_key, "lcd_used", &value, 1);
		if (ret != 1 || value != 1)
			continue;

		lcd = &lcds[disp];
		lcd_fb_device_unregister(lcd);
		lcd->exit(lcd);
		disp++;
	}

	kfree(lcds);
	kfree(lcd_private);
	lcds = NULL;
	lcd_private = NULL;
	return 0;
}
