#include <stdint.h>
#include <stdbool.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/net/buf.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/mesh.h>
#include <zephyr/bluetooth/mesh/cdb.h>
#include <zephyr/bluetooth/mesh/cfg.h>
#include <zephyr/bluetooth/mesh/cfg_cli.h>


/*
 * ============================================================
 * A nRF5340 - Bluetooth Mesh Provisioner / Controller
 *
 * Test flow:
 *
 * A nRF5340
 *      |
 *      | PB-ADV provisioning
 *      v
 * B nRF52833
 *
 * Then:
 *
 * A
 *  |
 *  +-- AppKey Add -----------------> B
 *  |
 *  +-- Vendor Model Bind ----------> B
 *  |
 *  +-- Vendor PING ----------------> B
 *
 * ============================================================
 */


/* ------------------------------------------------------------
 * UUIDs
 * ------------------------------------------------------------ */

#define A_UUID \
    { 0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, \
      0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF }

#define B_UUID \
    { 0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, \
      0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF }

static const uint8_t dev_uuid[16] = A_UUID;
static const uint8_t target_uuid[16] = B_UUID;


/* ------------------------------------------------------------
 * Mesh Network Configuration
 * ------------------------------------------------------------ */

#define NET_IDX        0x0000
#define A_ADDR         0x0001


/* ------------------------------------------------------------
 * Network Key
 * ------------------------------------------------------------ */

static const uint8_t net_key[16] = {
    0x10, 0x11, 0x12, 0x13,
    0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1A, 0x1B,
    0x1C, 0x1D, 0x1E, 0x1F
};


/* ------------------------------------------------------------
 * A Device Key
 * ------------------------------------------------------------ */

static const uint8_t a_dev_key[16] = {
    0x20, 0x21, 0x22, 0x23,
    0x24, 0x25, 0x26, 0x27,
    0x28, 0x29, 0x2A, 0x2B,
    0x2C, 0x2D, 0x2E, 0x2F
};


/* ------------------------------------------------------------
 * Application Key
 * ------------------------------------------------------------ */

#define APP_IDX 0x0001

static const uint8_t app_key[16] = {
    0x30, 0x31, 0x32, 0x33,
    0x34, 0x35, 0x36, 0x37,
    0x38, 0x39, 0x3A, 0x3B,
    0x3C, 0x3D, 0x3E, 0x3F
};


/* ------------------------------------------------------------
 * Vendor Model
 * ------------------------------------------------------------ */

#define VENDOR_COMPANY_ID 0x1234
#define VENDOR_MODEL_ID   0x0001

#define VENDOR_OP_PING \
    BT_MESH_MODEL_OP_3(0x01, VENDOR_COMPANY_ID)

#define VENDOR_OP_BEACON_REPORT \
    BT_MESH_MODEL_OP_3(0x02, VENDOR_COMPANY_ID)

/*
 * A does not need to receive a vendor opcode for this test.
 * It only needs the vendor model to exist in its composition
 * so that the AppKey can be bound to it.
 */

static int beacon_report_handler(const struct bt_mesh_model *model,
                                 struct bt_mesh_msg_ctx *ctx,
                                 struct net_buf_simple *buf)
{
    ARG_UNUSED(model);

    if (buf->len != 5) {
        printk("\n");
        printk("========================================\n");
        printk("INVALID BEACON REPORT\n");
        printk("Expected : 5 bytes\n");
        printk("Received : %u bytes\n", buf->len);
        printk("========================================\n");
        return 0;
    }

    uint16_t beacon_node_id = net_buf_simple_pull_le16(buf);
    uint16_t sequence       = net_buf_simple_pull_le16(buf);
    int8_t rssi             = (int8_t)net_buf_simple_pull_u8(buf);

    printk("\n");
    printk("========================================\n");
    printk("       BEACON REPORT RECEIVED\n");
    printk("========================================\n");

