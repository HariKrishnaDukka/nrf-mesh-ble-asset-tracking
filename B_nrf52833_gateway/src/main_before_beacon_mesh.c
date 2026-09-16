#include <stdint.h>
#include <stdbool.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>
#include <zephyr/net/buf.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gap.h>
#include <zephyr/bluetooth/mesh.h>


/* ============================================================
 * B nRF52833
 *
 * Bluetooth Mesh Node + BLE Beacon Scanner
 *
 * Current test:
 *
 * C nRF52833 BLE Beacon
 *          |
 *          | BLE Advertisement
 *          v
 * B nRF52833
 *
 * Mesh remains enabled, but BLE beacon data is NOT
 * transmitted over Mesh yet.
 *
 * Current milestone:
 *
 * C -> BLE ADV -> B -> C BEACON DETECTED
 *
 * ============================================================ */


/* ============================================================
 * B DEVICE UUID
 * ============================================================ */

#define B_UUID \
    { \
        0xB0, 0xB1, 0xB2, 0xB3, \
        0xB4, 0xB5, 0xB6, 0xB7, \
        0xB8, 0xB9, 0xBA, 0xBB, \
        0xBC, 0xBD, 0xBE, 0xBF \
    }

static const uint8_t dev_uuid[16] = B_UUID;


/* ============================================================
 * MESH VENDOR MODEL
 * ============================================================ */

#define VENDOR_COMPANY_ID    0x1234
#define VENDOR_MODEL_ID      0x0001

#define VENDOR_OP_PING \
    BT_MESH_MODEL_OP_3(0x01, VENDOR_COMPANY_ID)


/* ============================================================
 * VENDOR PING HANDLER
 * ============================================================ */

static int vendor_ping_handler(
    const struct bt_mesh_model *model,
    struct bt_mesh_msg_ctx *ctx,
    struct net_buf_simple *buf)
{
    ARG_UNUSED(model);
    ARG_UNUSED(buf);

    printk("\n");
    printk("========================================\n");
    printk("VENDOR PING RECEIVED\n");
    printk("========================================\n");

    printk("Source : 0x%04X\n", ctx->addr);
    printk("NetIdx : 0x%04X\n", ctx->net_idx);
    printk("AppIdx : 0x%04X\n", ctx->app_idx);

    printk("========================================\n");

    return 0;
}


/* ============================================================
 * VENDOR OPCODE TABLE
 * ============================================================ */

static const struct bt_mesh_model_op vendor_ops[] = {
    {
        VENDOR_OP_PING,
        BT_MESH_LEN_EXACT(0),
        vendor_ping_handler,
    },

    BT_MESH_MODEL_OP_END,
};


/* ============================================================
 * SIG MODELS
 *
 * IMPORTANT:
 * Vendor model is NOT placed here.
 * ============================================================ */

static struct bt_mesh_model root_models[] = {
    BT_MESH_MODEL_CFG_SRV,
};


/* ============================================================
 * VENDOR MODELS
 * ============================================================ */

static struct bt_mesh_model vnd_models[] = {
    BT_MESH_MODEL_VND(
        VENDOR_COMPANY_ID,
        VENDOR_MODEL_ID,
        vendor_ops,
        NULL,
        NULL
    ),
};


/* ============================================================
 * MESH ELEMENTS
 *
 * SIG models and Vendor models are separate.
 * ============================================================ */

static struct bt_mesh_elem elements[] = {
    BT_MESH_ELEM(
        0,
        root_models,
        vnd_models
    ),
};


/* ============================================================
 * MESH COMPOSITION
 * ============================================================ */

static const struct bt_mesh_comp comp = {
    .cid = 0xFFFF,
    .elem = elements,
    .elem_count = ARRAY_SIZE(elements),
};


/* ============================================================
 * BLE BEACON PROTOCOL
 *
 * C currently transmits:
 *
 * FF FF 01 01 00 01 00 00 00
 *
 * Byte 0-1 : Company ID
 * Byte 2   : Protocol Version
 * Byte 3   : Message Type
 * Byte 4-5 : Node ID
 * Byte 6-7 : Sequence
 * Byte 8   : Payload Length
 *
 * ============================================================ */

#define BEACON_COMPANY_ID       0xFFFF
#define BEACON_PROTOCOL_VERSION 0x01
#define BEACON_MESSAGE_TYPE     0x01
#define BEACON_NODE_ID          0x0001

#define BEACON_DATA_LENGTH      9U


/* ============================================================
 * BLE BEACON STATISTICS
 * ============================================================ */

static uint32_t beacon_count;


/* ============================================================
 * READ 16-BIT BIG-ENDIAN
 * ============================================================ */

static uint16_t read_u16_be(const uint8_t *data)
{
    return ((uint16_t)data[0] << 8) |
           ((uint16_t)data[1]);
}


/* ============================================================
 * PRINT BLE ADDRESS
 * ============================================================ */

