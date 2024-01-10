#!/usr/bin/python3

from gi.repository import GLib
from datetime import datetime
import bluetooth_utils
import bluetooth_constants
import dbus
import dbus.mainloop.glib
import sys
import signal
sys.path.insert(0, '.')

bus = None
adapter_interface = None
adapter_path = None
mainloop = None
devices = {}
managed_objects_found = 0

def onExit(sig, frame):
  global adapter_interface
  global mainloop
  global bus
  print("program destoryed...")
  adapter_interface.StopDiscovery()
  bus = dbus.SystemBus()
  bus.remove_signal_receiver(interfaces_added,"InterfacesAdded")
  bus.remove_signal_receiver(interfaces_added,"InterfacesRemoved")
  bus.remove_signal_receiver(properties_changed,"PropertiesChanged")
  mainloop.quit()
  sys.exit(0)
signal.signal(signal.SIGINT, onExit)

def get_known_devices(bus):
  global managed_objects_found
  global devices
  print("Listing devices already known to BlueZ:")
  object_manager = dbus.Interface(bus.get_object(bluetooth_constants.BLUEZ_SERVICE_NAME, "/"), bluetooth_constants.DBUS_OM_IFACE)
  managed_objects = object_manager.GetManagedObjects()

  for path, ifaces in managed_objects.items():
    for iface_name in ifaces:
      if iface_name == bluetooth_constants.DEVICE_INTERFACE:
        device_properties = ifaces[bluetooth_constants.DEVICE_INTERFACE]
        if 'Name' in device_properties and 'P2102' in device_properties['Name']:
          devices[path] = device_properties
          managed_objects_found += 1
          print("EXI path  : ", path)
          if 'Address' in device_properties:
            print("EXI bdaddr: ", bluetooth_utils.dbus_to_python(device_properties['Address']))
          if 'Name' in device_properties:
            print("EXI name  : ", bluetooth_utils.dbus_to_python(device_properties['Name']))
          if 'Connected' in device_properties:
            print("EXI cncted: ", bluetooth_utils.dbus_to_python(device_properties['Connected']))
          print("------------------------------")
  print("Found ",managed_objects_found," managed device objects")
   
def interfaces_added(path, interfaces):
  global devices
  if not bluetooth_constants.DEVICE_INTERFACE in interfaces:
    return
  
  device_properties = interfaces[bluetooth_constants.DEVICE_INTERFACE]
  if path not in devices:
    if 'Name' in device_properties and 'P2102' in device_properties['Name']:
      print("NEW path  :", path)
      print("current time: ", datetime.now().strftime("%H:%M:%S"))
      devices[path] = device_properties
      dev = devices[path]
      if 'Address' in dev:
        print("NEW bdaddr: ", bluetooth_utils.dbus_to_python(dev['Address']))
      if 'Name' in dev:
        print("NEW name  : ", bluetooth_utils.dbus_to_python(dev['Name']))
      if 'RSSI' in dev:
        print("NEW RSSI  : ", bluetooth_utils.dbus_to_python(dev['RSSI']))
      print("------------------------------")

def properties_changed(interface, changed, invalidated, path):
  global devices
  if interface != bluetooth_constants.DEVICE_INTERFACE:
    return

  if path in devices:
    devices[path] = dict(devices[path].items())
    devices[path].update(changed.items())
    dev = devices[path]
    print("CHG path  :", path)
    print("current time: ", datetime.now().strftime("%H:%M:%S"))
    if 'Address' in dev:
      print("CHG bdaddr: ", bluetooth_utils.dbus_to_python(dev['Address']))
    if 'Name' in dev:
      print("CHG name  : ", bluetooth_utils.dbus_to_python(dev['Name']))
    if 'RSSI' in dev:
      print("CHG RSSI  : ", bluetooth_utils.dbus_to_python(dev['RSSI']))
    print("------------------------------")

def interfaces_removed(path, interfaces):
  global devices
  if not bluetooth_constants.DEVICE_INTERFACE in interfaces:
    return
  
  if path in devices:
    dev = devices[path]
    if 'Address' in dev:
      print("DEL bdaddr: ", bluetooth_utils.dbus_to_python(dev['Address']))
    else:
      print("DEL path  : ", path)
      print("------------------------------")
    del devices[path]

def discover_devices(bus):
  global adapter_interface

  # InterfacesAdded signal is emitted by BlueZ when an advertising packet from a device it doesn't
  # already know about is received
  bus.add_signal_receiver(interfaces_added,
          dbus_interface = bluetooth_constants.DBUS_OM_IFACE,
          signal_name = "InterfacesAdded")

  # InterfacesRemoved signal is emitted by BlueZ when a device "goes away"
  bus.add_signal_receiver(interfaces_removed,
          dbus_interface = bluetooth_constants.DBUS_OM_IFACE,
          signal_name = "InterfacesRemoved")

  # PropertiesChanged signal is emitted by BlueZ when something re: a device already encountered
  # changes e.g. the RSSI value
  bus.add_signal_receiver(properties_changed,
          dbus_interface = bluetooth_constants.DBUS_PROPERTIES,
          signal_name = "PropertiesChanged",
          path_keyword = "path")
  
  scan_filter = dict()
  uuids = ['FE97']
  scan_filter.update({"UUIDs": uuids})
  scan_filter.update({"Discoverable": dbus.Boolean(False)})
  scan_filter.update({"Transport": dbus.String('le')})
  scan_filter.update({"DuplicateData": dbus.Boolean(False)})
  # scan_filter.update({"Pattern": dbus.String('P2102')})
  adapter_interface.SetDiscoveryFilter(scan_filter)
  adapter_interface.StartDiscovery(byte_arrays=True)
  print("Scanning...")

def main():
  global bus
  global adapter_path
  global adapter_interface
  global mainloop
  global devices
  global managed_objects_found
  
  # dbus initialisation steps
  dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
  bus = dbus.SystemBus()
  adapter_path = bluetooth_constants.BLUEZ_NAMESPACE + bluetooth_constants.ADAPTER_NAME
  adapter_object = bus.get_object(bluetooth_constants.BLUEZ_SERVICE_NAME, adapter_path)
  adapter_interface = dbus.Interface(adapter_object, bluetooth_constants.ADAPTER_INTERFACE)

  get_known_devices(bus)
  discover_devices(bus)
  mainloop = GLib.MainLoop()
  mainloop.run()

if __name__ == '__main__':
  main()