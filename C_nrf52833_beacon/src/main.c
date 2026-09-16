#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/sys/printk.h>
#include <stdint.h>

/*
 * C_BEACON application protocol
 *
 * Manufacturer Data:
 *
 * Byte 0-1 : Company ID
 * Byte 2   : Protocol Version
 * Byte 3   : Message Type
 * Byte 4-5 : Node ID
 * Byte 6-7 : Sequence Number
 * Byte 8   : Payload Length
 */

#define BEACON_COMPANY_ID       0xFFFF
#define BEACON_PROTOCOL_VERSION 0x01
#define BEACON_MESSAGE_TYPE     0x01
#define BEACON_NODE_ID          0x0001

/*
 * Advertising interval:
 *
 * 160 * 0.625 ms = 100 ms
 */
#define BEACON_ADV_INTERVAL     160

static uint16_t sequence_number;

/*
 * Manufacturer-specific payload.
 *
 * FF FF = development/test Company ID
 */
static uint8_t beacon_manufacturer_data[] = {
	(BEACON_COMPANY_ID >> 8) & 0xFF,
	BEACON_COMPANY_ID & 0xFF,

	BEACON_PROTOCOL_VERSION,

	BEACON_MESSAGE_TYPE,

	(BEACON_NODE_ID >> 8) & 0xFF,
	BEACON_NODE_ID & 0xFF,

	0x00,	/* Sequence MSB */
	0x00,	/* Sequence LSB */

	0x00	/* Payload length */
};

/*
 * BLE advertising packet.
 *
 * Contains:
 *   1. Flags
 *   2. Complete device name
 *   3. Manufacturer-specific data
 */
static const struct bt_data beacon_ad[] = {

	BT_DATA_BYTES(BT_DATA_FLAGS,
		      BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR),

	BT_DATA(BT_DATA_NAME_COMPLETE,
		CONFIG_BT_DEVICE_NAME,
		sizeof(CONFIG_BT_DEVICE_NAME) - 1),

	BT_DATA(BT_DATA_MANUFACTURER_DATA,
		beacon_manufacturer_data,
		sizeof(beacon_manufacturer_data))
};


/*
 * Update sequence number in the application packet.
 */
static void beacon_update_sequence(void)
{
	beacon_manufacturer_data[6] =
		(uint8_t)((sequence_number >> 8) & 0xFF);

	beacon_manufacturer_data[7] =
		(uint8_t)(sequence_number & 0xFF);
}


/*
 * Print the current manufacturer packet.
 */
static void beacon_print_packet(void)
{
	printk("Manufacturer Data: ");

	for (size_t i = 0;
	     i < sizeof(beacon_manufacturer_data);
	     i++) {
		printk("%02X ", beacon_manufacturer_data[i]);
	}

	printk("\r\n");
}


/*
 * Start BLE non-connectable advertising.
 */
static int beacon_start_advertising(void)
{
	struct bt_le_adv_param adv_param = {
		.id = BT_ID_DEFAULT,

		.options = BT_LE_ADV_OPT_USE_IDENTITY,

		.interval_min = BEACON_ADV_INTERVAL,
		.interval_max = BEACON_ADV_INTERVAL
	};

	return bt_le_adv_start(&adv_param,
			       beacon_ad,
			       ARRAY_SIZE(beacon_ad),
			       NULL,
			       0);
}


int main(void)
{
	int err;

	printk("\r\n");
	printk("====================================\r\n");
	printk("          C BEACON STARTING\r\n");
	printk("====================================\r\n");

	printk("Device Name   : %s\r\n",
	       CONFIG_BT_DEVICE_NAME);

	printk("Node ID       : 0x%04X\r\n",
	       BEACON_NODE_ID);

	printk("Protocol      : %u\r\n",
	       BEACON_PROTOCOL_VERSION);

	printk("Message Type  : %u\r\n",
	       BEACON_MESSAGE_TYPE);

	printk("Adv Interval  : 100 ms\r\n");

	printk("Initializing Bluetooth...\r\n");

	/*
	 * Initialize Bluetooth subsystem.
	 */
	err = bt_enable(NULL);

	if (err) {
		printk("ERROR: Bluetooth initialization failed: %d\r\n",
		       err);

		return 0;
	}

	printk("Bluetooth initialized successfully\r\n");

	/*
	 * Initial sequence number.
	 */
	sequence_number = 0;

	beacon_update_sequence();

	/*
	 * Start non-connectable BLE advertising.
	 */
	printk("Starting beacon advertising...\r\n");

	err = beacon_start_advertising();

	if (err) {
		printk("ERROR: Advertising start failed: %d\r\n",
		       err);

		return 0;
	}

	printk("Beacon advertising started successfully\r\n");

	beacon_print_packet();

	/*
	 * Main beacon loop.
	 */
	while (1) {

		k_sleep(K_SECONDS(1));

		sequence_number++;

		beacon_update_sequence();

		printk("Beacon sequence: %u\r\n",
		       sequence_number);

		beacon_print_packet();
	}

	return 0;
}