static void print_ble_address(const bt_addr_le_t *addr)
{
    printk(
        "%02X:%02X:%02X:%02X:%02X:%02X",
        addr->a.val[5],
        addr->a.val[4],
        addr->a.val[3],
        addr->a.val[2],
        addr->a.val[1],
        addr->a.val[0]
    );
}


/* ============================================================
 * PARSE C BEACON
 * ============================================================ */

static bool parse_c_beacon(
    const uint8_t *data,
    uint8_t len,
    int8_t rssi,
    const bt_addr_le_t *addr)
{
    uint16_t company_id;
    uint8_t protocol;
    uint8_t message_type;
    uint16_t node_id;
    uint16_t sequence;
    uint8_t payload_len;


    /* Minimum expected manufacturer data length */

    if (len < BEACON_DATA_LENGTH) {
        return false;
    }


    /* --------------------------------------------------------
     * Parse packet
     * -------------------------------------------------------- */

    company_id = read_u16_be(&data[0]);

    protocol = data[2];

    message_type = data[3];

    node_id = read_u16_be(&data[4]);

    sequence = read_u16_be(&data[6]);

    payload_len = data[8];


    /* --------------------------------------------------------
     * Validate Company ID
     * -------------------------------------------------------- */

    if (company_id != BEACON_COMPANY_ID) {
        return false;
    }


    /* --------------------------------------------------------
     * Validate protocol version
     * -------------------------------------------------------- */

    if (protocol != BEACON_PROTOCOL_VERSION) {
        return false;
    }


    /* --------------------------------------------------------
     * Validate message type
     * -------------------------------------------------------- */

    if (message_type != BEACON_MESSAGE_TYPE) {
        return false;
    }


    /* --------------------------------------------------------
     * Validate Node ID
     * -------------------------------------------------------- */

    if (node_id != BEACON_NODE_ID) {
        return false;
    }


    /* --------------------------------------------------------
     * Valid C beacon
     * -------------------------------------------------------- */

    beacon_count++;


    printk("\n");
    printk("========================================\n");
    printk("C BEACON DETECTED\n");
    printk("========================================\n");

    printk("Address  : ");
    print_ble_address(addr);
    printk("\n");

    printk("RSSI     : %d dBm\n", rssi);

    printk("Company  : 0x%04X\n", company_id);

    printk("Protocol : %u\n", protocol);

    printk("Type     : %u\n", message_type);

    printk("Node ID  : 0x%04X\n", node_id);

    printk("Sequence : %u\n", sequence);

    printk("Payload  : %u bytes\n", payload_len);

    printk("Count    : %u\n", beacon_count);

    printk("========================================\n");


    return true;
}


/* ============================================================
 * BLE SCAN RECEIVE CALLBACK
 * ============================================================ */

static void ble_scan_recv(
    const struct bt_le_scan_recv_info *info,
    struct net_buf_simple *buf)
{
    while (buf->len > 1U) {

        uint8_t field_len;
        uint8_t ad_type;
        uint8_t data_len;
        const uint8_t *data;


        /* ----------------------------------------------------
         * AD structure length
         * ---------------------------------------------------- */

        field_len = net_buf_simple_pull_u8(buf);


        if (field_len == 0U) {
            break;
        }


        /*
         * field_len includes the AD Type byte.
         *
         * Therefore field_len bytes must still be
         * available in the buffer.
         */

        if (field_len > buf->len) {
            break;
        }


        /* ----------------------------------------------------
         * AD Type
         * ---------------------------------------------------- */

        ad_type = net_buf_simple_pull_u8(buf);


        data_len = field_len - 1U;

        data = buf->data;


        /* ----------------------------------------------------
         * Manufacturer Specific Data
         * ---------------------------------------------------- */

        if (ad_type == BT_DATA_MANUFACTURER_DATA) {

            (void)parse_c_beacon(
                data,
                data_len,
                info->rssi,
                info->addr
            );
        }


        /* ----------------------------------------------------
         * Move to next AD structure
         * ---------------------------------------------------- */

        net_buf_simple_pull(buf, data_len);
    }
}


/* ============================================================
 * BLE SCAN CALLBACK STRUCTURE
 * ============================================================ */

static struct bt_le_scan_cb scan_callbacks = {
    .recv = ble_scan_recv,
};


/* ============================================================
 * START BLE SCANNER
 * ============================================================ */

static int start_ble_scanner(void)
{
    struct bt_le_scan_param scan_param = {
        /*
         * Passive scanning is enough because C's
         * manufacturer data is inside the advertisement.
         */
        .type = BT_LE_SCAN_TYPE_PASSIVE,

        .options = BT_LE_SCAN_OPT_NONE,

        .interval = BT_GAP_SCAN_FAST_INTERVAL,

        .window = BT_GAP_SCAN_FAST_WINDOW,
    };

    int err;


    /* Register scan callback */

    bt_le_scan_cb_register(&scan_callbacks);


    /* Start scanner */

    err = bt_le_scan_start(
        &scan_param,
        NULL
    );


    if (err) {

        printk(
            "ERROR: BLE scan start failed: %d\n",
            err
        );

        return err;
    }


    printk("\n");
    printk("========================================\n");
    printk("BLE SCANNER STARTED\n");
    printk("========================================\n");

    printk("Mode       : PASSIVE\n");

    printk("Target     : C_BEACON\n");

    printk(
        "Company ID : 0x%04X\n",
        BEACON_COMPANY_ID
    );

    printk(
        "Protocol   : %u\n",
        BEACON_PROTOCOL_VERSION
    );

    printk(
        "Message    : %u\n",
        BEACON_MESSAGE_TYPE
    );

    printk(
        "Node ID    : 0x%04X\n",
        BEACON_NODE_ID
    );

    printk("========================================\n");


    return 0;
}