    printk("Source       : 0x%04X\n", ctx->addr);
    printk("NetIdx       : 0x%04X\n", ctx->net_idx);
    printk("AppIdx       : 0x%04X\n", ctx->app_idx);
    printk("Beacon ID    : 0x%04X\n", beacon_node_id);
    printk("Sequence     : %u\n", sequence);
    printk("RSSI         : %d dBm\n", rssi);

    printk("========================================\n");

    return 0;
}

static const struct bt_mesh_model_op vendor_ops[] = {
    {
        VENDOR_OP_BEACON_REPORT,
        BT_MESH_LEN_EXACT(5),
        beacon_report_handler,
    },

    BT_MESH_MODEL_OP_END,
};


/* ------------------------------------------------------------
 * Configuration Client
 * ------------------------------------------------------------ */

static struct bt_mesh_cfg_cli cfg_cli;


/* ------------------------------------------------------------
 * SIG Models
 *
 * IMPORTANT:
 *
 * Only SIG models go into root_models[].
 * ------------------------------------------------------------ */

static struct bt_mesh_model root_models[] = {

    /*
     * Configuration Server
     */
    BT_MESH_MODEL_CFG_SRV,

    /*
     * Configuration Client
     */
    BT_MESH_MODEL_CFG_CLI(&cfg_cli),
};


/* ------------------------------------------------------------
 * Vendor Models
 *
 * IMPORTANT:
 *
 * Vendor models must be in a separate array.
 * ------------------------------------------------------------ */

static struct bt_mesh_model vnd_models[] = {

    BT_MESH_MODEL_VND(
        VENDOR_COMPANY_ID,
        VENDOR_MODEL_ID,
        vendor_ops,
        NULL,
        NULL
    ),
};


/* ------------------------------------------------------------
 * Mesh Element
 * ------------------------------------------------------------ */

static struct bt_mesh_elem elements[] = {

    BT_MESH_ELEM(
        0,
        root_models,
        vnd_models
    ),
};


/* ------------------------------------------------------------
 * Mesh Composition
 * ------------------------------------------------------------ */

static const struct bt_mesh_comp comp = {

    .cid = 0xFFFF,

    .elem = elements,

    .elem_count = ARRAY_SIZE(elements),
};


/* ------------------------------------------------------------
 * State
 * ------------------------------------------------------------ */

static bool provisioning_started;

/*
 * Actual address assigned to B by the provisioner.
 */
static uint16_t provisioned_b_addr;


/* ------------------------------------------------------------
 * Dedicated Configuration Workqueue
 *
 * Blocking Configuration Client transactions are executed
 * here instead of Zephyr's system workqueue.
 * ------------------------------------------------------------ */

#define CONFIG_WORKQ_STACK_SIZE 2048
#define CONFIG_WORKQ_PRIORITY   7

K_THREAD_STACK_DEFINE(
    config_workq_stack,
    CONFIG_WORKQ_STACK_SIZE
);

static struct k_work_q config_workq;

static struct k_work_delayable configure_b_work;


/* ------------------------------------------------------------
 * UUID Printing
 * ------------------------------------------------------------ */

static void print_uuid(const uint8_t *uuid)
{
    for (int i = 0; i < 16; i++) {
        printk("%02X", uuid[i]);
    }
}


/* ------------------------------------------------------------
 * UUID Matching
 * ------------------------------------------------------------ */

static bool uuid_matches(const uint8_t *uuid)
{
    for (int i = 0; i < 16; i++) {

        if (uuid[i] != target_uuid[i]) {
            return false;
        }
    }

    return true;
}


/* ------------------------------------------------------------
 * Provisioning: Unprovisioned Beacon
 * ------------------------------------------------------------ */

