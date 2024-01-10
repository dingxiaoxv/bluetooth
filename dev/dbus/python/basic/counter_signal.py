#!/usr/bin/python3
import dbus
import dbus.service
import dbus.mainloop.glib
from gi.repository import GLib
import time

mainloop = None

class Counter(dbus.service.Object):
  def __init__(self, bus):
    self.path = '/com/example/counter'
    self.c = 0
    dbus.service.Object.__init__(self, bus, self.path)

  def increment(self):
    self.c = self.c + 1
    print(self.c)
  
  # register a signal
  @dbus.service.signal('com.example.Counter')
  def CounterSignal(self, counter):
    # nothing else to do so...
    pass

  def emitCounterSignal(self):
    self.CounterSignal(self.c)

dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
bus = dbus.SystemBus()
counter = Counter(bus)

while True:
  counter.increment()
  counter.emitCounterSignal()
  time.sleep(1)

# run dbus-monitor --system to monitor