/* ============================================================
 * MESH PROVISIONING CALLBACK
 * ============================================================ */

static void prov_complete(
    uint16_t net_idx,
    uint16_t addr)
{
    printk("\n");
    printk("========================================\n");
    printk("B PROVISIONING COMPLETE\n");
    printk("========================================\n");

    printk(
        "NetIdx : 0x%04X\n",
        net_idx
    );

    printk(
        "Addr   : 0x%04X\n",
        addr
    );

    printk(
        "B IS NOW A PROVISIONED MESH NODE\n"
    );

    printk("========================================\n");
}


/* ============================================================
 * MESH RESET CALLBACK
 * ============================================================ */

static void prov_reset(void)
{
    printk("MESH RESET CALLBACK\n");
}


/* ============================================================
 * PROVISIONING STRUCTURE
 * ============================================================ */

static const struct bt_mesh_prov prov = {
    .uuid = dev_uuid,

    .complete = prov_complete,

    .reset = prov_reset,
};


/* ============================================================
 * BLUETOOTH READY
 * ============================================================ */

static void bt_ready(int err)
{
    if (err) {

        printk(
            "ERROR: Bluetooth initialization failed: %d\n",
            err
        );

        return;
    }


    printk(
        "Bluetooth initialized successfully\n"
    );


    /* --------------------------------------------------------
     * Initialize Bluetooth Mesh
     * -------------------------------------------------------- */

    printk(
        "Initializing Bluetooth Mesh...\n"
    );


    err = bt_mesh_init(
        &prov,
        &comp
    );


    if (err) {

        printk(
            "ERROR: Bluetooth Mesh initialization failed: %d\n",
            err
        );

        return;
    }


    printk(
        "Bluetooth Mesh initialized successfully\n"
    );


    /* --------------------------------------------------------
     * Enable PB-ADV provisioning
     * -------------------------------------------------------- */

    err = bt_mesh_prov_enable(
        BT_MESH_PROV_ADV
    );


    if (err) {

        printk(
            "ERROR: Mesh provisioning enable failed: %d\n",
            err
        );

        return;
    }


    printk(
        "Mesh PB-ADV provisioning enabled\n"
    );


    /* --------------------------------------------------------
     * Start BLE Observer
     * -------------------------------------------------------- */

    err = start_ble_scanner();


    if (err) {

        printk(
            "ERROR: BLE scanner initialization failed: %d\n",
            err
        );

        return;
    }


    /* --------------------------------------------------------
     * Gateway ready
     * -------------------------------------------------------- */

    printk("\n");
    printk("========================================\n");
    printk("B GATEWAY READY\n");
    printk("========================================\n");

    printk("Device      : B nRF52833\n");

    printk(
        "Vendor      : 0x%04X / 0x%04X\n",
        VENDOR_COMPANY_ID,
        VENDOR_MODEL_ID
    );

    printk("Mesh        : ENABLED\n");

    printk("PB-ADV      : ENABLED\n");

    printk("BLE Scanner : ENABLED\n");

    printk("========================================\n");
}


/* ============================================================
 * MAIN
 * ============================================================ */

int main(void)
{
    int err;


    printk("\n");
    printk("========================================\n");
    printk("       B nRF52833 GATEWAY START\n");
    printk("========================================\n");

    printk(
        "Role        : Mesh Node + BLE Scanner\n"
    );

    printk(
        "Vendor      : 0x%04X / 0x%04X\n",
        VENDOR_COMPANY_ID,
        VENDOR_MODEL_ID
    );

    printk("========================================\n");


    /* --------------------------------------------------------
     * Reset Mesh state for clean testing
     *
     * IMPORTANT:
     * bt_mesh_reset() returns void in this NCS version.
     * -------------------------------------------------------- */

    printk(
        "Resetting Mesh state...\n"
    );

    bt_mesh_reset();


    /* --------------------------------------------------------
     * Start Bluetooth
     * -------------------------------------------------------- */

    printk(
        "Starting Bluetooth...\n"
    );


    err = bt_enable(bt_ready);


    if (err) {

        printk(
            "ERROR: bt_enable() failed: %d\n",
            err
        );

        return 0;
    }


    /* --------------------------------------------------------
     * Main loop
     * -------------------------------------------------------- */

    while (1) {

        printk(
            "B Gateway alive | BLE detections: %u\n",
            beacon_count
        );

        k_sleep(K_SECONDS(5));
    }


    return 0;
}