static void prov_unprovisioned_beacon(
    uint8_t uuid[16],
    bt_mesh_prov_oob_info_t oob_info,
    uint32_t *uri_hash)
{
    ARG_UNUSED(oob_info);
    ARG_UNUSED(uri_hash);

    printk("\n");
    printk("========================================\n");
    printk("UNPROVISIONED MESH BEACON\n");
    printk("========================================\n");

    printk("UUID : ");
    print_uuid(uuid);
    printk("\n");

    if (!uuid_matches(uuid)) {

        printk("Target : NOT AUTHORIZED\n");
        printk("Action : IGNORE\n");
        printk("========================================\n");

        return;
    }

    printk("Target : B nRF52833\n");
    printk("Action : UUID MATCHED\n");

    if (provisioning_started) {

        printk("Action : PROVISIONING ALREADY ACTIVE\n");
        printk("========================================\n");

        return;
    }

    printk("Action : START PB-ADV PROVISIONING\n");
    printk("NetIdx : 0x%04X\n", NET_IDX);
    printk("Addr   : AUTO\n");

    int err = bt_mesh_provision_adv(
        uuid,
        NET_IDX,
        0x0000,
        0
    );

    if (err) {

        printk(
            "ERROR: bt_mesh_provision_adv() failed: %d\n",
            err
        );

        printk("========================================\n");

        return;
    }

    provisioning_started = true;

    printk("PB-ADV provisioning procedure started\n");
    printk("========================================\n");
}


/* ------------------------------------------------------------
 * Provisioning Link Open
 * ------------------------------------------------------------ */

static void prov_link_open(bt_mesh_prov_bearer_t bearer)
{
    printk("\n");
    printk("========================================\n");
    printk("PB-ADV LINK OPENED\n");
    printk("Bearer : 0x%02X\n", bearer);
    printk("========================================\n");
}


/* ------------------------------------------------------------
 * Provisioning Link Close
 * ------------------------------------------------------------ */

static void prov_link_close(bt_mesh_prov_bearer_t bearer)
{
    printk("\n");
    printk("========================================\n");
    printk("PB-ADV LINK CLOSED\n");
    printk("Bearer : 0x%02X\n", bearer);
    printk("========================================\n");

    /*
     * The provisioning transaction has ended.
     *
     * If provisioning did not complete, allow the next
     * unprovisioned beacon to start a fresh attempt.
     */
    provisioning_started = false;
}


/* ------------------------------------------------------------
 * Local Provisioning Complete
 * ------------------------------------------------------------ */

static void prov_complete(
    uint16_t net_idx,
    uint16_t addr)
{
    printk("\n");
    printk("========================================\n");
    printk("LOCAL PROVISIONING COMPLETE\n");
    printk("NetIdx  : 0x%04X\n", net_idx);
    printk("Address : 0x%04X\n", addr);
    printk("========================================\n");
}


/* ------------------------------------------------------------
 * Provisioning Node Added
 * ------------------------------------------------------------ */

static void prov_node_added(
    uint16_t net_idx,
    uint8_t uuid[16],
    uint16_t addr,
    uint8_t num_elem)
{
    printk("\n");
    printk("========================================\n");
    printk("PROVISIONING NODE ADDED\n");
    printk("========================================\n");

    printk("NetIdx   : 0x%04X\n", net_idx);

    printk("UUID     : ");
    print_uuid(uuid);
    printk("\n");

    printk("Address  : 0x%04X\n", addr);
    printk("Elements : %u\n", num_elem);

    printk("========================================\n");

    /*
     * Save the actual address assigned to B.
     */
    provisioned_b_addr = addr;

    provisioning_started = false;

    printk(
        "Saved B address : 0x%04X\n",
        provisioned_b_addr
    );

    /*
     * Give B time to transition from provisioning
     * into normal Mesh operation.
     */
    printk("Configuration scheduled after 3 seconds\n");

    k_work_reschedule_for_queue(
        &config_workq,
        &configure_b_work,
        K_SECONDS(3)
    );
}


/* ------------------------------------------------------------
 * Mesh Reset Callback
 * ------------------------------------------------------------ */

static void prov_reset(void)
{
    printk("MESH RESET CALLBACK\n");
}


