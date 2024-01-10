#!/usr/bin/python3
# Broadcasts connectable advertising packets

import bluetooth_constants
import bluetooth_exceptions
import dbus
import dbus.exceptions
import dbus.service
import dbus.mainloop.glib
import sys
import signal
import json
from gi.repository import GLib
sys.path.insert(0, '.')

bus = None
adapter_path = None
adv_mgr_interface = None
adv = None
mainloop = None
robot_id = None

def onExit(sig, frame):
  global adv
  global adv_mgr_interface
  global mainloop

  adv_mgr_interface.UnregisterAdvertisement(adv)
  print('Advertisement unregistered')
  dbus.service.Object.remove_from_connection(adv)
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
  print("Registering advertisement",adv.get_path())
  adv_mgr_interface.RegisterAdvertisement(adv.get_path(), {}, reply_handler=register_ad_cb, error_handler=register_ad_error_cb)
  print("Advertising as " + adv.local_name)

def main():
  global bus
  global adapter_path
  global adv_mgr_interface
  global adv
  global mainloop

  # get_robot_id()
  dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
  bus = dbus.SystemBus()
  adapter_path = bluetooth_constants.BLUEZ_NAMESPACE + bluetooth_constants.ADAPTER_NAME
  adv_mgr_interface = dbus.Interface(bus.get_object(bluetooth_constants.BLUEZ_SERVICE_NAME,adapter_path), 
                                    bluetooth_constants.ADVERTISING_MANAGER_INTERFACE)
  adv = Advertisement(bus, 0, 'peripheral')
  start_advertising()
  mainloop = GLib.MainLoop()
  mainloop.run()

if __name__ == '__main__':
  main()