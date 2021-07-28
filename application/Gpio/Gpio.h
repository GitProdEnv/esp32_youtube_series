#pragma once

#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "esp_intr_alloc.h"

#include <cassert>
#include <cstring>
#include <array>
#include <string_view>
#include <utility>

namespace Gpio
{

class GpioBase
{
protected:
    const gpio_num_t _pin;
    const bool _inverted_logic = false;
    const gpio_config_t _cfg;

public:
    constexpr GpioBase(const gpio_num_t pin,
                        const gpio_config_t& config, 
                        const bool invert_logic = false) noexcept :
        _pin{pin},
        _inverted_logic{invert_logic},
        _cfg{gpio_config_t{
                        .pin_bit_mask   = static_cast<uint64_t>(1) << _pin,
                        .mode           = config.mode,
                        .pull_up_en     = config.pull_up_en,
                        .pull_down_en   = config.pull_down_en,
                        .intr_type      = config.intr_type
            }}
    {
        assert(ArduinoPinMap::is_pin(pin));

        assert(!(ArduinoPinMap::is_input_only(pin) && 
                    (_cfg.pull_up_en == GPIO_PULLUP_ENABLE || _cfg.pull_up_en == GPIO_PULLUP_ENABLE)));
    }

    constexpr GpioBase(const std::string_view& arduino_pin_name,
                        const gpio_config_t& config, 
                        const bool invert_logic = false) noexcept :
        GpioBase{ArduinoPinMap::at(arduino_pin_name), config, invert_logic}
    {}

    ~GpioBase(void) noexcept
    {
        gpio_reset_pin(_pin);
    }

    [[nodiscard]] virtual bool state(void) const =0;

    [[nodiscard]] operator bool() const { return state(); }
    
    [[nodiscard]] esp_err_t init(void)
    {
        return gpio_config(&_cfg);
    }

    // https://www.az-delivery.de/en/blogs/azdelivery-blog-fur-arduino-und-raspberry-pi/das-24-und-letzte-turchen
    // https://www.electroschematics.com/arduino-uno-pinout/
    class ArduinoPinMap
    {
        using ArduinoPinMap_t = std::pair<const std::string_view, const gpio_num_t>;

#if CONFIG_IDF_TARGET_ESP32
        constexpr static std::array<ArduinoPinMap_t, 23> arduino_pins = 
        {{
            {"D0",  GPIO_NUM_3},
            {"D1",  GPIO_NUM_1},
            {"D2",  GPIO_NUM_26},
            {"D3",  GPIO_NUM_25},
            {"D4",  GPIO_NUM_17},
            {"D5",  GPIO_NUM_16},
            {"D6",  GPIO_NUM_27},
            {"D7",  GPIO_NUM_14},
            {"D8",  GPIO_NUM_12},
            {"D9",  GPIO_NUM_13},
            {"D10", GPIO_NUM_5},
            {"D11", GPIO_NUM_23},
            {"D12", GPIO_NUM_19},
            {"D13", GPIO_NUM_18},
            {"A0",  GPIO_NUM_39},
            {"A1",  GPIO_NUM_36},
            {"A2",  GPIO_NUM_34},
            {"A3",  GPIO_NUM_35},
            {"A4",  GPIO_NUM_4},
            {"A5",  GPIO_NUM_2},
            {"SDA", GPIO_NUM_21},
            {"SCL", GPIO_NUM_22},
            {"OD",  GPIO_NUM_0}
        }};

        constexpr static std::array<gpio_num_t, 8> adc1_pins = 
        {{
            GPIO_NUM_32,
            GPIO_NUM_33,
            GPIO_NUM_34,
            GPIO_NUM_35,
            GPIO_NUM_36,
            GPIO_NUM_37,
            GPIO_NUM_38,
            GPIO_NUM_39
        }};

        constexpr static std::array<gpio_num_t, 6> adc2_pins = 
        {{
            //GPIO_NUM_0, // Strapping
            //GPIO_NUM_2, // Strapping
            //GPIO_NUM_4, // ESP-WROVER-KIT pin
            GPIO_NUM_12,
            GPIO_NUM_13,
            GPIO_NUM_14,
            //GPIO_NUM_15, // Strapping
            GPIO_NUM_25,
            GPIO_NUM_26,
            GPIO_NUM_27
        }};

