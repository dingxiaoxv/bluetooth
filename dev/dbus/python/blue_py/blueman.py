from gi.repository import GLib
from datetime import datetime
import bluetooth_utils
import bluetooth_constants
import bluetooth_exceptions
import dbus
import dbus.exceptions
import dbus.service
import dbus.mainloop.glib
import sys
import signal
import json
sys.path.insert(0, '.')

bus = None
adapter_path = None
mainloop = None
adv = None
adv_mgr_interface = None
adapter_interface = None
devices = {}
managed_objects_found = 0
robot_id = None

def onExit(sig, frame):
  global adv
  global adv_mgr_interface
  global adapter_interface
  global bus
  global mainloop

  print('program destoryed')
  adv_mgr_interface.UnregisterAdvertisement(adv)
  print('Advertisement unregistered')
  dbus.service.Object.remove_from_connection(adv)
  adapter_interface.StopDiscovery()
  bus.remove_signal_receiver(interfaces_added,"InterfacesAdded")
  bus.remove_signal_receiver(interfaces_added,"InterfacesRemoved")
  bus.remove_signal_receiver(properties_changed,"PropertiesChanged")
  print('Scan stoped')
  
  mainloop.quit()
  sys.exit(0)
signal.signal(signal.SIGINT, onExit)

def get_robot_id():
  global robot_id
  json_file_path = '/config/app/delicfg.json'
  with open(json_file_path, 'r') as json_file:
    data = json.load(json_file)
  robot_id = data["general"]["SN"]
  print("robot id ", robot_id)

# adverties data
class Advertisement(dbus.service.Object):
  PATH_BASE = '/org/bluez/dreame/advertisement'

  def __init__(self, bus, index, advertising_type):
    self.path = self.PATH_BASE + str(index)
    self.bus = bus
    self.ad_type = advertising_type
    self.service_uuids = ['FE97']
    self.manufacturer_data = None
    self.solicit_uuids = None
    self.service_data = None
    self.local_name = 'P21023616CN00002LF'
    # self.local_name = robot_id
    self.include_tx_power = True
    self.data = None
    self.discoverable = True
    dbus.service.Object.__init__(self, bus, self.path)

  def get_properties(self):
    properties = dict()
    properties['Type'] = self.ad_type
    if self.service_uuids is not None:
      properties['ServiceUUIDs'] = dbus.Array(self.service_uuids, signature='s')
    if self.solicit_uuids is not None:
      properties['SolicitUUIDs'] = dbus.Array(self.solicit_uuids, signature='s')
    if self.manufacturer_data is not None:
      properties['ManufacturerData'] = dbus.Dictionary(self.manufacturer_data, signature='qv')
    if self.service_data is not None:
      properties['ServiceData'] = dbus.Dictionary(self.service_data, signature='sv')
    if self.local_name is not None:
      properties['LocalName'] = dbus.String(self.local_name)
    if self.discoverable is not None and self.discoverable == True:
      properties['Discoverable'] = dbus.Boolean(self.discoverable)
    if self.include_tx_power:
      properties['Includes'] = dbus.Array(["tx-power"], signature='s')
    if self.data is not None:
      properties['Data'] = dbus.Dictionary(self.data, signature='yv')
    print(properties)
    return {bluetooth_constants.ADVERTISING_MANAGER_INTERFACE: properties}

  def get_path(self):
    return dbus.ObjectPath(self.path)

  @dbus.service.method(bluetooth_constants.DBUS_PROPERTIES, in_signature='s', out_signature='a{sv}')
  def GetAll(self, interface):
    if interface != bluetooth_constants.ADVERTISEMENT_INTERFACE:
      raise bluetooth_exceptions.InvalidArgsException()
    return self.get_properties()[bluetooth_constants.ADVERTISING_MANAGER_INTERFACE]

  @dbus.service.method(bluetooth_constants.ADVERTISING_MANAGER_INTERFACE, in_signature='', out_signature='')
  def Release(self):
    print('%s: Released' % self.path)

def register_ad_cb():
  print('Advertisement registered OK')

def register_ad_error_cb(error):
  global mainloop
  print('Error: Failed to register advertisement: ' + str(error))
  mainloop.quit()

def start_advertising():
  global adv
  global adv_mgr_interface
  print("Registering advertisement ", adv.get_path(), " Advertising as " + adv.local_name)
  adv_mgr_interface.RegisterAdvertisement(adv.get_path(), {}, reply_handler=register_ad_cb, error_handler=register_ad_error_cb)

# scan devices
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
  adapter_interface.SetDiscoveryFilter(scan_filter)
  adapter_interface.StartDiscovery(byte_arrays=True)
  print("scanning...")

def main():
  global bus
  global adapter_path
  global adv_mgr_interface
  global adapter_interface
  global adv
  global mainloop

  # get_robot_id()
  dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
  bus = dbus.SystemBus()
  adapter_path = bluetooth_constants.BLUEZ_NAMESPACE + bluetooth_constants.ADAPTER_NAME
  adv_mgr_interface = dbus.Interface(bus.get_object(bluetooth_constants.BLUEZ_SERVICE_NAME,adapter_path), 
                                    bluetooth_constants.ADVERTISING_MANAGER_INTERFACE)
  adv = Advertisement(bus, 0, 'peripheral')
  adapter_object = bus.get_object(bluetooth_constants.BLUEZ_SERVICE_NAME, adapter_path)
  adapter_interface = dbus.Interface(adapter_object, bluetooth_constants.ADAPTER_INTERFACE)
  
  get_known_devices(bus)
  discover_devices(bus)
  start_advertising()

  mainloop = GLib.MainLoop()
  mainloop.run()

if __name__ == '__main__':
  main()