#include "buzzer.h"
#include "board.h"

// 全局单例定义
Buzzer Buzz;

void Buzzer::begin()
{
    pinMode(PIN_BEEP, OUTPUT);
    digitalWrite(PIN_BEEP, LOW);
}

void Buzzer::beep(uint16_t ms)
{
    _remainMs = ms;
}

void Buzzer::tickMicros()
{
    // 半周期翻转 (2.7kHz -> ~185us)
    static const uint32_t HALF_US = 1000000UL / BEEP_FREQ_HZ / 2;

    if (_remainMs > 0)
    {
        uint32_t now = micros();
        if (now - _lastToggleUs >= HALF_US)
        {
            _lastToggleUs = now;
            _out = !_out;
            digitalWrite(PIN_BEEP, _out ? HIGH : LOW);
        }
        // 用 millis 做粗计时, 与翻转互不干扰
        static uint32_t startMs = 0;
        if (_remainMs > 0 && startMs == 0)
            startMs = millis();
        if (startMs != 0)
        {
            if ((int32_t)(millis() - startMs) >= _remainMs)
            {
                _remainMs = 0;
                startMs = 0;
                _out = false;
                digitalWrite(PIN_BEEP, LOW);
            }
        }
    }
}
