# -*- coding: utf-8 -*-
"""
Solar Device Status MQTT Monitor - GUI Version (Chinese)
"""

import tkinter as tk
from tkinter import ttk, messagebox, scrolledtext
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
LOG_DIR        = os.path.join(os.path.dirname(os.path.abspath(__file__)), "logs")
POLL_INTERVAL  = 1000
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


class SolarMonitorApp:
    def __init__(self, root):
        self.root = root
        self.root.title("\u592a\u9633\u80fd\u4ece\u673a\u72b6\u6001\u76d1\u63a7")
        self.root.geometry("1200x750")
        self.root.minsize(1000, 650)

        self.devices = OrderedDict()
        self.start_time = None
        self.csv_file = None
        self.csv_writer = None
        self.csv_path = ""
        self.client = None
        self.connected = False
        self.msg_count = 0

        self._build_ui()
        self._log("\u7a0b\u5e8f\u5df2\u542f\u52a8\u3002\u8bf7\u914d\u7f6eMQTT\u670d\u52a1\u5668\u5730\u5740\u540e\u70b9\u51fb\u3010\u8fde\u63a5\u3011\u3002")

    def _build_ui(self):
        # --- Top: Connection Panel ---
        conn_frame = ttk.LabelFrame(self.root, text="MQTT \u8fde\u63a5\u914d\u7f6e", padding=8)
        conn_frame.pack(fill=tk.X, padx=8, pady=(8, 4))

        ttk.Label(conn_frame, text="\u670d\u52a1\u5668:").grid(row=0, column=0, sticky=tk.W, padx=(0,4))
        self.broker_var = tk.StringVar(value=MQTT_BROKER)
        ttk.Entry(conn_frame, textvariable=self.broker_var, width=28).grid(row=0, column=1, padx=4)

        ttk.Label(conn_frame, text="\u7aef\u53e3:").grid(row=0, column=2, sticky=tk.W, padx=(8,4))
        self.port_var = tk.StringVar(value=str(MQTT_PORT))
        ttk.Entry(conn_frame, textvariable=self.port_var, width=8).grid(row=0, column=3, padx=4)

        ttk.Label(conn_frame, text="\u4e3b\u9898:").grid(row=0, column=4, sticky=tk.W, padx=(8,4))
        self.topic_var = tk.StringVar(value=MQTT_TOPIC)
        ttk.Entry(conn_frame, textvariable=self.topic_var, width=22).grid(row=0, column=5, padx=4)

        self.conn_btn = ttk.Button(conn_frame, text="\u8fde\u63a5", command=self._toggle_connection, width=10)
        self.conn_btn.grid(row=0, column=6, padx=(16, 0))

        self.status_label = ttk.Label(conn_frame, text="  \u672a\u8fde\u63a5", foreground="gray")
        self.status_label.grid(row=0, column=7, padx=12)

        # --- Middle: Device Table ---
        table_frame = ttk.LabelFrame(self.root, text="\u8bbe\u5907\u72b6\u6001\u5217\u8868", padding=4)
        table_frame.pack(fill=tk.BOTH, expand=True, padx=8, pady=4)

        columns = ("device", "temp", "voltage", "dc_heat", "pwr_rev",
                    "total", "ok", "fail", "rate", "cons_fail",
                    "temp_range", "volt_range", "last_seen")
        self.tree = ttk.Treeview(table_frame, columns=columns, show="headings", height=14)

        headers = {
            "device":     ("\u8bbe\u5907ID",          120),
            "temp":       ("\u6e29\u5ea6(\u2103)",     80),
            "voltage":    ("\u7535\u538b(V)",          80),
            "dc_heat":    ("\u76f4\u6d41\u52a0\u70ed",  80),
            "pwr_rev":    ("\u7535\u6e90\u53cd\u63a5",  80),
            "total":      ("\u603b\u6b21\u6570",        65),
            "ok":         ("\u6210\u529f",              55),
            "fail":       ("\u5931\u8d25",              55),
            "rate":       ("\u6210\u529f\u7387",        70),
            "cons_fail":  ("\u8fde\u7eed\u5931\u8d25",  80),
            "temp_range": ("\u6e29\u5ea6\u8303\u56f4",  100),
            "volt_range": ("\u7535\u538b\u8303\u56f4",  100),
            "last_seen":  ("\u6700\u540e\u901a\u4fe1",  130),
        }
        for col, (text, width) in headers.items():
            self.tree.heading(col, text=text)
            self.tree.column(col, width=width, anchor=tk.CENTER)

        vsb = ttk.Scrollbar(table_frame, orient=tk.VERTICAL, command=self.tree.yview)
        self.tree.configure(yscrollcommand=vsb.set)
        self.tree.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        vsb.pack(side=tk.RIGHT, fill=tk.Y)

        self.tree.tag_configure("ok", foreground="#006600")
        self.tree.tag_configure("fail", foreground="#cc0000")
        self.tree.tag_configure("warn", foreground="#cc6600")

        # --- Bottom: Log Panel ---
        log_frame = ttk.LabelFrame(self.root, text="\u4e8b\u4ef6\u65e5\u5fd7", padding=4)
        log_frame.pack(fill=tk.BOTH, padx=8, pady=(4, 8), expand=False)

        self.log_text = scrolledtext.ScrolledText(log_frame, height=7, state=tk.DISABLED,
                                                   font=("Consolas", 9))
        self.log_text.pack(fill=tk.BOTH, expand=True)

        # --- Status Bar ---
        self.msg_label = ttk.Label(self.root, text="\u6d88\u606f: 0 | \u8bbe\u5907: 0")
        self.msg_label.pack(side=tk.BOTTOM, fill=tk.X, padx=8, pady=(0, 4))

    def _log(self, msg):
        ts = datetime.now().strftime("%H:%M:%S")
        self.log_text.configure(state=tk.NORMAL)
        self.log_text.insert(tk.END, "[%s] %s\n" % (ts, msg))
        self.log_text.see(tk.END)
        self.log_text.configure(state=tk.DISABLED)

    def _toggle_connection(self):
        if self.connected:
            self._disconnect()
        else:
            self._connect()

    def _connect(self):
        broker = self.broker_var.get().strip()
        try:
            port = int(self.port_var.get().strip())
        except ValueError:
            messagebox.showerror("\u9519\u8bef", "\u7aef\u53e3\u53f7\u65e0\u6548!")
            return
        topic = self.topic_var.get().strip()

        if not broker:
            messagebox.showerror("\u9519\u8bef", "\u670d\u52a1\u5668\u5730\u5740\u4e0d\u80fd\u4e3a\u7a7a!")
            return

        self._log("\u6b63\u5728\u8fde\u63a5 %s:%d ..." % (broker, port))

        try:
            self.client = mqtt.Client(
                callback_api_version=mqtt.CallbackAPIVersion.VERSION2,
                client_id="solar_gui_%d" % int(time.time())
            )
            if MQTT_USERNAME:
                self.client.username_pw_set(MQTT_USERNAME, MQTT_PASSWORD)
            self.client.on_connect = self._on_connect
            self.client.on_message = self._on_message
            self.client.on_disconnect = self._on_disconnect

            self.client.connect(broker, port, keepalive=60)
            self.client.loop_start()

            self.start_time = datetime.now()
            self._open_csv()
            self.connected = True
            self.conn_btn.configure(text="\u65ad\u5f00")
            self.status_label.configure(text="  \u5df2\u8fde\u63a5", foreground="green")
            self._log("\u5df2\u8fde\u63a5\u5230 %s:%d" % (broker, port))
            self._log("\u5df2\u8ba2\u9605\u4e3b\u9898: %s" % topic)
            self._refresh_table()

        except Exception as e:
            self._log("\u8fde\u63a5\u5931\u8d25: %s" % str(e))
            messagebox.showerror("\u8fde\u63a5\u9519\u8bef", str(e))

    def _disconnect(self):
        if self.client:
            self.client.loop_stop()
            self.client.disconnect()
            self.client = None
        self.connected = False
        self.conn_btn.configure(text="\u8fde\u63a5")
        self.status_label.configure(text="  \u672a\u8fde\u63a5", foreground="gray")
        self._log("\u5df2\u65ad\u5f00\u8fde\u63a5")
        if self.csv_file:
            self.csv_file.close()
            self.csv_file = None

    def _on_connect(self, client, userdata, flags, reason_code, properties):
        if reason_code == 0:
            topic = self.topic_var.get().strip()
            client.subscribe(topic)
            self.root.after(0, lambda: self._log("MQTT\u8fde\u63a5\u6210\u529f\uff0c\u5df2\u8ba2\u9605: %s" % topic))
        else:
            self.root.after(0, lambda: self._log("MQTT\u8fde\u63a5\u5931\u8d25: %d" % reason_code))

    def _on_disconnect(self, client, userdata, flags, reason_code, properties):
        self.root.after(0, lambda: self._log("MQTT\u8fde\u63a5\u65ad\u5f00 (%d)" % reason_code))

    def _on_message(self, client, userdata, msg):
        try:
            payload = json.loads(msg.payload.decode("utf-8"))
        except (json.JSONDecodeError, UnicodeDecodeError) as e:
            self.root.after(0, lambda: self._log("\u89e3\u6790\u9519\u8bef: %s" % str(e)))
            return

        device_id = msg.topic.split("/")[-1] if "/" in msg.topic else msg.topic
        now = datetime.now()

        if device_id not in self.devices:
            self.devices[device_id] = DeviceStats(device_id)
        self.devices[device_id].update(payload, now)
        self.msg_count += 1

        if self.csv_writer:
            ts = now.strftime("%Y-%m-%d %H:%M:%S")
            ok = payload.get("ok", 0)
            self.csv_writer.writerow([
                ts, device_id,
                payload.get("t", ""), payload.get("v", ""),
                "\u662f" if payload.get("dc") else "\u5426",
                "\u662f" if payload.get("pr") else "\u5426",
                "\u662f" if ok else "\u5426",
                payload.get("err", "")
            ])
            self.csv_file.flush()

        ok = payload.get("ok", 0)
        if ok == 1:
            log_msg = "\u2705 %s  \u6e29\u5ea6=%d\u2103  \u7535\u538b=%dV  \u76f4\u6d41\u52a0\u70ed=%s  \u7535\u6e90\u53cd\u63a5=%s" % (
                device_id, payload.get("t", 0), payload.get("v", 0),
                "\u662f" if payload.get("dc") else "\u5426",
                "\u662f" if payload.get("pr") else "\u5426")
        else:
            log_msg = "\u274c %s  \u901a\u4fe1\u5931\u8d25(err=%s)" % (device_id, payload.get("err", "?"))
        self.root.after(0, lambda m=log_msg: self._log(m))

    def _open_csv(self):
        try:
            os.makedirs(LOG_DIR, exist_ok=True)
        except OSError:
            pass
        ds = datetime.now().strftime("%Y%m%d_%H%M%S")
        self.csv_path = os.path.join(LOG_DIR, "solar_log_%s.csv" % ds)
        self.csv_file = open(self.csv_path, "w", newline="", encoding="utf-8-sig")
        self.csv_writer = csv.writer(self.csv_file)
        self.csv_writer.writerow([
            "\u65f6\u95f4", "\u8bbe\u5907ID", "\u6e29\u5ea6(\u2103)", "\u7535\u538b(V)",
            "\u76f4\u6d41\u52a0\u70ed", "\u7535\u6e90\u53cd\u63a5", "\u6210\u529f", "\u9519\u8bef\u7801"
        ])
        self.csv_file.flush()

    def _refresh_table(self):
        if not self.connected:
            return

        for item in self.tree.get_children():
            self.tree.delete(item)

        for dev_id, stats in self.devices.items():
            ok = stats.last_ok
            if ok == 1:
                temp_str = "%d" % stats.last_status.get("t", 0)
                volt_str = "%d" % stats.last_status.get("v", 0)
                dc_str = "\u662f" if stats.last_status.get("dc") else "\u5426"
                pr_str = "\u662f" if stats.last_status.get("pr") else "\u5426"
                tag = "ok"
            elif ok == 0:
                temp_str = "--"
                volt_str = "--"
                dc_str = "--"
                pr_str = "--"
                tag = "fail"
            else:
                temp_str = "--"
                volt_str = "--"
                dc_str = "--"
                pr_str = "--"
                tag = ""

            if stats.consecutive_fail >= 3:
                tag = "warn"

            if stats.success_count > 0:
                temp_range = "%d~%d\u2103" % (stats.min_temp, stats.max_temp)
                volt_range = "%d~%dV" % (stats.min_voltage, stats.max_voltage)
            else:
                temp_range = "--"
                volt_range = "--"

            last_seen = stats.last_seen.strftime("%m-%d %H:%M:%S") if stats.last_seen else "--"
            cons_fail = "%d" % stats.consecutive_fail
            if stats.consecutive_fail >= 3:
                cons_fail = "!! %d" % stats.consecutive_fail

            self.tree.insert("", tk.END, values=(
                dev_id, temp_str, volt_str, dc_str, pr_str,
                stats.total_count, stats.success_count, stats.fail_count,
                "%.1f%%" % stats.success_rate, cons_fail,
                temp_range, volt_range, last_seen
            ), tags=(tag,))

        self.msg_label.configure(
            text="\u6d88\u606f: %d | \u8bbe\u5907: %d | \u65e5\u5fd7: %s" % (
                self.msg_count, len(self.devices), self.csv_path))

        self.root.after(POLL_INTERVAL, self._refresh_table)

    def on_closing(self):
        self._disconnect()
        self.root.destroy()


def main():
    root = tk.Tk()
    app = SolarMonitorApp(root)
    root.protocol("WM_DELETE_WINDOW", app.on_closing)
    root.mainloop()


if __name__ == "__main__":
    main()