/* ------------------------------------------------------------
 * Provisioning Structure
 * ------------------------------------------------------------ */

static struct bt_mesh_prov prov = {

    .uuid = dev_uuid,

    .unprovisioned_beacon =
        prov_unprovisioned_beacon,

    .link_open =
        prov_link_open,

    .link_close =
        prov_link_close,

    .complete =
        prov_complete,

    .node_added =
        prov_node_added,

    .reset =
        prov_reset,
};


/* ------------------------------------------------------------
 * Send Vendor PING
 * ------------------------------------------------------------ */

static int send_vendor_ping(uint16_t dst)
{
    BT_MESH_MODEL_BUF_DEFINE(
        msg,
        VENDOR_OP_PING,
        0
    );

    bt_mesh_model_msg_init(
        &msg,
        VENDOR_OP_PING
    );

    struct bt_mesh_msg_ctx ctx = {
        .net_idx = NET_IDX,
        .app_idx = APP_IDX,
        .addr = dst,
        .send_ttl = BT_MESH_TTL_DEFAULT,
        .send_rel = true,
    };

    printk("\n");
    printk("========================================\n");
    printk("SENDING VENDOR PING\n");
    printk("========================================\n");

    printk("Destination : 0x%04X\n", dst);
    printk("NetIdx      : 0x%04X\n", NET_IDX);
    printk("AppIdx      : 0x%04X\n", APP_IDX);
    printk("Company ID  : 0x%04X\n", VENDOR_COMPANY_ID);
    printk("Model ID    : 0x%04X\n", VENDOR_MODEL_ID);
    printk("Opcode      : 0x%06X\n", VENDOR_OP_PING);

    int err = bt_mesh_model_send(
        &vnd_models[0],
        &ctx,
        &msg,
        NULL,
        NULL
    );

    printk("Send result : %d\n", err);
    printk("========================================\n");

    return err;
}


/* ------------------------------------------------------------
 * Configure B
 * ------------------------------------------------------------ */

