/*
 * Race Capture Firmware
 *
 * Copyright (C) 2016 Autosport Labs
 *
 * This file is part of the Race Capture firmware suite
 *
 * This is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details. You should
 * have received a copy of the GNU General Public License along with
 * this code. If not, see <http://www.gnu.org/licenses/>.
 */

#include "sampleRecord_test.h"

#include "ADC.h"
#include "ADC_mock.h"
#include "FreeRTOS.h"
#include "GPIO.h"
#include "capabilities.h"
#include "gps.h"
#include "imu.h"
#include "imu_gsum.h"
#include "lap_stats.h"
#include "loggerConfig.h"
#include "loggerHardware.h"
#include "loggerSampleData.test.h"
#include "mock_serial.h"
#include "predictive_timer_2.h"
#include "sampleRecord.h"
#include "task.h"
#include "task_testing.h"

#include <string>
#include <stdio.h>

using std::string;

// Registers the fixture into the 'registry'
CPPUNIT_TEST_SUITE_REGISTRATION( SampleRecordTest );

LoggerConfig *lc;
struct sample s;

void SampleRecordTest::setUp()
{
        InitLoggerHardware();
        setupMockSerial();
        GPS_init(10, getMockSerial());
        initialize_logger_config();
        reset_ticks();

        lc = getWorkingLoggerConfig();
        lapstats_reset(false);
        size_t channelCount = get_enabled_channel_count(lc);
        init_sample_buffer(&s, channelCount);

}


void SampleRecordTest::tearDown()
{
        free_sample_buffer(&s);
}


void SampleRecordTest::testPopulateSampleRecord()
{
        //mock up some values to test later
        lc->ADCConfigs[7].scalingMode = SCALING_MODE_RAW;
        ADC_mock_set_value(7, 123);
        ADC_sample_all();

        lapstats_reset(false);
        // Set it so we have 1 tick.
        increment_tick();
        CPPUNIT_ASSERT_EQUAL(1, (int) (xTaskGetTickCount()));

        populate_sample_buffer(&s, 0);

        const ChannelSample *samples = s.channel_samples;

        // Interval Channel
        CPPUNIT_ASSERT_EQUAL((int) (xTaskGetTickCount() * MS_PER_TICK),
                             samples->valueInt);

        // UTC Channel.  Just test that its 0 for now
        samples++;
        CPPUNIT_ASSERT_EQUAL(0ll, (long long) getMillisSinceEpoch());
        CPPUNIT_ASSERT_EQUAL(0ll, samples->valueLongLong);

        //ElapsedTime
        samples++;
        //5 milliseconds in minutes. We incremented 1 tick earlier;
        //test is configured for 5ms / tick
        CPPUNIT_ASSERT_EQUAL((float) (5/60000.0), samples->valueFloat);

        //analog channel
        samples++;
        CPPUNIT_ASSERT_EQUAL(123 * 0.0048828125f, samples->valueFloat);

        //accelerometer channels
        samples++;
        CPPUNIT_ASSERT_EQUAL(imu_read_value(IMU_CHANNEL_X, &lc->ImuConfigs[0]),
                             samples->valueFloat);

        samples++;
        CPPUNIT_ASSERT_EQUAL(imu_read_value(IMU_CHANNEL_Y, &lc->ImuConfigs[1]),
                             samples->valueFloat);

        samples++;
        CPPUNIT_ASSERT_EQUAL(imu_read_value(IMU_CHANNEL_Z, &lc->ImuConfigs[2]),
                             samples->valueFloat);

        samples++;
        CPPUNIT_ASSERT_EQUAL(imu_read_value(IMU_CHANNEL_YAW, &lc->ImuConfigs[3]),
                             samples->valueFloat);

        samples++;
        CPPUNIT_ASSERT_EQUAL(imu_read_value(IMU_CHANNEL_PITCH, &lc->ImuConfigs[4]),
                             samples->valueFloat);

        samples++;
        CPPUNIT_ASSERT_EQUAL(imu_read_value(IMU_CHANNEL_ROLL, &lc->ImuConfigs[5]),
                             samples->valueFloat);

        samples++;
        CPPUNIT_ASSERT_EQUAL( 1.0f, samples->valueFloat); //IMU Gsum channel

        samples++;
        CPPUNIT_ASSERT_EQUAL( 1.0f, samples->valueFloat); //IMU Gsum_max channel

        samples++;
        CPPUNIT_ASSERT_EQUAL( 100.0f, samples->valueFloat); //IMU Gsum_pct channel

        //GPS / Track channels
        /*
         * !!! BE WARNED!!!  It seems some of these samples should be valueInts instead of
         * valueFloat.  If you are going to change it to a non-zero value, you will
         * need to double check this.  GDB was the quickest way I found to debug this.
         * Else you will enter a world of pain as I did trying to figure out why you
         * get NaN or something else weird that you didn't expect.
         */
        samples++;
        CPPUNIT_ASSERT_EQUAL((float) 0, samples->valueFloat); //Latitude

        samples++;
        CPPUNIT_ASSERT_EQUAL((float) 0, samples->valueFloat); //Longtiude

        samples++;
        CPPUNIT_ASSERT_EQUAL((float) 0, samples->valueFloat); //Speed

        samples++;
        CPPUNIT_ASSERT_EQUAL((float) 0, samples->valueFloat); //Altitude

        samples++;
        CPPUNIT_ASSERT_EQUAL((float) 0, samples->valueFloat); //GPSSats

        samples++;
        CPPUNIT_ASSERT_EQUAL((float) 0, samples->valueFloat); //GPSQual

        samples++;
        CPPUNIT_ASSERT_EQUAL((float) 0, samples->valueFloat); //GPSDOP

        samples++;
        CPPUNIT_ASSERT_EQUAL((float) 0, samples->valueFloat); //lapCount

        samples++;
        CPPUNIT_ASSERT_EQUAL((float) 0, samples->valueFloat); //lapTime

        samples++;
        CPPUNIT_ASSERT_EQUAL((int) -1, samples->valueInt); //sectorCfg

        samples++;
        CPPUNIT_ASSERT_EQUAL((float) 0, samples->valueFloat); //SectorTime

        samples++;
        CPPUNIT_ASSERT_EQUAL((float) 0, samples->valueFloat); //PredTime

        samples++;
        CPPUNIT_ASSERT_EQUAL((int) 0, samples->valueInt); //CurrentLap

        samples++;
        CPPUNIT_ASSERT_EQUAL((float) 0, samples->valueFloat); //Distance
}