        constexpr static std::array<gpio_num_t, 38> interrupt_pins = 
        {{
            GPIO_NUM_0,
            GPIO_NUM_1,
            GPIO_NUM_2,
            GPIO_NUM_3,
            GPIO_NUM_4,
            GPIO_NUM_5,
            GPIO_NUM_6,
            GPIO_NUM_7,
            GPIO_NUM_8,
            GPIO_NUM_9,
            GPIO_NUM_10,
            GPIO_NUM_11,
            GPIO_NUM_12,
            GPIO_NUM_13,
            GPIO_NUM_14,
            GPIO_NUM_15,
            GPIO_NUM_16,
            GPIO_NUM_17,
            GPIO_NUM_18,
            GPIO_NUM_19,
            GPIO_NUM_20,
            GPIO_NUM_21,
            GPIO_NUM_22,
            GPIO_NUM_23,
            GPIO_NUM_25,
            GPIO_NUM_26,
            GPIO_NUM_27,
            GPIO_NUM_28,
            GPIO_NUM_29,
            GPIO_NUM_30,
            GPIO_NUM_31,
            GPIO_NUM_32,
            GPIO_NUM_33,
            GPIO_NUM_34,
            GPIO_NUM_35,
            //GPIO_NUM_36, // Clash with WiFi
            GPIO_NUM_37,
            GPIO_NUM_38//,
            //GPIO_NUM_39 // Clash with WiFi
        }};
#else
#error "This code is written for ESP32 only."
#endif

    public:
        constexpr static gpio_num_t at(const std::string_view& arduino_pin_name) noexcept
        {
            for (const auto& iter : arduino_pins)
                if (iter.first == arduino_pin_name) 
                    return iter.second;

            return GPIO_NUM_NC;
        }

        constexpr static bool is_pin(const gpio_num_t pin) noexcept
        {
            return (GPIO_NUM_NC < pin && GPIO_NUM_MAX > pin) && GPIO_IS_VALID_GPIO(pin); // TODO probably don't need both the macro and our search
        }

        constexpr static bool is_pin(const std::string_view& arduino_pin_name) noexcept
        {
            return is_pin(at(arduino_pin_name));
        }

        constexpr static bool is_input(const gpio_num_t pin) noexcept
        {
            return is_pin(pin);
        }

        constexpr static bool is_input(const std::string_view& arduino_pin_name) noexcept
        {
            return is_input(at(arduino_pin_name));
        }

        constexpr static bool is_output(const gpio_num_t pin) noexcept
        {
            return is_pin(pin) &&
                    (GPIO_NUM_NC < pin && GPIO_NUM_MAX > pin && (GPIO_NUM_34 > pin || GPIO_NUM_39 < pin)) &&
                    GPIO_IS_VALID_OUTPUT_GPIO(pin); // TODO probably don't need both the macro and our search
        }

        constexpr static bool is_output(const std::string_view& arduino_pin_name) noexcept
        {
            return is_output(at(arduino_pin_name));
        }

        constexpr static bool is_input_and_output(const gpio_num_t pin) noexcept
        {
            return is_input(pin) && is_output(pin);
        }

        constexpr static bool is_input_and_output(const std::string_view& arduino_pin_name) noexcept
        {
            return is_input_and_output(at(arduino_pin_name));
        }

        constexpr static bool is_input_only(const gpio_num_t pin) noexcept
        {
            return is_input(pin) && !is_output(pin);
        }

        constexpr static bool is_input_only(const std::string_view& arduino_pin_name) noexcept
        {
            return is_input_only(at(arduino_pin_name));
        }

        constexpr static bool is_analogue(const gpio_num_t pin) noexcept
        {
            if (is_pin(pin))
            {
                for (const gpio_num_t avail : adc1_pins)
                    if (pin == avail) 
                        return true;
                for (const gpio_num_t avail : adc2_pins)
                    if (pin == avail) 
                        return true;
            }

            return false;
        }