static void configure_b_work_handler(struct k_work *work)
{
    ARG_UNUSED(work);

    uint8_t status = 0xFF;
    int err;

    const uint16_t b_addr = provisioned_b_addr;

    printk("\n");
    printk("========================================\n");
    printk("CONFIGURING B\n");
    printk("========================================\n");

    printk("B Address : 0x%04X\n", b_addr);

    if (b_addr == BT_MESH_ADDR_UNASSIGNED) {

        printk("ERROR: B address is unassigned\n");

        return;
    }


    /* --------------------------------------------------------
     * STEP 1
     *
     * Add Application Key locally to A
     * -------------------------------------------------------- */

    printk("\n");
    printk("STEP 1: ADD APPKEY LOCALLY TO A\n");

    uint8_t local_status = bt_mesh_app_key_add(
        APP_IDX,
        NET_IDX,
        app_key
    );

    printk(
        "Local AppKey result : 0x%02X\n",
        local_status
    );

    if (local_status != 0x00) {

        /*
         * If already present, continue.
         */
        if (local_status != 0x0B) {

            printk("ERROR: Local AppKey add failed\n");

            return;
        }

        printk("Local AppKey already exists\n");

    } else {

        printk("Local AppKey ready\n");
    }


    /* --------------------------------------------------------
     * STEP 2
     *
     * Synchronous Configuration Client AppKey Add
     * -------------------------------------------------------- */

    printk("\n");
    printk("STEP 2: ADD APPKEY TO B\n");
    printk("----------------------------------------\n");

    printk(
        "Target address : 0x%04X\n",
        b_addr
    );

    printk(
        "NetIdx         : 0x%04X\n",
        NET_IDX
    );

    printk(
        "AppIdx         : 0x%04X\n",
        APP_IDX
    );

    printk("Sending synchronous AppKey Add...\n");

    status = 0xFF;

    err = bt_mesh_cfg_cli_app_key_add(
        NET_IDX,
        b_addr,
        NET_IDX,
        APP_IDX,
        app_key,
        &status
    );

    printk("\n");

    printk(
        "SYNC AppKey Add transport result : %d\n",
        err
    );

    printk(
        "SYNC AppKey Add status            : 0x%02X\n",
        status
    );

    printk("----------------------------------------\n");

    if (err) {

        printk(
            "ERROR: AppKey Add transaction failed\n"
        );

        printk(
            "Expected transport result : 0\n"
        );

        printk("========================================\n");

        return;
    }

    if (status != 0x00) {

        printk(
            "ERROR: B returned non-success status\n"
        );

        printk(
            "Expected status : 0x00\n"
        );

        printk("========================================\n");

        return;
    }

    printk("SUCCESS: B accepted AppKey\n");
    printk("========================================\n");


    /* --------------------------------------------------------
     * STEP 3
     *
     * Bind AppKey to B Vendor Model
     * -------------------------------------------------------- */

    printk("\n");
    printk("STEP 3: BIND APPKEY TO B VENDOR MODEL\n");

    printk("----------------------------------------\n");

    printk("Target address : 0x%04X\n", b_addr);
    printk("AppIdx         : 0x%04X\n", APP_IDX);
    printk("Company ID     : 0x%04X\n",
           VENDOR_COMPANY_ID);
    printk("Model ID       : 0x%04X\n",
           VENDOR_MODEL_ID);

    status = 0xFF;

    err = bt_mesh_cfg_cli_mod_app_bind_vnd(
        NET_IDX,
        b_addr,
        b_addr,
        APP_IDX,
        VENDOR_MODEL_ID,
        VENDOR_COMPANY_ID,
        &status
    );

    printk(
        "Vendor Bind transport result : %d\n",
        err
    );

    printk(
        "Vendor Bind status            : 0x%02X\n",
        status
    );

    printk("----------------------------------------\n");

    if (err || status != 0x00) {

        printk(
            "ERROR: Vendor Model Bind failed\n"
        );

        printk("========================================\n");

        return;
    }

    printk("SUCCESS: B Vendor Model bound\n");

    printk("========================================\n");


    /* --------------------------------------------------------
     * STEP 4
     *
     * Bind AppKey to A Vendor Model
     * -------------------------------------------------------- */

    printk("\n");
    printk("STEP 4: BIND APPKEY TO A VENDOR MODEL\n");

    printk("----------------------------------------\n");

    printk("Target address : 0x%04X\n", A_ADDR);
    printk("AppIdx         : 0x%04X\n", APP_IDX);
    printk("Company ID     : 0x%04X\n",
           VENDOR_COMPANY_ID);
    printk("Model ID       : 0x%04X\n",
           VENDOR_MODEL_ID);

    status = 0xFF;

    err = bt_mesh_cfg_cli_mod_app_bind_vnd(
        NET_IDX,
        A_ADDR,
        A_ADDR,
        APP_IDX,
        VENDOR_MODEL_ID,
        VENDOR_COMPANY_ID,
        &status
    );

    printk(
        "A Vendor Bind transport result : %d\n",
        err
    );

    printk(
        "A Vendor Bind status            : 0x%02X\n",
        status
    );

    printk("----------------------------------------\n");

    if (err || status != 0x00) {

        printk(
            "ERROR: A Vendor Model Bind failed\n"
        );

        printk("========================================\n");

        return;
    }

    printk("SUCCESS: A Vendor Model bound\n");

    printk("========================================\n");


    /* --------------------------------------------------------
     * STEP 5
     *
     * Vendor PING
     * -------------------------------------------------------- */

    printk("\n");
    printk("STEP 5: SEND VENDOR PING\n");

    k_sleep(K_MSEC(500));

    err = send_vendor_ping(b_addr);

    if (err) {

        printk(
            "ERROR: Vendor PING send failed: %d\n",
            err
        );

        return;
    }

    printk("Vendor PING submitted successfully\n");

    printk("\n");
    printk("========================================\n");
    printk("VENDOR MODEL TEST COMPLETE\n");
    printk("========================================\n");
}


