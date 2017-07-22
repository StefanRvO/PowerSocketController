#pragma once
//This class handles the communication and setup of a single CS5463 Energy IC.
//The class will not be threadsafe, don't use functionallity from it from multiple threads simultaneously.
#include "esp_system.h"
#include "driver/gpio.h"
#include <cstdint>
#include "driver/spi_master.h"

enum calibration_type
{
    i_dc_offset     = 0b01001,
    i_dc_gain       = 0b01010,
    i_ac_offset     = 0b01101,
    i_ac_gain       = 0b01110,
    u_dc_offset     = 0b10001,
    u_dc_gain       = 0b10010,
    u_ac_offset     = 0b10101,
    u_ac_gain       = 0b10110,
    u_i_dc_offset   = 0b10001,
    u_i_dc_gain     = 0b10010,
    u_i_ac_offset   = 0b10101,
    u_i_ac_gain     = 0b10110,
};

enum registers : uint8_t
{
    config = 0,
    i_dcoff,
    i_gn,
    v_dcoff,
    v_gn,
    cycle_count,
    pulserateE,
    i_instant,
    v_instant,
    p_instant,
    p_active_instant,
    i_rms,
    v_rms,
    epsilon,
    p_off,
    status,
    i_acoff,
    v_acoff,
    mode,
    temperature,
    q_avg,
    q_instant,
    i_peak,
    v_peak,
    q_trig,
    pf,
    mask,
    apparent_power,
    ctrl,
    p_harmonic,
    p_fundemental,
    q_fundemental,
};

struct status_register
{
    bool data_ready;
    bool conversion_ready;
    bool current_out_range;
    bool voltage_out_range;
    bool irms_out_range;
    bool vrms_out_range;
    bool energy_out_range;
    bool current_fault;
    bool voltage_sag;
    bool temp_update;
    bool temp_modu_oscilation;
    bool voltage_modu_oscilation;
    bool current_modu_oscilation;
    bool low_supply_detected;
    bool epsilon_update;
    bool invalid_command;
};

struct mode_register
{
    bool e2mode;
    bool xvdel;
    bool xidel;
    bool ihpf;
    bool vhpf;
    bool iir;
    uint8_t e3mode;
    bool pos;
    bool afc;
};

struct config_register
{
    uint8_t phase_compensation;
    bool i_gain;
    bool ewa;
    bool int_mode;
    bool int_inv;
    bool inv_cpu;
    uint8_t clock_div;
};

struct ctrl_register
{
    bool stop;
    bool int_opendrain;
    bool no_cpu;
    bool no_osc;
};


class CS5463
{
    public:
        CS5463(gpio_num_t slave_select);
        static void init_spi(gpio_num_t miso, gpio_num_t mosi, gpio_num_t clk);
        int start_conversions(bool single_shot = false);
        int software_reset();
        int read_register(registers the_register, uint8_t *databuff); //Databuff needs to be able to hold 3 bytes.
        int write_register(registers the_register, uint8_t *data_out, uint8_t *data_in = nullptr, uint8_t len = 3);
        int do_spi_transaction(uint8_t cmd, uint8_t len = 0, uint8_t *data_out = 0, uint8_t *data_in = 0);
        int perform_calibration(calibration_type type);
        int power_up_halt();
        int halt_to_standby();
        int halt_to_sleep();
        int read_status_register(status_register *data);
        int read_temperature(float *temp);
        int set_operation_mode(mode_register reg);
        int set_computation_cycle_duration(uint32_t milliseconds);
        int get_epsilon(float *e);
        int set_epsilon(float e);
        uint32_t owr = 4000;
    private:
        spi_device_handle_t spi;
        static bool initialised;
        static xSemaphoreHandle spi_mux;
};
