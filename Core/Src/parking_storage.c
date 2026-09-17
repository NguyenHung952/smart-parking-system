#include "parking_storage.h"

#include "eeprom_24c16.h"
#include "main.h"
#include "parking_slot.h"

#define PARKING_STORAGE_MAGIC             0x504BU
#define PARKING_STORAGE_VERSION           1U
#define PARKING_STORAGE_RECORD_SIZE       16U
#define PARKING_STORAGE_RECORD_A_ADDRESS  0x0000U
#define PARKING_STORAGE_RECORD_B_ADDRESS  0x0010U
#define PARKING_STORAGE_RETRY_MS          1000U

typedef struct
{
    uint8_t slot_mask;
    uint32_t total_entry; /* Legacy field: so lan TRONG -> CO XE. */
    uint32_t total_exit;  /* Legacy field: so lan CO XE -> TRONG. */
    uint16_t sequence;
} ParkingStorageRecord_t;

static uint8_t storage_ready;
static uint8_t last_save_ok;
static uint8_t save_pending;
static uint8_t active_record;
static uint8_t last_slot_mask;
static uint8_t write_enabled;
static uint32_t total_entry;
static uint32_t total_exit;
static uint16_t current_sequence;
static uint32_t retry_tick;

static void parking_storage_init_internal(uint8_t synchronize_current_mask,
                                          uint8_t allow_write);
static uint8_t parking_storage_load_record(uint16_t address,
                                           ParkingStorageRecord_t *record);
static uint8_t parking_storage_save(void);
static void parking_storage_encode(const ParkingStorageRecord_t *record,
                                   uint8_t data[PARKING_STORAGE_RECORD_SIZE]);
static uint8_t parking_storage_decode(
    const uint8_t data[PARKING_STORAGE_RECORD_SIZE],
    ParkingStorageRecord_t *record);
static uint16_t parking_storage_crc16(const uint8_t *data, uint8_t length);
static uint8_t parking_storage_sequence_is_newer(uint16_t first,
                                                 uint16_t second);
static void parking_storage_write_u16(uint8_t *data, uint16_t value);
static void parking_storage_write_u32(uint8_t *data, uint32_t value);
static uint16_t parking_storage_read_u16(const uint8_t *data);
static uint32_t parking_storage_read_u32(const uint8_t *data);

void parking_storage_init(void)
{
    /* AUTO: doc record cu, dong bo mask cam bien hien tai va cho phep ghi. */
    parking_storage_init_internal(0U, 1U);
}

void parking_storage_init_read_only(void)
{
    /* TEST/DEMO: chi doc bo dem cu, tuyet doi khong sua du lieu EEPROM. */
    parking_storage_init_internal(0U, 0U);
}

void parking_storage_set_baseline_mask(uint8_t mask)
{
    last_slot_mask = (uint8_t)(mask & 0x0FU);
}

