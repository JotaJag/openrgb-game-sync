import sys,struct,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from unittest.mock import patch
from pipe_service.backend import Backend

class Datagram:
 def __init__(self,*a):self.packets=[]
 def sendto(self,data,address):self.packets.append(data)
 def close(self):pass
class PipeServiceTests(unittest.TestCase):
 def setUp(self):
  self.mock=patch('pipe_service.backend.socket.socket',Datagram);self.mock.start();self.addCleanup(self.mock.stop)
  self.backend=Backend();self.addCleanup(self.backend.close)
 def call(self,method,params=None):return self.backend.dispatch({'event':'request','client':1,'pid':123,'request':{'method':method,'params':params or {}}})
 def test_original_protocol_and_device_type(self):
  self.assertEqual(self.call('CorsiarHandshakeMethod',{'sdkProtocolVersion':16})['errorCode'],4)
  self.assertEqual(self.call('CorsiarHandshakeMethod',{'sdkProtocolVersion':10})['serverProtocolVersion'],10)
  self.assertEqual(self.call('CorsairGetDeviceInfo',{'deviceIndex':0})['result']['type'],2)
  self.assertEqual(self.call('CorsairGetDeviceInfo',{'deviceIndex':99})['errorCode'],5)
 def test_colors_focus_readback_and_release(self):
  self.call('CorsiarHandshakeMethod',{'sdkProtocolVersion':10})
  self.call('CorsairSetLedsColorsFlushBuffer',{'ledsColorsByDeviceIndex':[{'deviceIndex':0,'ledColors':'1,10,20,30#2,40,50,60'}]})
  self.assertFalse(self.backend.udp.packets)
  self.backend.dispatch({'event':'focus','pid':123})
  frame=self.backend.udp.packets[-1];self.assertEqual(struct.unpack_from('<I',frame,96)[0],0x1e140a)
  self.assertEqual(self.call('CorsairGetLedsColors',{'ledsColors':'1,0,0,0'})['ledsColors'],'1,10,20,30')
  self.backend.dispatch({'event':'focus','pid':999})
  self.assertEqual(struct.unpack_from('<H',self.backend.udp.packets[-1],6)[0],2)
  self.backend.dispatch({'event':'disconnect','client':1});self.assertFalse(self.backend.sessions)
 def test_strict_color_bounds(self):
  for raw in ['1,256,0,0','2147483648,1,2,3','1,1,2,3 junk','-1,0,0,0']:
   with self.assertRaises(ValueError):self.backend.parse_colors(raw)
 def test_missing_keyboard_keys_do_not_drop_the_whole_frame(self):
  self.call('CorsiarHandshakeMethod',{'sdkProtocolVersion':10})
  self.backend.dispatch({'event':'focus','pid':123})
  reply=self.call('CorsairSetLedsColors',{'ledsColors':'1,10,20,30#121,0,0,0#98,0,0,0'})
  self.assertEqual(reply['errorCode'],0)
  self.assertEqual(struct.unpack_from('<I',self.backend.udp.packets[-1],96)[0],0x1e140a)
if __name__=='__main__':unittest.main()
