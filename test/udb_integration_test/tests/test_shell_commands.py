#!/usr/bin/env python
from udb_test_helper import UDBTestResources, ContextTest
from unittest import TestCase, skipUnless
from udb_serial_device import UDBSerialTestDevice, OopsError
from typing import Generator
import time
import re

class ShellCommandTest(ContextTest):
    udb_serial: UDBSerialTestDevice

    def context(self) -> Generator:
        with UDBSerialTestDevice() as self.udb_serial:
            yield

    def test_help(self) -> None:
        output = self.udb_serial.command("help")
        self.assertRegex(output, "show available commands")

    def test_gpio(self) -> None:
        output = self.udb_serial.command("gpio read E 9")
        self.assertRegex(output, "GPIO port E pin 9 is 0")
        output = self.udb_serial.command("gpio pp_set E 9")
        self.assertRegex(output, "GPIO port E pin 9 is 1")
        output = self.udb_serial.command("gpio read E 9")
        self.assertRegex(output, "GPIO port E pin 9 is 1")
        output = self.udb_serial.command("gpio pp_clear E 9")
        self.assertRegex(output, "GPIO port E pin 9 is 0")
        output = self.udb_serial.command("gpio read E 9")
        self.assertRegex(output, "GPIO port E pin 9 is 0")

    def test_pwm(self) -> None:
        self.udb_serial.command("pwm start 50 60")
        self.udb_serial.command("pwm stop")

    def test_version(self) -> None:
        output = self.udb_serial.command("version")
        self.assertRegex(output, "Interface ver: udb_(.*)_hw:([0-9]*)\r\nBootloader " \
                                 "ver: (.*)\r\n\r\nDONE 0\r")

    def test_adapter_type(self) -> None:
        self.udb_serial.command("adapter_type")

    def test_i2c_probe(self) -> None:
        output = self.udb_serial.command("i2c probe 2")
        self.assertRegex(output, "probing...\r\n0x17\r\n0x50\r\n0x51\r\n0x52\r\n" \
                                            "0x53\r\n0x54\r\n0x55\r\n0x56\r\n0x57")

    def test_measure_power(self) -> None:
        output = self.udb_serial.command("measure_power")
        result = re.search("Target: Mainboard USB\r\n\tvoltage: ([0-9]*) mV\r\n\tcurrent: ([0-9]*) uA\r\n", output)
        if result != None:
            # needs this assert otherwise typing complains cause result is of type
            # Optional[Match]
            assert result is not None
            self.assertLess(int(result.group(1)), 5150, "Voltage is unexpectedly large")
            self.assertGreater(int(result.group(1)), 4850, "Voltage is unexpectedly small")
            self.assertLess(int(result.group(2)), 150000, "Current is unexpectedly large")
            self.assertGreater(int(result.group(2)), 113000, "Current is unexpectedly small")
        else:
            self.assertTrue(False, "Can't find expected output")

    def test_uptime(self) -> None:
        test_seconds = 10

        output = self.udb_serial.command("uptime")
        result = re.search("([0-9]*) mins ([0-9]*) secs\r\n", output)
        prev_secs = int(result.group(1)) * 60 + int(result.group(2))

        time.sleep(test_seconds)

        output = self.udb_serial.command("uptime")
        result = re.search("([0-9]*) mins ([0-9]*) secs\r\n", output)
        secs = int(result.group(1)) * 60 + int(result.group(2))

        # secs may wrap around in seconds
        self.assertEqual((prev_secs + test_seconds) % 3600, secs, "uptime is not accurate")

    def test_ext_relay(self) -> None:
        self.udb_serial.command("ext_relay on")
        output = self.udb_serial.command("ext_relay status")
        self.assertRegex(output, "external relay is on")
        self.udb_serial.command("ext_relay off")
        output = self.udb_serial.command("ext_relay status")
        self.assertRegex(output, "external relay is off")

class ShellCommandWithResetTest(TestCase):
    def test_reset(self) -> None:
        with UDBSerialTestDevice() as udb_serial:
            try:
                output = udb_serial.command("reset")
                self.assertTrue(False, f"Expected UDB to reset, but it didn't. Serial " \
                                       f"output: {output}")
            except OSError:
                pass
        with UDBSerialTestDevice() as udb_serial:
            self.assertLess(udb_serial.get_time_to_open(),
                            UDBTestResources.get_expected_boot_timedelta(),
                            msg="Regression in boot time")

    @skipUnless(UDBTestResources.should_run_all_tests(),
                "this test runs only with the --run-all flag and you have to diconnect your " \
                "debugger from UDB otherwise the assert will halt UDB")
    def test_watchdog(self) -> None:
        with UDBSerialTestDevice() as udb_serial:
            try:
                output = udb_serial.command("fault test_watchdog")
                time.sleep(10)
                self.assertTrue(False, f"Expected UDB to reset by watchdog, but it didn't. Serial " \
                                       f"output: {output}")
            except OSError:
                pass
        with UDBSerialTestDevice() as udb_serial:
            self.assertLess(udb_serial.get_time_to_open(),
                            UDBTestResources.get_expected_boot_timedelta(),
                            msg="Regression in boot time")

    @skipUnless(UDBTestResources.should_run_all_tests(),
                "this test runs only with the --run-all flag and you have to diconnect your " \
                "debugger from UDB otherwise the assert will halt UDB")
    def test_assert(self) -> None:
        with UDBSerialTestDevice() as udb_serial:
            try:
                output = udb_serial.command("fault test_assert")
                self.assertTrue(False, "Expected UDB to reset, but it didn't. Please make sure " \
                                       "there is no debugger connected to UDB, because then the " \
                                       "assert will cause UDB to halt! Serial output: " \
                                       f"{output}")
            except (OopsError, OSError):
                pass
        with UDBSerialTestDevice() as udb_serial:
            self.assertLess(udb_serial.get_time_to_open(),
                            UDBTestResources.get_expected_boot_timedelta(),
                            msg="Regression in boot time")