void SampleRecordTest::testInitSampleRecord()
{
        LoggerConfig *lc = getWorkingLoggerConfig();

        const size_t expectedEnabledChannels = 28;
        size_t channelCount = get_enabled_channel_count(lc);
        CPPUNIT_ASSERT_EQUAL(expectedEnabledChannels, channelCount);

        ChannelSample *ts = s.channel_samples;
        const struct TimeConfig *tc = lc->TimeConfigs;

        // Check what should be Uptime (Interval)
        CPPUNIT_ASSERT_EQUAL(string("Interval"), string(tc->cfg.label));
        CPPUNIT_ASSERT_EQUAL(string("ms"), string(tc->cfg.units));
        CPPUNIT_ASSERT_EQUAL(TimeType_Uptime, tc->tt);
        CPPUNIT_ASSERT_EQUAL(SampleData_Int_Noarg, ts->sampleData);
        ++ts;
        ++tc;

        // Check what should be the Utc
        CPPUNIT_ASSERT_EQUAL(string("Utc"), string(tc->cfg.label));
        CPPUNIT_ASSERT_EQUAL(string("ms"), string(tc->cfg.units));
        CPPUNIT_ASSERT_EQUAL(TimeType_UtcMillis, tc->tt);
        CPPUNIT_ASSERT_EQUAL(SampleData_LongLong_Noarg, ts->sampleData);
        ++ts;

        // Elapsed Time should be present on every sample
        LapConfig *lapConfig = &(lc->LapConfigs);
        CPPUNIT_ASSERT_EQUAL((void *) &lapConfig->elapsed_time_cfg, (void *) ts->cfg);
        CPPUNIT_ASSERT_EQUAL(SampleData_Float_Noarg, ts->sampleData);
        CPPUNIT_ASSERT_EQUAL((void *) lapstats_elapsed_time_minutes,(void *) ts->get_float_sample);
        ts++;

        for (int i = 0; i < CONFIG_ADC_CHANNELS; i++) {
                ADCConfig *ac = &lc->ADCConfigs[i];
                if (ac->cfg.sampleRate == SAMPLE_DISABLED)
                        continue;

                CPPUNIT_ASSERT_EQUAL((uint8_t) i, ts->channelIndex);
                CPPUNIT_ASSERT_EQUAL((void *) &ac->cfg, (void *) ts->cfg);
                CPPUNIT_ASSERT_EQUAL((void *) get_analog_sample,
                                     (void *) ts->get_float_sample);
                CPPUNIT_ASSERT_EQUAL(SampleData_Float, ts->sampleData);
                ts++;
        }

        for (int i = 0; i < CONFIG_IMU_CHANNELS; i++) {
                ImuConfig *ac = &lc->ImuConfigs[i];
                if (ac->cfg.sampleRate == SAMPLE_DISABLED)
                        continue;

                CPPUNIT_ASSERT_EQUAL((uint8_t)i,ts->channelIndex);
                CPPUNIT_ASSERT_EQUAL((void *) &ac->cfg, (void *) ts->cfg);
                CPPUNIT_ASSERT_EQUAL((void *) get_imu_sample,
                                     (void *) ts->get_float_sample);
                CPPUNIT_ASSERT_EQUAL(SampleData_Float, ts->sampleData);
                ts++;
        }

        /* Check what should be IMU Gsum channel */
        CPPUNIT_ASSERT_EQUAL(&lc->imu_gsum, ts->cfg);
        CPPUNIT_ASSERT_EQUAL(SampleData_Float_Noarg, ts->sampleData);
        ++ts;

        /* Check what should be IMU Gsum_max channel */
        CPPUNIT_ASSERT_EQUAL(&lc->imu_gsummax, ts->cfg);
        CPPUNIT_ASSERT_EQUAL(SampleData_Float_Noarg, ts->sampleData);
        ++ts;

        /* Check what should be IMU Gsum_Pct channel */
        CPPUNIT_ASSERT_EQUAL(&lc->imu_gsumpct, ts->cfg);
        CPPUNIT_ASSERT_EQUAL(SampleData_Float_Noarg, ts->sampleData);
        ++ts;

        for (int i = 0; i < CONFIG_TIMER_CHANNELS; i++) {
                TimerConfig *tc = &lc->TimerConfigs[i];
                if (tc->cfg.sampleRate == SAMPLE_DISABLED)
                        continue;

                CPPUNIT_ASSERT_EQUAL((uint8_t)i, ts->channelIndex);
                CPPUNIT_ASSERT_EQUAL((void *) &tc->cfg, (void *) ts->cfg);
                CPPUNIT_ASSERT_EQUAL((void *) timer_get_sample,
                                     (void *) ts->get_float_sample);
                CPPUNIT_ASSERT_EQUAL(SampleData_Float, ts->sampleData);
                ts++;
        }

        for (int i = 0; i < CONFIG_GPIO_CHANNELS; i++) {
                GPIOConfig *gc = &lc->GPIOConfigs[i];
                if (gc->cfg.sampleRate == SAMPLE_DISABLED)
                        continue;

                CPPUNIT_ASSERT_EQUAL((uint8_t)i, ts->channelIndex);
                CPPUNIT_ASSERT_EQUAL((void *) &gc->cfg, (void *) ts->cfg);
                CPPUNIT_ASSERT_EQUAL((void *) GPIO_get,
                                     (void *) ts->get_int_sample);
                CPPUNIT_ASSERT_EQUAL(SampleData_Int, ts->sampleData);
                ts++;
        }

        for (int i = 0; i < CONFIG_PWM_CHANNELS; i++) {
                PWMConfig *pc = &lc->PWMConfigs[i];
                if (pc->cfg.sampleRate == SAMPLE_DISABLED)
                        continue;

                CPPUNIT_ASSERT_EQUAL((uint8_t)i, ts->channelIndex);
                CPPUNIT_ASSERT_EQUAL((void *) &pc->cfg, (void *) ts->cfg);
                CPPUNIT_ASSERT_EQUAL((void *) get_pwm_sample,
                                     (void *) ts->get_int_sample);
                CPPUNIT_ASSERT_EQUAL(SampleData_Int, ts->sampleData);
                ts++;
        }


        GPSConfig *gpsConfig = &(lc->GPSConfigs);
        if (gpsConfig->latitude.sampleRate != SAMPLE_DISABLED) {
                CPPUNIT_ASSERT_EQUAL((void *) &gpsConfig->latitude,
                                     (void *) ts->cfg);
                CPPUNIT_ASSERT_EQUAL((void *) GPS_getLatitude,
                                     (void *) ts->get_float_sample);
                CPPUNIT_ASSERT_EQUAL(SampleData_Float_Noarg, ts->sampleData);
                ts++;
        }

        if (gpsConfig->longitude.sampleRate != SAMPLE_DISABLED) {
                CPPUNIT_ASSERT_EQUAL((void *) &gpsConfig->longitude,
                                     (void *) ts->cfg);
                CPPUNIT_ASSERT_EQUAL((void *) GPS_getLongitude,
                                     (void *) ts->get_float_sample);
                CPPUNIT_ASSERT_EQUAL(SampleData_Float_Noarg, ts->sampleData);
                ts++;
        }

        if (gpsConfig->speed.sampleRate != SAMPLE_DISABLED) {
                CPPUNIT_ASSERT_EQUAL((void *) &gpsConfig->speed,
                                     (void *) ts->cfg);
                CPPUNIT_ASSERT_EQUAL((void *) getGpsSpeedInMph,
                                     (void *) ts->get_float_sample);
                CPPUNIT_ASSERT_EQUAL(SampleData_Float_Noarg, ts->sampleData);
                ts++;
        }

        if (gpsConfig->altitude.sampleRate != SAMPLE_DISABLED) {
                CPPUNIT_ASSERT_EQUAL((void *) &gpsConfig->altitude,
                                     (void *) ts->cfg);
                CPPUNIT_ASSERT_EQUAL((void *) getAltitude,
                                     (void *) ts->get_float_sample);
                CPPUNIT_ASSERT_EQUAL(SampleData_Float_Noarg, ts->sampleData);
                ts++;
        }

        if (gpsConfig->satellites.sampleRate != SAMPLE_DISABLED) {
                CPPUNIT_ASSERT_EQUAL((void *) &gpsConfig->satellites,
                                     (void *) ts->cfg);
                CPPUNIT_ASSERT_EQUAL((void *) GPS_getSatellitesUsedForPosition,
                                     (void *) ts->get_int_sample);
                CPPUNIT_ASSERT_EQUAL(SampleData_Int_Noarg, ts->sampleData);
                ts++;
        }

        if (gpsConfig->quality.sampleRate != SAMPLE_DISABLED) {
                CPPUNIT_ASSERT_EQUAL((void *) &gpsConfig->quality,
                                     (void *) ts->cfg);
                CPPUNIT_ASSERT_EQUAL((void *) GPS_getQuality,
                                     (void *) ts->get_int_sample);
                CPPUNIT_ASSERT_EQUAL(SampleData_Int_Noarg, ts->sampleData);
                ts++;
        }

        if (gpsConfig->DOP.sampleRate != SAMPLE_DISABLED) {
                CPPUNIT_ASSERT_EQUAL((void *) &gpsConfig->DOP,
                                     (void *) ts->cfg);
                CPPUNIT_ASSERT_EQUAL((void *) GPS_getDOP,
                                     (void *) ts->get_float_sample);
                CPPUNIT_ASSERT_EQUAL(SampleData_Float_Noarg, ts->sampleData);
                ts++;
        }

        if (lapConfig->lapCountCfg.sampleRate != SAMPLE_DISABLED) {
                CPPUNIT_ASSERT_EQUAL((void *) &lapConfig->lapCountCfg,
                                     (void *) ts->cfg);
                CPPUNIT_ASSERT_EQUAL(SampleData_Int_Noarg, ts->sampleData);
                CPPUNIT_ASSERT_EQUAL((void *) getLapCount,
                                     (void *) ts->get_int_sample);
                ts++;
        }

        if (lapConfig->lapTimeCfg.sampleRate != SAMPLE_DISABLED) {
                CPPUNIT_ASSERT_EQUAL((void *) &lapConfig->lapTimeCfg,
                                     (void *) ts->cfg);
                CPPUNIT_ASSERT_EQUAL(SampleData_Float_Noarg, ts->sampleData);
                CPPUNIT_ASSERT_EQUAL((void *) getLastLapTimeInMinutes,
                                     (void *) ts->get_float_sample);
                ts++;
        }

        if (lapConfig->sectorCfg.sampleRate != SAMPLE_DISABLED) {
                CPPUNIT_ASSERT_EQUAL((void *) &lapConfig->sectorCfg,
                                     (void *) ts->cfg);
                CPPUNIT_ASSERT_EQUAL(SampleData_Int_Noarg, ts->sampleData);
                CPPUNIT_ASSERT_EQUAL((void *) getSector,
                                     (void *) ts->get_int_sample);
                ts++;
        }

        if (lapConfig->sectorTimeCfg.sampleRate != SAMPLE_DISABLED) {
                CPPUNIT_ASSERT_EQUAL((void *) &lapConfig->sectorTimeCfg,
                                     (void *) ts->cfg);
                CPPUNIT_ASSERT_EQUAL(SampleData_Float_Noarg, ts->sampleData);
                CPPUNIT_ASSERT_EQUAL((void *) getLastSectorTimeInMinutes,
                                     (void *) ts->get_float_sample);
                ts++;
        }

        if (lapConfig->predTimeCfg.sampleRate != SAMPLE_DISABLED) {
                CPPUNIT_ASSERT_EQUAL((void *) &lapConfig->predTimeCfg,
                                     (void *) ts->cfg);
                CPPUNIT_ASSERT_EQUAL(SampleData_Float_Noarg, ts->sampleData);
                CPPUNIT_ASSERT_EQUAL((void *) getPredictedTimeInMinutes,
                                     (void *) ts->get_float_sample);
                ts++;
        }

        if (lapConfig->current_lap_cfg.sampleRate != SAMPLE_DISABLED) {
                CPPUNIT_ASSERT_EQUAL((void *) &lapConfig->current_lap_cfg,
                                     (void *) ts->cfg);
                CPPUNIT_ASSERT_EQUAL(SampleData_Int_Noarg, ts->sampleData);
                CPPUNIT_ASSERT_EQUAL((void *) lapstats_current_lap,
                                     (void *) ts->get_int_sample);
                ts++;
        }

        if (lapConfig->distance.sampleRate != SAMPLE_DISABLED) {
                CPPUNIT_ASSERT_EQUAL((void *) &lapConfig->distance,
                                     (void *) ts->cfg);
                CPPUNIT_ASSERT_EQUAL((void *) getLapDistanceInMiles,
                                     (void *) ts->get_float_sample);
                CPPUNIT_ASSERT_EQUAL(SampleData_Float_Noarg, ts->sampleData);
                ts++;
        }

        if (lapConfig->session_time_cfg.sampleRate != SAMPLE_DISABLED) {
                CPPUNIT_ASSERT_EQUAL((void *) &lapConfig->session_time_cfg,
                                     (void *) ts->cfg);
                CPPUNIT_ASSERT_EQUAL(SampleData_Float_Noarg, ts->sampleData);
                CPPUNIT_ASSERT_EQUAL((void *) lapstats_session_time_minutes,
                                     (void *) ts->get_float_sample);
                ts++;
        }

        //amount shoud match
        const size_t size = ts - s.channel_samples;
        CPPUNIT_ASSERT_EQUAL(expectedEnabledChannels, size);
}