/* ------------------------------------------------------------
 * Bluetooth Ready
 * ------------------------------------------------------------ */

static void bt_ready(int err)
{
    if (err) {

        printk(
            "ERROR: Bluetooth initialization failed: %d\n",
            err
        );

        return;
    }

    printk("Bluetooth initialized successfully\n");

    printk("Initializing Bluetooth Mesh...\n");

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

    printk("Bluetooth Mesh initialized successfully\n");


    /* --------------------------------------------------------
     * Create Mesh CDB
     * -------------------------------------------------------- */

    printk("Creating Mesh CDB...\n");

    err = bt_mesh_cdb_create(net_key);

    if (err) {

        printk(
            "ERROR: bt_mesh_cdb_create() failed: %d\n",
            err
        );

        return;
    }

    printk("Mesh CDB created successfully\n");


    /* --------------------------------------------------------
     * Locally provision A
     * -------------------------------------------------------- */

    printk("\n");
    printk("Locally provisioning A...\n");

    err = bt_mesh_provision(
        net_key,
        NET_IDX,
        0x00,
        0x00000000,
        A_ADDR,
        a_dev_key
    );

    if (err) {

        printk(
            "ERROR: bt_mesh_provision() failed: %d\n",
            err
        );

        return;
    }

    printk("A local provisioning successful\n");
    printk("A address : 0x%04X\n", A_ADDR);
    printk("A NetIdx  : 0x%04X\n", NET_IDX);
    printk("A IVIndex : 0x00000000\n");


    /* --------------------------------------------------------
     * Start Provisioner
     * -------------------------------------------------------- */

    printk("\n");
    printk("========================================\n");
    printk("       A nRF5340 PROVISIONER\n");
    printk("========================================\n");

    printk("UUID       : ");
    print_uuid(dev_uuid);
    printk("\n");

    printk("Role       : Mesh Provisioner\n");
    printk("Address    : 0x%04X\n", A_ADDR);
    printk("NetIdx     : 0x%04X\n", NET_IDX);
    printk("CDB        : READY\n");
    printk("PB-ADV     : ENABLED\n");
    printk("Target     : B nRF52833\n");

    printk(
        "Vendor     : 0x%04X / 0x%04X\n",
        VENDOR_COMPANY_ID,
        VENDOR_MODEL_ID
    );

    printk("Config WQ  : DEDICATED\n");

    printk(
        "Config WQ Stack : %u bytes\n",
        CONFIG_WORKQ_STACK_SIZE
    );

    printk("Status     : WAITING FOR B\n");

    printk("========================================\n");
}


/* ------------------------------------------------------------
 * Main
 * ------------------------------------------------------------ */

int main(void)
{
    int err;

    printk("\n");
    printk("========================================\n");
    printk("       A nRF5340 PROVISIONER START\n");
    printk("========================================\n");

    printk("NCS v2.7.0\n");
    printk("Zephyr 3.6.99-ncs2\n");


    /*
     * Start dedicated Configuration Client workqueue.
     */
    printk("Starting configuration workqueue...\n");

    k_work_queue_start(
        &config_workq,
        config_workq_stack,
        K_THREAD_STACK_SIZEOF(config_workq_stack),
        CONFIG_WORKQ_PRIORITY,
        NULL
    );

    k_work_init_delayable(
        &configure_b_work,
        configure_b_work_handler
    );

    printk("Configuration workqueue started\n");

    printk("Starting Bluetooth...\n");

    err = bt_enable(bt_ready);

    if (err) {

        printk(
            "ERROR: bt_enable() failed: %d\n",
            err
        );

        return 0;
    }

    while (1) {

        printk("A Provisioner alive\n");

        k_sleep(K_SECONDS(5));
    }

    return 0;
}