/*
 * Copyright(c) 2019 Xilinx, Inc. All rights reserved.
 *
 * This source code is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * The full GNU General Public License is included in this distribution in
 * the file called "COPYING".
 */

#ifndef QDMA_RESOURCE_MGMT_H_
#define QDMA_RESOURCE_MGMT_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * DOC: QDMA resource management interface definitions
 *
 * Header file *qdma_resource_mgmt.h* defines data structures and function
 * signatures exported for QDMA queue management.
 */
#include "qdma_platform_env.h"

/**
 * Error codes
 */
#define QDMA_RESOURCE_MGMT_SUCCESS             (0)
#define QDMA_RESOURCE_MGMT_MEMALLOC_FAIL       (1)
#define QDMA_MASTER_RESOURCE_ALREADY_EXISTS    (2)
#define QDMA_MASTER_RESOURCE_DOES_NOT_EXIST    (3)
#define QDMA_DEV_ALREADY_EXISTS                (4)
#define QDMA_DEV_DOES_NOT_EXIST                (5)
#define QDMA_RESOURCE_NOT_ENOUGH_QUEUE         (6)
#define QDMA_QMAX_PROG_FREEZE                  (7)

/**
 * enum qdma_dev_q_range: Q ranage check
 */
enum qdma_dev_q_range {
	/** @QDMA_DEV_Q_IN_RANGE: Q belongs to dev */
	QDMA_DEV_Q_IN_RANGE,
	/** @QDMA_DEV_Q_OUT_OF_RANGE: Q does not belong to dev */
	QDMA_DEV_Q_OUT_OF_RANGE,
	/** @QDMA_DEV_Q_RANGE_MAX: total Q validity states */
	QDMA_DEV_Q_RANGE_MAX
};

/*****************************************************************************/
/**
 * qdma_master_resource_create(): create the master q resource
 *
 * @pci_bus_num:  pci bus number this master resource belongs to
 * @q_base:     base from which this master resource needs to be created
 * @total_q:     total queues in this master resource
 *
 * A master resource per driver per board is created to manage the queues
 * allocated to this driver.
 *
 * Return:	0  : success and < 0: failure
 *****************************************************************************/
int qdma_master_resource_create(uint32_t pci_bus_num, int q_base,
				uint32_t total_q);

/*****************************************************************************/
/**
 * qdma_master_resource_destroy(): destroy the master q resource
 *
 * @pci_bus_num:  pci bus number this master resource belongs to
 *
 * Return:	None
 *****************************************************************************/
void qdma_master_resource_destroy(uint32_t pci_bus_num);

/*****************************************************************************/
/**
 * qdma_dev_entry_create(): create a device entry for @func_id
 *
 * @pci_bus_num:  pci bus number
 * @func_id:     device identification id
 *
 * A device entry is to be created on every function probe.
 *
 * Return:	0  : success and < 0: failure
 *****************************************************************************/
int qdma_dev_entry_create(uint32_t pci_bus_num, uint16_t func_id);

/*****************************************************************************/
/**
 * qdma_dev_entry_destroy(): destroy device entry for @func_id
 *
 * @pci_bus_num:  pci bus number
 * @func_id:     device identification id
 *
 * Return:	None
 *****************************************************************************/
void qdma_dev_entry_destroy(uint32_t pci_bus_num, uint32_t func_id);

/*****************************************************************************/
/**
 * qdma_dev_update(): update qmax for the device
 *
 * @pci_bus_num:  pci bus number
 * @func_id:     device identification id
 * @dev_type:    device type
 * @qmax:        qmax for this device
 * @qbase:       output qbase for this device
 *
 * This API is to be called for update request of qmax of any function.
 *
 * Return:	0  : success and < 0: failure
 *****************************************************************************/
int qdma_dev_update(uint32_t pci_bus_num, uint32_t func_id,
		    uint32_t qmax, int *qbase);

/*****************************************************************************/
/**
 * qdma_dev_qinfo_get(): get device info
 *
 * @pci_bus_num:  pci bus number
 * @func_id:     device identification id
 * @dev_type:    device type
 * @qmax:        output qmax for this device
 * @qbase:       output qbase for this device
 *
 * This API can be used get the qbase and qmax for any function
 *
 * Return:	0  : success and < 0: failure
 *****************************************************************************/
int qdma_dev_qinfo_get(uint32_t pci_bus_num, uint32_t func_id,
		       int *qbase, uint32_t *qmax);

/*****************************************************************************/
/**
 * qdma_dev_is_queue_in_range(): check if queue belongs to this device
 *
 * @pci_bus_num:  pci bus number
 * @func_id:     device identification id
 * @qid_hw:      hardware queue id
 *
 * This API checks if the queue ID is in valid range for function specified
 *
 * Return:	@QDMA_DEV_Q_IN_RANGE  : valid and
 * @QDMA_DEV_Q_OUT_OF_RANGE: invalid
 *****************************************************************************/
enum qdma_dev_q_range qdma_dev_is_queue_in_range(uint32_t pci_bus_num,
						 uint32_t func_id,
						 uint32_t qid_hw);

/*****************************************************************************/
/**
 * qdma_dev_increment_active_queue(): increment active queue count
 *
 * @pci_bus_num:  pci bus number
 * @func_id:     device identification id
 *
 * This API is used to increment the active queue count of this function
 *
 * Return:	0  : success and < 0: failure
 *****************************************************************************/
int qdma_dev_increment_active_queue(uint32_t pci_bus_num, uint32_t func_id);

/*****************************************************************************/
/**
 * qdma_dev_decrement_active_queue(): increment active queue count
 *
 * @pci_bus_num:  pci bus number
 * @func_id:     device identification id
 *
 * This API is used to increment the active queue count of this function
 *
 * Return:	0  : success and < 0: failure
 *****************************************************************************/
int qdma_dev_decrement_active_queue(uint32_t pci_bus_num, uint32_t func_id);

/*****************************************************************************/
/**
 * qdma_is_active_queue(): check if any queue is active
 *
 * @pci_bus_num:  pci bus number
 *
 * This API is used to check if any active queue is present.
 *
 * Return:	active queue count
 *****************************************************************************/
uint32_t qdma_get_active_queue_count(uint32_t pci_bus_numd);

/*****************************************************************************/
/**
 * qdma_get_device_active_queue_count(): get device active queue count
 *
 * @pci_bus_num:  pci bus number
 * @func_id:     device identification id
 *
 * This API is used to get the active queue count of this function
 *
 * Return:	0  : success and < 0: failure
 *****************************************************************************/
uint32_t qdma_get_device_active_queue_count(uint32_t pci_bus_num,
						uint32_t func_id);

#ifdef __cplusplus
}
#endif

#endif /* LIBQDMA_QDMA_RESOURCE_MGMT_H_ */