        constexpr static bool is_analogue(const std::string_view& arduino_pin_name) noexcept
        {
            return is_analogue(at(arduino_pin_name));
        }

        constexpr static bool is_interrupt(const gpio_num_t pin) noexcept
        {
            if (is_pin(pin))
                for (const gpio_num_t avail : interrupt_pins)
                    if (pin == avail) 
                        return true;

            return false;
        }

        constexpr static bool is_interrupt(const std::string_view& arduino_pin_name) noexcept
        {
            return is_interrupt(at(arduino_pin_name));
        }

        constexpr static bool is_adc1(const gpio_num_t pin) noexcept
        {
            if (is_pin(pin))
            {
                for (const gpio_num_t avail : adc1_pins)
                    if (pin == avail) 
                        return true;
            }

            return false;
        }

        constexpr static bool is_adc1(const std::string_view& arduino_pin_name) noexcept
        {
            return is_adc1(at(arduino_pin_name));
        }

        constexpr static bool is_adc2(const gpio_num_t pin) noexcept
        {
            if (is_pin(pin))
            {
                for (const gpio_num_t avail : adc2_pins)
                    if (pin == avail) 
                        return true;
            }

            return false;
        }

        constexpr static bool is_adc2(const std::string_view& arduino_pin_name) noexcept
        {
            return is_adc2(at(arduino_pin_name));
        }

        ArduinoPinMap(void) = delete; // Not constructable, compile time only
    };    

}; // GpioBase

class GpioOutput : public GpioBase
{
    bool _state = false; // map the users wish

protected:
    constexpr GpioOutput(const gpio_num_t pin, 
                            const gpio_config_t& cfg, 
                            const bool invert) noexcept :
        GpioBase{pin, cfg, invert}
    { assert(ArduinoPinMap::is_output(pin)); }

    constexpr GpioOutput(const std::string_view& arduino_pin_name, 
                            const gpio_config_t& cfg, 
                            const bool invert) noexcept :
        GpioOutput{ArduinoPinMap::at(arduino_pin_name), cfg, invert}
    {}

public:
    constexpr GpioOutput(const gpio_num_t pin, const bool invert = false) noexcept :
        GpioOutput{pin, 
                    gpio_config_t{
                        .pin_bit_mask   = static_cast<uint64_t>(1) << pin,
                        .mode           = GPIO_MODE_OUTPUT,     // TODO I & O, open drain
                        .pull_up_en     = GPIO_PULLUP_DISABLE,  // TODO
                        .pull_down_en   = GPIO_PULLDOWN_ENABLE, // TODO
                        .intr_type      = GPIO_INTR_DISABLE
                    }, 
                    invert}
    {}

    constexpr GpioOutput(const std::string_view& arduino_pin_name, const bool invert = false) noexcept :
        GpioOutput{ArduinoPinMap::at(arduino_pin_name), invert}
    {}

    [[nodiscard]] esp_err_t init(void)
    {
        esp_err_t status = GpioBase::init();
        if (ESP_OK == status) status |= set(false);
        return status;
    }

    esp_err_t set(const bool state)
    {
        const bool      state_to_set = _inverted_logic ? !state : state;
        const esp_err_t status       = gpio_set_level(_pin, state_to_set);
        if (ESP_OK == status) _state = state_to_set;

        return status;
    }

    [[nodiscard]] bool state(void) const 
        { return _inverted_logic ? !_state : _state; }
};

class GpioInput : public GpioBase
{
protected:
    constexpr GpioInput(const gpio_num_t pin, 
                        const gpio_config_t& cfg, 
                        const bool invert = false) noexcept :
        GpioBase{pin, cfg, invert}
    { assert(ArduinoPinMap::is_input(pin)); }