static void parking_storage_init_internal(uint8_t synchronize_current_mask,
                                          uint8_t allow_write)
{
    ParkingStorageRecord_t record_a;
    ParkingStorageRecord_t record_b;
    uint8_t record_a_valid;
    uint8_t record_b_valid;
    uint8_t current_mask;

    storage_ready = 0U;
    last_save_ok = 0U;
    save_pending = 0U;
    active_record = 1U;
    last_slot_mask = (uint8_t)(parking_slot_get_stable_mask() & 0x0FU);
    write_enabled = allow_write;
    total_entry = 0U;
    total_exit = 0U;
    current_sequence = 0U;
    retry_tick = HAL_GetTick();

    if(eeprom_24c16_is_ready() != EEPROM_24C16_OK)
    {
        return;
    }

    storage_ready = 1U;
    record_a_valid = parking_storage_load_record(
        PARKING_STORAGE_RECORD_A_ADDRESS,
        &record_a);
    record_b_valid = parking_storage_load_record(
        PARKING_STORAGE_RECORD_B_ADDRESS,
        &record_b);

    if((record_a_valid != 0U) && (record_b_valid != 0U))
    {
        if(parking_storage_sequence_is_newer(record_b.sequence,
                                             record_a.sequence) != 0U)
        {
            active_record = 1U;
            last_slot_mask = record_b.slot_mask;
            total_entry = record_b.total_entry;
            total_exit = record_b.total_exit;
            current_sequence = record_b.sequence;
        }
        else
        {
            active_record = 0U;
            last_slot_mask = record_a.slot_mask;
            total_entry = record_a.total_entry;
            total_exit = record_a.total_exit;
            current_sequence = record_a.sequence;
        }

        last_save_ok = 1U;
    }
    else if(record_a_valid != 0U)
    {
        active_record = 0U;
        last_slot_mask = record_a.slot_mask;
        total_entry = record_a.total_entry;
        total_exit = record_a.total_exit;
        current_sequence = record_a.sequence;
        last_save_ok = 1U;
    }
    else if(record_b_valid != 0U)
    {
        active_record = 1U;
        last_slot_mask = record_b.slot_mask;
        total_entry = record_b.total_entry;
        total_exit = record_b.total_exit;
        current_sequence = record_b.sequence;
        last_save_ok = 1U;
    }
    else if(write_enabled != 0U)
    {
        /* EEPROM co phan hoi nhung chua co record hop le: tao record dau tien. */
        save_pending = 1U;
    }
    else
    {
        /* Read-only va chua co record: xem nhu EEPROM san sang, bo dem = 0. */
        last_save_ok = 1U;
    }

    /*
     * Moi lan RESET trong AUTO bat dau phien moi:
     * xoa CHIEM/TRA va ghi lai record, nhung giu nguyen last_slot_mask.
     */
    if(write_enabled != 0U)
    {
        total_entry = 0U;
        total_exit = 0U;
        save_pending = 1U;
    }

    if(synchronize_current_mask != 0U)
    {
        current_mask = (uint8_t)(parking_slot_get_stable_mask() & 0x0FU);

        /* Boot khong duoc tinh thanh mot luot CHIEM/TRA O. Chi cap nhat baseline. */
        if(last_slot_mask != current_mask)
        {
            last_slot_mask = current_mask;
            save_pending = (write_enabled != 0U) ? 1U : 0U;
        }

        /*
         * RESET MCU = bat dau lai phien su dung.
         *
         * Theo yeu cau cua he thong hien tai, RESET phai dua lich su
         * CHIEM/TRẢ ve 0, nhung KHONG xoa trang thai 4 o trong EEPROM.
         * Nhu vay sau RESET, cac o dang co xe van duoc giu lam baseline,
         * con hai bo dem lich su duoc khoi tao lai tu 0.
         *
         * Quan trong: chi lam viec nay trong AUTO. TEST/DEMO dung
         * parking_storage_init_read_only(), nen khong ghi EEPROM.
         */
        if(write_enabled != 0U)
        {
            total_entry = 0U;
            total_exit = 0U;
            save_pending = 1U;
        }
    }

    if((save_pending != 0U) && (write_enabled != 0U))
    {
        last_save_ok = parking_storage_save();
        save_pending = (last_save_ok == 0U) ? 1U : 0U;
        retry_tick = HAL_GetTick();
    }
}

void parking_storage_update(void)
{
    uint32_t now = HAL_GetTick();

    if((storage_ready == 0U) || (write_enabled == 0U))
    {
        return;
    }

    /*
     * KHONG tu dong dem IN/OUT theo thay doi S1-S4.
     * Counter chi duoc tang boi parking_storage_record_entry()/exit()
     * sau khi Gate State Machine da xac nhan dung quy trinh.
     */
    if((save_pending != 0U) &&
       ((last_save_ok != 0U) ||
        ((uint32_t)(now - retry_tick) >= PARKING_STORAGE_RETRY_MS)))
    {
        last_save_ok = parking_storage_save();
        save_pending = (last_save_ok == 0U) ? 1U : 0U;
        retry_tick = now;
    }
}

void parking_storage_record_entry(uint8_t slot)
{
    (void)slot;

    if((storage_ready == 0U) || (write_enabled == 0U))
    {
        return;
    }

    total_entry++;
    last_slot_mask = (uint8_t)(parking_slot_get_stable_mask() & 0x0FU);
    save_pending = 1U;
}

void parking_storage_record_exit(uint8_t slot)
{
    (void)slot;

    if((storage_ready == 0U) || (write_enabled == 0U))
    {
        return;
    }

    total_exit++;
    last_slot_mask = (uint8_t)(parking_slot_get_stable_mask() & 0x0FU);
    save_pending = 1U;
}

uint8_t parking_storage_is_ready(void)
{
    return storage_ready;
}

uint8_t parking_storage_last_save_ok(void)
{
    return last_save_ok;
}

uint32_t parking_storage_get_slot_occupied_count(void)
{
    return total_entry;
}

uint32_t parking_storage_get_slot_released_count(void)
{
    return total_exit;
}

/* API tuong thich nguoc: ten cu, y nghia van la bien doi trang thai o do. */
uint32_t parking_storage_get_total_entry(void)
{
    return parking_storage_get_slot_occupied_count();
}

uint32_t parking_storage_get_total_exit(void)
{
    return parking_storage_get_slot_released_count();
}

uint8_t parking_storage_get_last_mask(void)
{
    return last_slot_mask;
}

