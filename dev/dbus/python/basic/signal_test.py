#!/usr/bin/python3
import dbus
import dbus.mainloop.glib
from gi.repository import GLib

mainloop = None

def greeting_signal_received(greeting):
  print(greeting)

dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
bus = dbus.SystemBus()
# The add_signal_receiver function registers this application’s requirement that signals named
# GreetingSignal emitted by an interface called com.example.greeting
bus.add_signal_receiver(greeting_signal_received, signal_name = "GreetingSignal", dbus_interface = "com.example.greeting")

mainloop = GLib.MainLoop()
mainloop.run()

# run in terminal
# dbus-send --system --type=signal / com.example.greeting.GreetingSignal string:"hello"