    constexpr GpioInput(const std::string_view& arduino_pin_name, 
                        const gpio_config_t& cfg, 
                        const bool invert = false) noexcept :
        GpioInput{ArduinoPinMap::at(arduino_pin_name), cfg, invert}
    {}

public:
    constexpr GpioInput(const gpio_num_t pin, const bool invert = false) noexcept :
        GpioInput{pin, 
                    gpio_config_t{
                        .pin_bit_mask   = static_cast<uint64_t>(1) << pin,
                        .mode           = GPIO_MODE_INPUT,
                        .pull_up_en     = GPIO_PULLUP_DISABLE,
                        .pull_down_en   = GPIO_PULLDOWN_ENABLE,
                        .intr_type      = GPIO_INTR_DISABLE
                    }, 
                    invert}
    {}

    constexpr GpioInput(const std::string_view& arduino_pin_name, 
                        const bool invert = false) noexcept :
        GpioInput{ArduinoPinMap::at(arduino_pin_name), invert}
    {}

    [[nodiscard]] bool get(void) const
    {
        const bool pin_state = gpio_get_level(_pin) == 0 ? false : true;
        return _inverted_logic ? !pin_state : pin_state;
    }

    [[nodiscard]] bool state(void) const 
        { return get(); }
};

class GpioInterrupt : public GpioInput
{
    constexpr static esp_err_t isr_service_state_default{ESP_ERR_INVALID_STATE};
    static esp_err_t isr_service_state;

public:
    constexpr GpioInterrupt(const gpio_num_t pin, 
                            const gpio_int_type_t interrupt_type) noexcept :
        GpioInput{pin, 
                    gpio_config_t{
                        .pin_bit_mask   = static_cast<uint64_t>(1) << pin,
                        .mode           = GPIO_MODE_INPUT,
                        .pull_up_en     = GPIO_PULLUP_DISABLE,
                        .pull_down_en   = GPIO_PULLDOWN_DISABLE,
                        .intr_type      = interrupt_type
                    }}
    { assert(ArduinoPinMap::is_interrupt(pin)); }

    constexpr GpioInterrupt(const std::string_view& arduino_pin_name, 
                            const gpio_int_type_t interrupt_type) noexcept :
        GpioInterrupt{ArduinoPinMap::at(arduino_pin_name), interrupt_type}
    {}

    ~GpioInterrupt(void) noexcept
    {
        gpio_intr_disable(_pin);
        gpio_isr_handler_remove(_pin);
    }

    [[nodiscard]] esp_err_t init(gpio_isr_t isr_callback)
    {
        esp_err_t status = GpioInput::init();

        if (ESP_OK == status && isr_service_state_default == isr_service_state)
        {
            const int flags = ESP_INTR_FLAG_LOWMED | 
                (_cfg.intr_type == GPIO_INTR_POSEDGE ||
                    _cfg.intr_type == GPIO_INTR_NEGEDGE ||
                    _cfg.intr_type == GPIO_INTR_ANYEDGE
                        ? ESP_INTR_FLAG_EDGE : 0);
            isr_service_state = gpio_install_isr_service(flags);
            status |= isr_service_state;
        }

        if (ESP_OK == status)
        {
            status |= gpio_isr_handler_add(_pin, isr_callback, this);
        }

        return status;
    }
};

inline esp_err_t GpioInterrupt::isr_service_state{isr_service_state_default};

class GpioAnalogueInput : public GpioInput
{
    constexpr static adc_bits_width_t  width_default{ADC_WIDTH_BIT_12};
    constexpr static adc_atten_t       atten_default{ADC_ATTEN_DB_0};
    constexpr static uint32_t          n_samples_default{10};
    static bool two_point_supported, vref_supported;

    enum class Adc_num_t
    {
        ADC_1,
        ADC_2,
        ADC_MAX
    } const adc_num{Adc_num_t::ADC_MAX};

    /*const*/ adc_channel_t     channel;
    /*const*/ adc_bits_width_t  width{width_default};
    /*const*/ adc_atten_t       atten{atten_default};
    /*const*/ adc_unit_t        unit{ADC_UNIT_1}; // ESP32 only supports unit 1

    /*const*/ adc1_channel_t    adc1_channel; // TODO std::variant instead???
    /*const*/ adc2_channel_t    adc2_channel; // TODO std::variant instead???

    esp_adc_cal_characteristics_t adc_chars{};