void SampleRecordTest::testIsValidLoggerMessage()
{
        LoggerMessage lm;
        struct sample s;

        /* Test the non sample case.  This always passes */
        lm = create_logger_message(LoggerMessageType_Start, 0, NULL, false);
        lm.ticks = 42;
        s.ticks = 42;
        CPPUNIT_ASSERT_EQUAL(true, is_sample_data_valid(&lm));
        lm.ticks = 41;
        CPPUNIT_ASSERT_EQUAL(true,  is_sample_data_valid(&lm));

        /* Test the sample case. */
        lm = create_logger_message(LoggerMessageType_Sample, 0, &s, false);
        lm.ticks = 42;
        s.ticks = 42;
        CPPUNIT_ASSERT_EQUAL(true, is_sample_data_valid(&lm));
        lm.ticks = 41;
        CPPUNIT_ASSERT_EQUAL(false,  is_sample_data_valid(&lm));

}

void SampleRecordTest::testLoggerMessageAlwaysHasTime()
{
        /*
         * Check that we always populate the Interval time, regardless of
         * what the sample rate is that is set for that channel.
         */
        int var = 0;
        int tick = 0;
        while (var < 5 && tick <= 1000) {
                int sr = populate_sample_buffer(&s, tick++);

                // No sample, no check.
                if (SAMPLE_DISABLED == sr)
                        continue;

                ++var;
                CPPUNIT_ASSERT_EQUAL(true, s.channel_samples->populated);
        }

        CPPUNIT_ASSERT_EQUAL(true, tick < 1000);
}

void SampleRecordTest::test_get_sample_value_by_name()
{
        lc->ADCConfigs[7].scalingMode = SCALING_MODE_RAW;
        ADC_mock_set_value(7, 123);
        ADC_sample_all();

        increment_tick();
        CPPUNIT_ASSERT_EQUAL(1, (int) (xTaskGetTickCount()));

        populate_sample_buffer(&s, 0);


        double value;
        char *units;
        bool result = get_sample_value_by_name(&s, "Speed", &value, &units);
        CPPUNIT_ASSERT_EQUAL(true, result);
        CPPUNIT_ASSERT_EQUAL((double)0, value);
        CPPUNIT_ASSERT_EQUAL(string("mph"), string(units));

        result = get_sample_value_by_name(&s, "Battery", &value, &units);
        CPPUNIT_ASSERT_EQUAL(true, result);
        CPPUNIT_ASSERT_EQUAL((double)123 * 0.0048828125f, value);
        CPPUNIT_ASSERT_EQUAL(string("Volts"), string(units));

        result = get_sample_value_by_name(&s, "FooBar", &value, &units);
        CPPUNIT_ASSERT_EQUAL(false, result);
}