static uint8_t parking_storage_load_record(uint16_t address,
                                           ParkingStorageRecord_t *record)
{
    uint8_t data[PARKING_STORAGE_RECORD_SIZE];

    if(record == 0)
    {
        return 0U;
    }

    if(eeprom_24c16_read(address,
                         data,
                         PARKING_STORAGE_RECORD_SIZE) != EEPROM_24C16_OK)
    {
        return 0U;
    }

    return parking_storage_decode(data, record);
}

static uint8_t parking_storage_save(void)
{
    ParkingStorageRecord_t record;
    ParkingStorageRecord_t verification;
    uint8_t data[PARKING_STORAGE_RECORD_SIZE];
    uint8_t next_record;
    uint16_t address;

    if((storage_ready == 0U) || (write_enabled == 0U))
    {
        return 0U;
    }

    next_record = (active_record == 0U) ? 1U : 0U;
    address = (next_record == 0U) ?
              PARKING_STORAGE_RECORD_A_ADDRESS :
              PARKING_STORAGE_RECORD_B_ADDRESS;

    record.slot_mask = last_slot_mask;
    record.total_entry = total_entry;
    record.total_exit = total_exit;
    record.sequence = (uint16_t)(current_sequence + 1U);
    parking_storage_encode(&record, data);

    if(eeprom_24c16_write(address,
                          data,
                          PARKING_STORAGE_RECORD_SIZE) != EEPROM_24C16_OK)
    {
        return 0U;
    }

    if(parking_storage_load_record(address, &verification) == 0U)
    {
        return 0U;
    }

    if((verification.slot_mask != record.slot_mask) ||
       (verification.total_entry != record.total_entry) ||
       (verification.total_exit != record.total_exit) ||
       (verification.sequence != record.sequence))
    {
        return 0U;
    }

    active_record = next_record;
    current_sequence = record.sequence;
    return 1U;
}

static void parking_storage_encode(const ParkingStorageRecord_t *record,
                                   uint8_t data[PARKING_STORAGE_RECORD_SIZE])
{
    uint16_t crc;

    parking_storage_write_u16(&data[0], PARKING_STORAGE_MAGIC);
    data[2] = PARKING_STORAGE_VERSION;
    data[3] = (uint8_t)(record->slot_mask & 0x0FU);
    parking_storage_write_u32(&data[4], record->total_entry);
    parking_storage_write_u32(&data[8], record->total_exit);
    parking_storage_write_u16(&data[12], record->sequence);
    crc = parking_storage_crc16(data, 14U);
    parking_storage_write_u16(&data[14], crc);
}

static uint8_t parking_storage_decode(
    const uint8_t data[PARKING_STORAGE_RECORD_SIZE],
    ParkingStorageRecord_t *record)
{
    uint16_t stored_crc;
    uint16_t calculated_crc;

    if((data == 0) || (record == 0))
    {
        return 0U;
    }

    if((parking_storage_read_u16(&data[0]) != PARKING_STORAGE_MAGIC) ||
       (data[2] != PARKING_STORAGE_VERSION) ||
       ((data[3] & 0xF0U) != 0U))
    {
        return 0U;
    }

    stored_crc = parking_storage_read_u16(&data[14]);
    calculated_crc = parking_storage_crc16(data, 14U);

    if(stored_crc != calculated_crc)
    {
        return 0U;
    }

    record->slot_mask = data[3];
    record->total_entry = parking_storage_read_u32(&data[4]);
    record->total_exit = parking_storage_read_u32(&data[8]);
    record->sequence = parking_storage_read_u16(&data[12]);
    return 1U;
}

static uint16_t parking_storage_crc16(const uint8_t *data, uint8_t length)
{
    uint16_t crc = 0xFFFFU;
    uint8_t index;

    for(index = 0U; index < length; index++)
    {
        uint8_t bit;

        crc ^= (uint16_t)((uint16_t)data[index] << 8U);

        for(bit = 0U; bit < 8U; bit++)
        {
            crc = ((crc & 0x8000U) != 0U) ?
                  (uint16_t)((crc << 1U) ^ 0x1021U) :
                  (uint16_t)(crc << 1U);
        }
    }

    return crc;
}

static uint8_t parking_storage_sequence_is_newer(uint16_t first,
                                                 uint16_t second)
{
    return ((int16_t)(first - second) > 0) ? 1U : 0U;
}

static void parking_storage_write_u16(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8U);
}

static void parking_storage_write_u32(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8U);
    data[2] = (uint8_t)(value >> 16U);
    data[3] = (uint8_t)(value >> 24U);
}

static uint16_t parking_storage_read_u16(const uint8_t *data)
{
    return (uint16_t)((uint16_t)data[0] |
                      ((uint16_t)data[1] << 8U));
}

static uint32_t parking_storage_read_u32(const uint8_t *data)
{
    return ((uint32_t)data[0]) |
           ((uint32_t)data[1] << 8U) |
           ((uint32_t)data[2] << 16U) |
           ((uint32_t)data[3] << 24U);
}