    uint32_t vref{1100};

public:
    constexpr GpioAnalogueInput(const gpio_num_t pin,
                                const adc_bits_width_t width,
                                const adc_atten_t attenuation) noexcept :
        GpioInput{pin, 
                    gpio_config_t{
                        .pin_bit_mask   = static_cast<uint64_t>(1) << pin,
                        .mode           = GPIO_MODE_INPUT,
                        .pull_up_en     = GPIO_PULLUP_DISABLE,
                        .pull_down_en   = GPIO_PULLDOWN_DISABLE,
                        .intr_type      = GPIO_INTR_DISABLE
                    }},
        adc_num{pin_to_adc_num(pin)},
        channel{pin_to_adc_channel(pin)},
        width{width},
        atten{attenuation},
        adc1_channel{pin_to_adc1_channel(pin)},
        adc2_channel{pin_to_adc2_channel(pin)}
    { 
        assert(ArduinoPinMap::is_analogue(pin));
        assert(Adc_num_t::ADC_MAX != adc_num);
        assert(ADC_CHANNEL_MAX > channel);
        assert(ADC_WIDTH_MAX > width);
        assert(ADC_ATTEN_MAX > atten);
        switch (adc_num)
        {
        case Adc_num_t::ADC_1:
            assert(ADC1_CHANNEL_MAX > adc1_channel);
            break;
        case Adc_num_t::ADC_2:
            assert(ADC2_CHANNEL_MAX > adc2_channel);
            break;
        default:
            assert(false);
        };
    }

    constexpr GpioAnalogueInput(const gpio_num_t pin) noexcept :
        GpioAnalogueInput{pin, width_default, atten_default}
    {}

    explicit constexpr GpioAnalogueInput(const gpio_num_t pin,
                                            const adc_bits_width_t width) noexcept :
        GpioAnalogueInput{pin, width, atten_default}
    {}

    explicit constexpr GpioAnalogueInput(const gpio_num_t pin,
                                            const adc_atten_t attenuation) noexcept :
        GpioAnalogueInput{pin, width_default, atten}
    {}

    constexpr GpioAnalogueInput(const std::string_view& arduino_pin_name,
                                const adc_bits_width_t width,
                                const adc_atten_t attenuation) noexcept :
        GpioAnalogueInput{ArduinoPinMap::at(arduino_pin_name), width, attenuation}
    {}

    [[nodiscard]] esp_err_t init(void)
    {
        check_efuse();

        esp_err_t status{ESP_OK};
        
        switch (adc_num)
        {
        case Adc_num_t::ADC_1:
            if (ESP_OK == status)
                status |= adc1_config_width(width);
            
            if (ESP_OK == status)
                status |= adc1_config_channel_atten(adc1_channel, atten);
            break;
        case Adc_num_t::ADC_2:
            if (ESP_OK == status)
                status |= adc2_config_channel_atten(adc2_channel, atten);
            break;
        default:
            status = ESP_FAIL;
            break;
        };

        const esp_adc_cal_value_t 
        val_type = esp_adc_cal_characterize(unit, atten, width, vref, &adc_chars); // TODO vref

        (void)val_type;

        return status;
    }

    [[nodiscard]] uint32_t get(const uint32_t n_samples = n_samples_default) const
    {
        esp_err_t   status{ESP_OK}; 
        size_t      n_fails = 0;
        int         cumulative_result = 0;

        switch (adc_num)
        {
        case Adc_num_t::ADC_1:
            for (uint32_t i = 0 ; i < n_samples ; ++i)
            {
                cumulative_result += adc1_get_raw(adc1_channel);
            }
            break;
        case Adc_num_t::ADC_2:
        {
            int temp_result = 0;
            for (uint32_t i = 0 ; i < n_samples ; ++i)
            {
                status = adc2_get_raw(adc2_channel, width, &temp_result);
                if (ESP_OK == status) cumulative_result += temp_result;
                else ++n_fails;
            }
            break;
        }
        default:
            n_fails = n_samples;
            status = ESP_FAIL;
            break;
        };

        const int n_samples_read = n_samples - n_fails;
        
        if (0 < n_samples_read)
        {
            int mean_result = cumulative_result / n_samples_read;
            return esp_adc_cal_raw_to_voltage(mean_result, &adc_chars);
        }

        return 0;
    }

