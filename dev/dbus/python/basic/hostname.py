#!/usr/bin/python3
import dbus

bus = dbus.SystemBus()
# org.freedesktop.hostname1 --> interface 
# /org/freedesktop/hostname1 --> object
proxy = bus.get_object('org.freedesktop.hostname1','/org/freedesktop/hostname1')
interface = dbus.Interface(proxy, 'org.freedesktop.DBus.Properties')

print("----------------")
all_props = interface.GetAll('org.freedesktop.hostname1')
print(all_props)

hostname = interface.Get('org.freedesktop.hostname1','Hostname')
print("The host name is ",hostname)