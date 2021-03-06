/*
 * Copyright (c) 2016-2017, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_GPIO

#include <sys/types.h>
#include <tegrabl_utils.h>
#include <tegrabl_debug.h>
#include <tegrabl_error.h>
#include <tegrabl_drf.h>
#include <tegrabl_io.h>

#include <tegrabl_gpio.h>
#include <tegrabl_gpio_local.h>
#include <tegrabl_gpio_hw.h>

#define TEGRA_GPIO_MAIN_DRIVER_NAME "tegra_gpio_main_driver"
#define TEGRA_GPIO_AON_DRIVER_NAME  "tegra_gpio_aon_driver"

#define TEGRABL_MAX_GPIOS_PER_BANK 8

static bool s_is_gpio_initialised;

/**
 * @brief check if gpio num is valid
 *
 * @param id controllor id struct
 * @param gpio gpio num
 *
 * @return true if valid
 */
static inline bool is_gpio_valid(const struct tegrabl_gpio_id *id,
								 uint32_t gpio)
{
	uint32_t bank = gpio / TEGRABL_MAX_GPIOS_PER_BANK;
	return bank < id->bank_count;
}

/**
 * @brief get reg addr
 *
 * @param id controllor id struct
 * @param bank_gpio gpio bank id
 * @param reg reg offset
 *
 * @return reg addr
 * */
static inline uint32_t reg(const struct tegrabl_gpio_id *id,
						   uint32_t bank_gpio, uint32_t reg)
{
	uint32_t bank = bank_gpio / TEGRABL_MAX_GPIOS_PER_BANK;
	uint32_t gpio = bank_gpio % TEGRABL_MAX_GPIOS_PER_BANK;
	uint32_t bank_base = id->bank_bases[bank];

	return id->base_addr + bank_base + (reg - GPIO_N_ENABLE_CONFIG_00_0) +
			(gpio * (GPIO_N_ENABLE_CONFIG_01_0 - GPIO_N_ENABLE_CONFIG_00_0));
}

/**
 * @brief Tegra GPIO driver API for reading a GPIO pin state.
 *
 * @param gpio_num The GPIO pin number.
 * @param state The variable to read back the current GPIO pin state.
 *
 * @return TEGRABL_NO_ERROR on success with 'state' argument having GPIO pin
 *         state otherwise error.
 */
static tegrabl_error_t tegrabl_gpio_read(uint32_t gpio_num,
										 enum gpio_pin_state *state,
										 void *drv_data)
{
	uint32_t reg_val;
	bool pin_state;
	const struct tegrabl_gpio_id *id = NULL;
	tegrabl_error_t status = TEGRABL_NO_ERROR;

	if (drv_data == NULL) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	id = (const struct tegrabl_gpio_id *)drv_data;

	if (!is_gpio_valid(id, gpio_num)) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	reg_val = NV_READ32(reg(id, gpio_num, GPIO_N_INPUT_00_0));
	pin_state = !!NV_DRF_VAL(GPIO, N_INPUT_00, GPIO_IN, reg_val);

	*state = (pin_state == true) ? GPIO_PIN_STATE_HIGH : GPIO_PIN_STATE_LOW;

	return status;
}

/**
 * @brief config gpio direction to input
 *
 * @param id controller id struct
 * @param gpio gpio id
 * @param state gpio config mode
 */
static void gpio_set_direction(const struct tegrabl_gpio_id *id,
							   uint32_t gpio, enum gpio_pin_mode state)
{
	uint32_t val;

	val = NV_READ32(reg(id, gpio, GPIO_N_ENABLE_CONFIG_00_0));
	if (state == GPIO_PINMODE_INPUT) {
		val |= NV_DRF_DEF(GPIO, N_ENABLE_CONFIG_00, GPIO_ENABLE, ENABLE) |
			   NV_DRF_DEF(GPIO, N_ENABLE_CONFIG_00, IN_OUT, IN);
	} else if (state == GPIO_PINMODE_OUTPUT) {
		val |= NV_DRF_DEF(GPIO, N_ENABLE_CONFIG_00, GPIO_ENABLE, ENABLE) |
			   NV_DRF_DEF(GPIO, N_ENABLE_CONFIG_00, IN_OUT, OUT);
	}
	NV_WRITE32(reg(id, gpio, GPIO_N_ENABLE_CONFIG_00_0), val);
}

