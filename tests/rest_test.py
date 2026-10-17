import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import http.client, json, socket, struct, threading, time, unittest
from chroma_rest import Server

class RESTTests(unittest.TestCase):
 def setUp(self):
  self.udp=socket.socket(socket.AF_INET,socket.SOCK_DGRAM); self.udp.bind(('127.0.0.1',0)); self.udp.settimeout(.5)
  self.server=Server(('127.0.0.1',0),self.udp.getsockname()[1]); self.stop=threading.Event()
  def run():
   while not self.stop.is_set(): self.server.handle_request(); self.server.tick()
  self.thread=threading.Thread(target=run); self.thread.start()
 def tearDown(self):
  self.stop.set(); self.thread.join(2); self.server.server_close(); self.udp.close()
 def request(self,method,path,body=None,headers=None):
  conn=http.client.HTTPConnection('127.0.0.1',self.server.server_port,timeout=2)
  conn.request(method,path,json.dumps(body).encode() if body is not None else None,headers or {})
  response=conn.getresponse(); data=json.loads(response.read()); conn.close(); return response.status,data
 def test_session_effect_cache_release(self):
  _,session=self.request('POST','/razer/chromasdk',{'title':'REST integration'})
  path='/'+session['uri'].split('/',3)[3]
  _,effect=self.request('POST',path+'/keyboard',{'effect':'CHROMA_STATIC','param':{'color':65280}})
  with self.assertRaises(socket.timeout): self.udp.recv(4096)
  self.assertEqual(self.request('PUT',path+'/effect',{'id':effect['id']})[1]['result'],0)
  packet=self.udp.recv(4096); values=struct.unpack_from('<I6H',packet)
  self.assertEqual(values,(0x3153474f,2,1,257,6,22,132)); self.assertEqual(set(struct.unpack_from('<132I',packet,96)),{65280})
  self.assertEqual(self.request('PUT',path+'/heartbeat')[1]['result'],0)
  self.request('DELETE',path)
  while True:
   packet=self.udp.recv(4096)
   if struct.unpack_from('<H',packet,6)[0]==2: break
  self.assertEqual(len(packet),96)
 def test_validation_unsupported_and_expiration(self):
  self.assertEqual(self.request('POST','/razer/chromasdk',{}, {'Origin':'https://example.com'})[0],403)
  _,session=self.request('POST','/razer/chromasdk',{})
  path='/'+session['uri'].split('/',3)[3]
  self.assertEqual(self.request('PUT',path+'/keyboard',{'effect':'CHROMA_WAVE'})[1]['result'],50)
  self.assertEqual(self.request('PUT',path+'/keyboard',{'effect':'CHROMA_CUSTOM','param':[[1]]})[0],400)
  self.request('PUT',path+'/chromalink',{'effect':'CHROMA_STATIC','param':{'color':255}})
  self.server.sessions[session['sessionid']]['seen']=time.monotonic()-16
  time.sleep(.3); self.assertFalse(self.server.sessions)

if __name__=='__main__': unittest.main()