    [[nodiscard]] bool state(void) const 
        { return get() > vref; }

private:
    static void check_efuse(void) noexcept
    {
        if (ESP_OK == esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP)) two_point_supported = true;
        else two_point_supported = false;

        if (ESP_OK == esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_VREF)) vref_supported = true;
        else vref_supported = false;
    }

    static constexpr adc1_channel_t pin_to_adc1_channel(const gpio_num_t pin) noexcept
    {
        switch (pin)
        {
        case GPIO_NUM_36:
            return ADC1_CHANNEL_0;
        case GPIO_NUM_37:
            return ADC1_CHANNEL_1;
        case GPIO_NUM_38:
            return ADC1_CHANNEL_2;
        case GPIO_NUM_39:
            return ADC1_CHANNEL_3;
        case GPIO_NUM_32:
            return ADC1_CHANNEL_4;
        case GPIO_NUM_33:
            return ADC1_CHANNEL_5;
        case GPIO_NUM_34:
            return ADC1_CHANNEL_6;
        case GPIO_NUM_35:
            return ADC1_CHANNEL_7;
        default:
            return ADC1_CHANNEL_MAX;
        };
    }

    static constexpr adc2_channel_t pin_to_adc2_channel(const gpio_num_t pin) noexcept
    {
        switch (pin)
        {
        case GPIO_NUM_4:
            return ADC2_CHANNEL_0;
        case GPIO_NUM_0:
            return ADC2_CHANNEL_1;
        case GPIO_NUM_2:
            return ADC2_CHANNEL_2;
        case GPIO_NUM_15:
            return ADC2_CHANNEL_3;
        case GPIO_NUM_13:
            return ADC2_CHANNEL_4;
        case GPIO_NUM_12:
            return ADC2_CHANNEL_5;
        case GPIO_NUM_14:
            return ADC2_CHANNEL_6;
        case GPIO_NUM_27:
            return ADC2_CHANNEL_7;
        case GPIO_NUM_25:
            return ADC2_CHANNEL_8;
        case GPIO_NUM_26:
            return ADC2_CHANNEL_9;
        default:
            return ADC2_CHANNEL_MAX;
        };
    }

    static constexpr Adc_num_t pin_to_adc_num(const gpio_num_t pin) noexcept
    {
        const adc1_channel_t adc1_ch = pin_to_adc1_channel(pin);
        const adc2_channel_t adc2_ch = pin_to_adc2_channel(pin);

        if ((ADC1_CHANNEL_MAX != adc1_ch && ADC2_CHANNEL_MAX != adc2_ch) &&
            !(ADC1_CHANNEL_MAX == adc1_ch && ADC2_CHANNEL_MAX == adc2_ch))
        {
            // Not both valid channels and not bot invalid channels (effectively logical XOR)
            if (pin == ArduinoPinMap::is_adc1(pin) && ADC1_CHANNEL_MAX != adc1_ch)
                return Adc_num_t::ADC_1;
            else if (pin == ArduinoPinMap::is_adc2(pin) && ADC2_CHANNEL_MAX != adc2_ch)
                return Adc_num_t::ADC_2;
            // NOTE If we get here something is wrong with the logic
        }
        return Adc_num_t::ADC_MAX;
    }

    static constexpr adc_channel_t pin_to_adc_channel(const gpio_num_t pin) noexcept
    {
        switch (pin_to_adc_num(pin))
        {
        case Adc_num_t::ADC_1:
            return static_cast<adc_channel_t>(pin_to_adc1_channel(pin));
        case Adc_num_t::ADC_2:
            return static_cast<adc_channel_t>(pin_to_adc2_channel(pin));
        case Adc_num_t::ADC_MAX:
            return ADC_CHANNEL_MAX;
        };
    }
};

inline bool GpioAnalogueInput::two_point_supported{false}, GpioAnalogueInput::vref_supported{false};

//class GpioAnalogueOutput : public GpioOutput;

} // namespace GPIO