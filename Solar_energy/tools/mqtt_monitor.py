# -*- coding: utf-8 -*-
"""
Solar Device Status MQTT Monitor
=================================
- Subscribe to solar/status/# topic
- Real-time display of all slave device status
- Record communication stability for each device
- Auto-generate CSV log files

Usage:
  1. pip install paho-mqtt
  2. Edit MQTT_BROKER below
  3. python mqtt_monitor.py
"""

import paho.mqtt.client as mqtt
import json
import time
import os
import csv
from datetime import datetime
from collections import OrderedDict

# ================== Configuration ==================
MQTT_BROKER    = "broker.emqx.io"
MQTT_PORT      = 1883
MQTT_TOPIC     = "solar/status/#"
MQTT_USERNAME  = ""
MQTT_PASSWORD  = ""
LOG_DIR        = "logs"
# ====================================================


class DeviceStats:
    def __init__(self, device_id):
        self.device_id = device_id
        self.total_count = 0
        self.success_count = 0
        self.fail_count = 0
        self.last_seen = None
        self.last_status = None
        self.last_ok = None
        self.consecutive_fail = 0
        self.min_voltage = 9999
        self.max_voltage = 0
        self.min_temp = 999
        self.max_temp = -999

    @property
    def success_rate(self):
        if self.total_count == 0:
            return 0.0
        return (self.success_count / self.total_count) * 100.0

    def update(self, payload, timestamp):
        self.total_count += 1
        self.last_seen = timestamp
        ok = payload.get("ok", 0)
        self.last_ok = ok
        self.last_status = payload

        if ok == 1:
            self.success_count += 1
            self.consecutive_fail = 0
            temp = payload.get("t", 0)
            voltage = payload.get("v", 0)
            if temp < self.min_temp:
                self.min_temp = temp
            if temp > self.max_temp:
                self.max_temp = temp
            if voltage < self.min_voltage:
                self.min_voltage = voltage
            if voltage > self.max_voltage:
                self.max_voltage = voltage
        else:
            self.fail_count += 1
            self.consecutive_fail += 1


class SolarMonitor:
    def __init__(self):
        self.devices = OrderedDict()
        self.start_time = datetime.now()

        if not os.path.exists(LOG_DIR):
            os.makedirs(LOG_DIR)

        date_str = self.start_time.strftime("%Y%m%d_%H%M%S")
        self.csv_path = os.path.join(LOG_DIR, "solar_log_%s.csv" % date_str)
        self.csv_file = open(self.csv_path, "w", newline="", encoding="utf-8-sig")
        self.csv_writer = csv.writer(self.csv_file)
        self.csv_writer.writerow([
            "Time", "DeviceID", "Temp(C)", "Voltage(V)",
            "DC_Heating", "Power_Reverse", "Success", "ErrorCode"
        ])
        self.csv_file.flush()

        self.client = mqtt.Client(
            callback_api_version=mqtt.CallbackAPIVersion.VERSION2,
            client_id="solar_monitor_%d" % int(time.time())
        )
        if MQTT_USERNAME:
            self.client.username_pw_set(MQTT_USERNAME, MQTT_PASSWORD)
        self.client.on_connect = self._on_connect
        self.client.on_message = self._on_message

    def _on_connect(self, client, userdata, flags, reason_code, properties):
        if reason_code == 0:
            print("[Connected] %s:%d" % (MQTT_BROKER, MQTT_PORT))
            client.subscribe(MQTT_TOPIC)
            print("[Subscribed] %s" % MQTT_TOPIC)
        else:
            print("[Connect Failed] code: %d" % reason_code)

    def _on_message(self, client, userdata, msg):
        try:
            payload = json.loads(msg.payload.decode("utf-8"))
        except (json.JSONDecodeError, UnicodeDecodeError) as e:
            print("[Parse Error] %s: %s" % (msg.topic, e))
            return

        device_id = msg.topic.split("/")[-1] if "/" in msg.topic else msg.topic
        now = datetime.now()
        timestamp = now.strftime("%Y-%m-%d %H:%M:%S")

        if device_id not in self.devices:
            self.devices[device_id] = DeviceStats(device_id)
        self.devices[device_id].update(payload, now)

        ok = payload.get("ok", 0)
        err = payload.get("err", "")
        self.csv_writer.writerow([
            timestamp, device_id,
            payload.get("t", ""), payload.get("v", ""),
            "Yes" if payload.get("dc") else "No",
            "Yes" if payload.get("pr") else "No",
            "Yes" if ok else "No",
            err
        ])
        self.csv_file.flush()

        self._print_status(device_id, payload, timestamp)

    def _print_status(self, device_id, payload, timestamp):
        ok = payload.get("ok", 0)
        if ok == 1:
            t = payload.get("t", 0)
            v = payload.get("v", 0)
            dc = "Yes" if payload.get("dc") else "No"
            pr = "Yes" if payload.get("pr") else "No"
            print("[%s] OK  %s  Temp=%3dC  Volt=%5dV  DC_Heat=%3s  PwrRev=%3s" % (
                timestamp, device_id, t, v, dc, pr))
        else:
            err = payload.get("err", "?")
            print("[%s] FAIL %s  err=%s" % (timestamp, device_id, err))

    def print_summary(self):
        print("")
        print("=" * 90)
        print("  Communication Stability Summary  |  Uptime: %s" % (
            datetime.now() - self.start_time))
        print("=" * 90)
        print("%-16s %6s %6s %6s %8s %8s %12s %14s" % (
            "DeviceID", "Total", "OK", "Fail",
            "Rate", "ConsFail", "TempRange", "VoltRange"))
        print("-" * 90)

        for dev_id, stats in self.devices.items():
            if stats.success_count > 0:
                temp_range = "%d~%dC" % (stats.min_temp, stats.max_temp)
                volt_range = "%d~%dV" % (stats.min_voltage, stats.max_voltage)
            else:
                temp_range = "N/A"
                volt_range = "N/A"

            if stats.consecutive_fail >= 3:
                fail_warn = "!! %d" % stats.consecutive_fail
            else:
                fail_warn = str(stats.consecutive_fail)

            print("%-16s %6d %6d %6d %7.1f%% %8s %12s %14s" % (
                dev_id, stats.total_count, stats.success_count,
                stats.fail_count, stats.success_rate,
                fail_warn, temp_range, volt_range))

        print("=" * 90)
        print("  Total: %d devices  |  Log: %s" % (
            len(self.devices), self.csv_path))
        print("=" * 90)
        print("")

    def run(self):
        print("=" * 50)
        print("  Solar Device Status MQTT Monitor")
        print("=" * 50)
        print("  Broker: %s:%d" % (MQTT_BROKER, MQTT_PORT))
        print("  Topic:  %s" % MQTT_TOPIC)
        print("  Log:    %s" % self.csv_path)
        print("  Press Ctrl+C to stop and show summary")
        print("=" * 50)
        print("")

        try:
            self.client.connect(MQTT_BROKER, MQTT_PORT, keepalive=60)
            self.client.loop_start()
            while True:
                time.sleep(1)
        except KeyboardInterrupt:
            print("\n\nStopping...")
        except ConnectionRefusedError:
            print("[Error] Cannot connect to %s:%d" % (MQTT_BROKER, MQTT_PORT))
        except Exception as e:
            print("[Error] %s" % e)
        finally:
            self.client.loop_stop()
            self.client.disconnect()
            self.print_summary()
            self.csv_file.close()
            print("Log saved to: %s" % self.csv_path)


if __name__ == "__main__":
    monitor = SolarMonitor()
    monitor.run()
