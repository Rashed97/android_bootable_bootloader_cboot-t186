/*
 * Copyright (c) 2015, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef TEGRABL_CONSOLE_H
#define TEGRABL_CONSOLE_H

#include <stdint.h>
#include <tegrabl_error.h>
#include <stdbool.h>

/**
* @brief Console Interface types
*/
enum tegrabl_console_interface {
	TEGRABL_CONSOLE_UART,
	TEGRABL_CONSOLE_SEMIHOST
};

/**
* @brief Console structure
*/
struct tegrabl_console {
	enum tegrabl_console_interface interface;
	uint32_t instance;
	void *dev;
	bool is_registered;
	tegrabl_error_t (*getchar)(struct tegrabl_console *hconsole, char *ch);
	tegrabl_error_t (*putchar)(struct tegrabl_console *hconsole, char ch);
	tegrabl_error_t (*puts)(struct tegrabl_console *hconsole, char *str);
	tegrabl_error_t (*close)(struct tegrabl_console *hconsole);
};

/**
* @brief Registers the console layer with given interface.
*
* @param interface Interface to the console.
* @param instance Instance of the controller.
* @param data Interface parameters.
*
* @return  TEGRABL_NO_ERROR if success. Error code in case of failure.
*/
tegrabl_error_t tegrabl_console_register(
	enum tegrabl_console_interface interface, uint32_t instance, void *data);

/**
* @brief Opens the tegrabl_console interface.
*
* @return  TEGRABL_NO_ERROR if success. Error code in case of failure.
*/
struct tegrabl_console *tegrabl_console_open(void);

/**
* @brief Sends the character to console.
*
* @param interface Interface to the console.
* @param ch character to be sent.
*
* @return  TEGRABL_NO_ERROR if success. Error code in case of failure.
*/
tegrabl_error_t tegrabl_console_putchar(struct tegrabl_console *hconsole,
	char ch);

/**
* @brief Receives the character from console.
*
* @param interface Interface to the console.
* @param ch Address at which received character has to be stored.
*
* @return  TEGRABL_NO_ERROR if success. Error code in case of failure.
*/
tegrabl_error_t tegrabl_console_getchar(struct tegrabl_console *hconsole,
	char *ch);

/**
* @brief Prints the given string on the console.
*
* @param str String to be print
*
* @return  TEGRABL_NO_ERROR if success. Error code in case of failure.
*/
tegrabl_error_t tegrabl_console_puts(struct tegrabl_console *hconsole,
	char *str);

/**
* @brief Closes the usb tegrabl_console interface.
*
* @param instance Instance of the usb controller.
*
* @return  TEGRABL_NO_ERROR if success. Error code in case of failure.
*/
tegrabl_error_t tegrabl_console_close(struct tegrabl_console *hconsole);

#endif
