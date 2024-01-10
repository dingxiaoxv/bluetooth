#!/usr/bin/python3
import dbus
import dbus.service
import dbus.mainloop.glib
from gi.repository import GLib

mainloop = None

class Calculator(dbus.service.Object):
  # constructor
  def __init__(self, bus):
    self.path = '/com/example/calculator'
    dbus.service.Object.__init__(self, bus, self.path)

  # input param(ii) means 2 32-bit integer output param(i) means 1 32-bit integer
  @dbus.service.method("com.example.calculator_interface", in_signature='ii', out_signature='i')
  def Add(self, val1, val2):
    sum = val1 + val2
    print(val1," + ",val2," = ",sum)
    return sum
  
  @dbus.service.method("com.example.calculator_interface", in_signature='dd', out_signature='d')
  def division(self, val1, val2):
    res = val1 / val2
    print(val1," / ",val2," = ",res)
    return res

dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
bus = dbus.SystemBus()
calc = Calculator(bus)
mainloop = GLib.MainLoop()
print("waiting for some calculations to do....")
mainloop.run()

