"""Protocol 10 service for an unchanged Corsair 3.0.234 DLL."""
from pathlib import Path
import json, re, struct, uuid, socket, time
ROOT=Path(__file__).resolve().parents[1]
KEYS=[int(n) for n in re.findall(r'\d+', (ROOT/'bridge/keyboard_map.h').read_text().split('cue_keys[126] = {')[1].split('};')[0])]
HID=[int(n) for n in re.findall(r'\d+', (ROOT/'bridge/keyboard_map.h').read_text().split('hid_keys[126] = {')[1].split('};')[0])]

KEY_INDEX={key:i for i,key in enumerate(KEYS) if key}

class Backend:
 def __init__(self,port=6743,background=False):
  self.sessions={};self.focus=0;self.background=background;self.udp=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);self.address=('127.0.0.1',port)
  self.calls={};self.errors=[];self.last_emit=0
  self.frames_sent=0;self.releases_sent=0;self.accepted_colors=0;self.ignored_leds=set()
 def emit(self,s,release=False):
  if not release and (not s['selected'] or (not self.background and self.focus!=s['pid'])):return
  colors=s['colors'];count=0 if release else len(colors)
  packet=struct.pack('<I6H16s64s',0x3153474f,2,2 if release else 1,0x201,0 if release else 6,0 if release else 21,count,s['uuid'].bytes,b'iCUE original DLL')
  if count:packet+=struct.pack('<126I',*colors)
  self.udp.sendto(packet,self.address)
  if release:self.releases_sent+=1
  else:self.frames_sent+=1
 def tick(self):
  now=time.monotonic()
  if now-self.last_emit<.2:return
  self.last_emit=now
  for s in self.sessions.values():self.emit(s)
 def dispatch(self,event):
  if event.get('event')=='focus':
   self.focus=event['pid']
   for s in self.sessions.values():
    if not self.background and self.focus!=s['pid'] and s['selected']:self.emit(s,True)
   self.tick();return {}
  cid=event.get('client')
  if event.get('event')=='disconnect':
   if cid in self.sessions:self.emit(self.sessions.pop(cid),True)
   return {}
  request=event.get('request',{});method=request.get('method');params=request.get('params',{})
  self.calls[method]=self.calls.get(method,0)+1
  s=self.sessions.setdefault(cid,{'pid':event.get('pid',0),'uuid':uuid.uuid4(),'colors':[0]*126,'selected':False})
  response={'errorCode':0,'result':True}
  if method=='CorsiarHandshakeMethod':
   protocol=params.get('sdkProtocolVersion')
   if protocol!=10:return {'errorCode':4,'breakingChanges':True,'serverProtocolVersion':10,'serverVersion':'3.0.234'}
   return {'errorCode':0,'breakingChanges':False,'serverProtocolVersion':10,'serverVersion':'3.0.234'}
  if method=='CorsairGetDeviceCount':return {'errorCode':0,'result':1}
  if 'deviceIndex' in params and params['deviceIndex']!=0:return {'errorCode':5,'result':False}
  if method=='CorsairGetDeviceInfo':
   response['result']={'type':2,'model':'OpenRGB virtual keyboard','physicalLayout':1,'logicalLayout':1,'capsMask':1,'ledsCount':sum(bool(k) for k in KEYS),'channels':[],'deviceId':'openrgb-keyboard'}
  elif method in ('CorsairGetLedPositions','CorsairGetLedPositionsByDeviceIndex'):
   response['result']=[{'ledId':k,'top':(i//21)*20,'left':(i%21)*20,'height':18,'width':18} for i,k in enumerate(KEYS) if k]
  elif method in ('CorsairSetLedsColors','CorsairSetLedsColorsFlushBuffer'):
   groups=params.get('ledsColorsByDeviceIndex',[]) if method.endswith('FlushBuffer') else [{'deviceIndex':0,'ledColors':params.get('ledsColors','')}]
   pending=[]
   if not isinstance(groups,list) or len(groups)>1:return {'errorCode':5,'result':False}
   for group in groups:
    if group.get('deviceIndex')!=0:return {'errorCode':5,'result':False}
    values=self.parse_colors(group.get('ledColors',''))
    pending.extend(values)
   self.accepted_colors+=1
   for led,r,g,b in pending:
    if led not in KEY_INDEX and len(self.ignored_leds)<32:self.ignored_leds.add(led)
    if led in KEY_INDEX:s['colors'][KEY_INDEX[led]]=r|(g<<8)|(b<<16)
   if any(led in KEY_INDEX for led,*_ in pending):s['selected']=True;self.emit(s)
  elif method in ('CorsairGetLedsColors','CorsairGetLedsColorsByDeviceIndex'):
   values=self.parse_colors(params.get('ledsColors',params.get('ledColors','')))
   result=[]
   for led,*_ in values:
    c=s['colors'][KEY_INDEX[led]] if led in KEY_INDEX else (_[0]|(_[1]<<8)|(_[2]<<16));result.append(','.join(map(str,(led,c&255,(c>>8)&255,(c>>16)&255))))
   response['ledsColors']='#'.join(result)
  elif method=='CorsairGetLedIdForKeyName':
   key=params.get('keyName','')
   if isinstance(key,int):key=chr(key) if 0<=key<128 else ''
   key=key.upper() if isinstance(key,str) else ''
   code=ord(key)-ord('A')+4 if len(key)==1 and 'A'<=key<='Z' else ord(key)-ord('1')+30 if len(key)==1 and '1'<=key<='9' else 39 if key=='0' else 44 if key==' ' else 0
   response['result']=KEYS[HID.index(code)] if code in HID and code else 0
  elif method=='CorsiarReleaseControlMethod':self.emit(s,True);s['selected']=False
  elif method in ('InternalAquireAccessMode','CorsairSetLayerPriority'):pass
  else:
   message='Unsupported method: '+str(method)
   if message not in self.errors:self.errors=(self.errors+[message])[-8:]
   return {'errorCode':4,'result':False}
  return response
 @staticmethod
 def parse_colors(raw):
  if not isinstance(raw,str) or len(raw)>60000:raise ValueError('Color packet length')
  if not raw:return []
  values=[]
  for item in raw.split('#'):
   if not re.fullmatch(r'[0-9]+,[0-9]+,[0-9]+,[0-9]+',item):raise ValueError('Color packet format')
   led,r,g,b=map(int,item.split(','))
   if led>2147483647:raise ValueError('LED identifier out of range')
   if any(c>255 for c in (r,g,b)):raise ValueError('RGB channel out of range')
   values.append((led,r,g,b))
  if len(values)>4096:raise ValueError('Too many LEDs')
  return values
 def close(self):
  for s in self.sessions.values():self.emit(s,True)
  self.udp.close()