/**
 * @brief Tegra GPIO driver API for configuring a GPIO pin.
 *        Sets the pin mode either to INPUT or OUTPUT based on mode value.
 *
 * @param gpio_num The GPIO pin number.
 * @param mode gpio config mode
 * @param drv_data driver private data
 *
 * @return TEGRABL_NO_ERROR on success otherwise error.
 */
static tegrabl_error_t tegrabl_gpio_config(uint32_t gpio_num,
										   enum gpio_pin_mode mode,
										   void *drv_data)
{
	tegrabl_error_t status = TEGRABL_NO_ERROR;
	const struct tegrabl_gpio_id *id = NULL;

	if (drv_data == NULL) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	id = (const struct tegrabl_gpio_id *)drv_data;

	if (!is_gpio_valid(id, gpio_num)) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	switch (mode) {
	case GPIO_PINMODE_INPUT:
	case GPIO_PINMODE_OUTPUT:
		gpio_set_direction(id, gpio_num, mode);
		break;

	default:
		break;
	}

	return status;
}

/**
 * @brief Tegra GPIO driver API for setting a GPIO pin state.
 *
 * @param gpio_num The GPIO pin number.
 * @param state The GPIO pin state (LOW/HIGH).
 * @param drv_data driver private data
 *
 * @return TEGRABL_NO_ERROR on success otherwise error.
 */
static tegrabl_error_t tegrabl_gpio_write(uint32_t gpio_num,
										  enum gpio_pin_state state,
										  void *drv_data)
{
	uint32_t reg_val;
	bool pin_state;
	const struct tegrabl_gpio_id *id = NULL;

	if (drv_data == NULL) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	id = (const struct tegrabl_gpio_id *)drv_data;

	if (!is_gpio_valid(id, gpio_num)) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	pin_state = (state == GPIO_PIN_STATE_LOW) ? 0 : 1;
	reg_val = NV_DRF_NUM(GPIO, N_OUTPUT_VALUE_00, GPIO_OUT_VAL, pin_state);
	NV_WRITE32(reg(id, gpio_num, GPIO_N_OUTPUT_VALUE_00_0), reg_val);

	return TEGRABL_NO_ERROR;
}

static struct gpio_driver_ops tegra_gpio_ops = {
	.read = &tegrabl_gpio_read,
	.write = &tegrabl_gpio_write,
	.config = &tegrabl_gpio_config
};

static struct gpio_driver drv[] = {
	{
		.name = TEGRA_GPIO_MAIN_DRIVER_NAME,
		.ops = &tegra_gpio_ops,
		.chip_id = TEGRA_GPIO_MAIN_CHIPID,
		.driver_data = (void *)&tegra_gpio_id_main,
	},
	{
		.name = TEGRA_GPIO_AON_DRIVER_NAME,
		.ops = &tegra_gpio_ops,
		.chip_id = TEGRA_GPIO_AON_CHIPID,
		.driver_data = (void *)&tegra_gpio_id_aon,
	},
};

tegrabl_error_t tegrabl_gpio_driver_init()
{
	tegrabl_error_t status = TEGRABL_NO_ERROR;
	uint32_t i;

	if (s_is_gpio_initialised) {
		return TEGRABL_NO_ERROR;
	}

	for (i = 0; i < ARRAY_SIZE(drv); i++) {
		status = tegrabl_gpio_driver_register(&drv[i]);
		if (status != TEGRABL_NO_ERROR)
			pr_critical("%s: failed to register tegra gpio driver: %s\n",
						__func__, drv[i].name);
		else {
			s_is_gpio_initialised = true;
			pr_debug("%s: tegra gpio driver:%s registered successfully\n",
					 __func__, drv[i].name);
		}
	}

	return status;
}
