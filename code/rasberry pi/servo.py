import paho.mqtt.client as mqtt
import pigpio
from collections import deque
import time

pi = pigpio.pi()

servo_topics = {
    "sensor/potentiometer_1": 6,    # 180 degree
    "sensor/potentiometer_2": 5,    # 180 degree
    "sensor/potentiometer_3": 27,   # 360 degree
    "sensor/potentiometer_4": 22,   # 360 degree
    "sensor/potentiometer_5": 17,   # 180 degree
    "sensor/potentiometer_6": 4     # 180 degree
}

buffers = {topic: deque(maxlen=5) for topic in servo_topics}
previous_pw = {topic: 1500 for topic in servo_topics}

for gpio_pin in servo_topics.values():
    pi.set_mode(gpio_pin, pigpio.OUTPUT)

def map_voltage_to_pulsewidth_180(v):
    v = max(0.0, min(3.3, v))
    return int((v / 3.3) * 2000 + 500)  # 500 to 2500 us pulsewidth

def map_voltage_to_pulsewidth_360(v):
    v = max(0.0, min(3.3, v))
    # Center at 1500, range ±1000 for continuous rotation speed control
    return int(1500 + (v - 1.65) * (1000 / 1.65))

def on_message(client, userdata, msg):
    topic = msg.topic
    if topic not in servo_topics:
        return

    try:
        voltage = float(msg.payload.decode())
        buffers[topic].append(voltage)
        avg_v = sum(buffers[topic]) / len(buffers[topic])

        if topic in ["sensor/potentiometer_3", "sensor/potentiometer_4"]:
            pw = map_voltage_to_pulsewidth_360(avg_v)
        else:
            pw = map_voltage_to_pulsewidth_180(avg_v)

        if abs(pw - previous_pw[topic]) > 15:
            pi.set_servo_pulsewidth(servo_topics[topic], pw)
            previous_pw[topic] = pw
            time.sleep(0.01)

    except ValueError:
        pass

client = mqtt.Client()
client.on_message = on_message
client.connect("192.168.100.25", 1883, 60)

for topic in servo_topics:
    client.subscribe(topic)

client.loop_forever()

