#!/usr/bin/env python3
"""Local Chroma REST subset. No hardware access or game-library replacement."""
from http.server import BaseHTTPRequestHandler, HTTPServer
from urllib.parse import urlsplit
import json, os, socket, struct, time, uuid

DEVICES={'keyboard':(1,6,22),'mouse':(2,9,7),'headset':(3,1,5),
         'mousepad':(4,1,15),'keypad':(5,4,5),'chromalink':(6,1,5)}


def effect_frame(device,data):
    role,rows,cols=DEVICES[device]; effect=data.get('effect'); param=data.get('param')
    if effect=='CHROMA_NONE': colors=[0]*(rows*cols)
    elif effect=='CHROMA_STATIC':
        color=param['color']
        if type(color)!=int or not 0<=color<=0xffffff: raise ValueError('color')
        colors=[color]*(rows*cols)
    elif effect in ('CHROMA_CUSTOM','CHROMA_CUSTOM_KEY'):
        matrix=param
        overlay=None
        if effect=='CHROMA_CUSTOM_KEY':
            if device!='keyboard': raise ValueError('device')
            matrix=param['color']; overlay=param['key']
        def flatten(value):
            if rows==1 and isinstance(value,list) and len(value)==cols and all(type(c)==int for c in value): return value
            if not isinstance(value,list) or len(value)!=rows: raise ValueError('rows')
            if any(not isinstance(row,list) or len(row)!=cols for row in value): raise ValueError('columns')
            return [c for row in value for c in row]
        colors=flatten(matrix)
        if any(type(c)!=int or not 0<=c<=0xffffff for c in colors): raise ValueError('color')
        if overlay is not None:
            keys=flatten(overlay)
            if any(type(c)!=int or not 0<=c<=0x1ffffff for c in keys): raise ValueError('key')
            colors=[(k&0xffffff) if k&0x1000000 else c for c,k in zip(colors,keys)]
    else: raise NotImplementedError(effect)
    return role,rows,cols,colors


class Server(HTTPServer):
    def __init__(self,address,udp_port=6743):
        super().__init__(address,Handler); self.timeout=.1; self.sessions={}
        self.udp=socket.socket(socket.AF_INET,socket.SOCK_DGRAM); self.destination=('127.0.0.1',udp_port)
        self.last_tick=0
    def packet(self,session,frame=None):
        role,rows,cols,colors=frame if frame else (0,0,0,[])
        data=struct.pack('<I6H16s64s',0x3153474f,2,1 if frame else 2,0x100|role,
                         rows,cols,len(colors),session['id'].bytes,session['app'].encode()[:63])
        if colors: data+=struct.pack('<'+'I'*len(colors),*colors)
        self.udp.sendto(data,self.destination)
    def tick(self):
        now=time.monotonic()
        if now-self.last_tick<.2: return
        self.last_tick=now
        for key,session in list(self.sessions.items()):
            if now-session['seen']>15:
                self.packet(session); del self.sessions[key]
            else:
                for frame in session['frames'].values(): self.packet(session,frame)
    def server_close(self):
        for session in self.sessions.values(): self.packet(session)
        self.udp.close(); super().server_close()


class Handler(BaseHTTPRequestHandler):
    def setup(self):
        super().setup(); self.connection.settimeout(.5)
    def log_message(self,*args): pass
    def answer(self,data,status=200):
        data=json.dumps(data).encode(); self.send_response(status)
        self.send_header('Content-Type','application/json'); self.send_header('Content-Length',str(len(data)))
        self.send_header('Connection','close'); self.end_headers(); self.wfile.write(data)
    def request(self):
        try:
            host=urlsplit('http://'+self.headers.get('Host','')).hostname
            origin=self.headers.get('Origin')
            if host not in ('127.0.0.1','localhost') or (origin and urlsplit(origin).hostname not in ('127.0.0.1','localhost')):
                return self.answer({'result':5},403)
            if self.headers.get('Transfer-Encoding'): return self.answer({'result':87},400)
            size=int(self.headers.get('Content-Length','0'))
            if not 0<=size<=32768: return self.answer({'result':87},413)
            raw=self.rfile.read(size); body=json.loads(raw) if size else {}
            if not isinstance(body,dict): raise ValueError('body')
            path=urlsplit(self.path).path.rstrip('/'); method=self.command
            if path=='/razer/chromasdk':
                if method=='GET': return self.answer({'version':'3.0.0','result':0})
                if method!='POST': return self.answer({'result':50},405)
                if len(self.server.sessions)>=8: return self.answer({'result':8},503)
                key=str(uuid.uuid4()); session={'id':uuid.UUID(key),'app':str(body.get('title','Chroma REST'))[:64],
                    'seen':time.monotonic(),'frames':{},'effects':{}}
                self.server.sessions[key]=session
                return self.answer({'sessionid':key,'uri':f'http://127.0.0.1:{self.server.server_port}/{key}/chromasdk','result':0})
            parts=path.strip('/').split('/')
            if len(parts)<2 or parts[1]!='chromasdk' or parts[0] not in self.server.sessions:
                return self.answer({'result':1167},404)
            session=self.server.sessions[parts[0]]; session['seen']=time.monotonic()
            endpoint=parts[2] if len(parts)==3 else ''
            if len(parts)==2 and method=='DELETE':
                self.server.packet(session); del self.server.sessions[parts[0]]; return self.answer({'result':0})
            if endpoint=='heartbeat' and method=='PUT': return self.answer({'tick':int(time.monotonic()),'result':0})
            if endpoint in DEVICES and method in ('PUT','POST'):
                frame=effect_frame(endpoint,body)
                if method=='PUT':
                    session['frames'][frame[0]]=frame; self.server.packet(session,frame); return self.answer({'result':0})
                if len(session['effects'])>=256: return self.answer({'result':8},503)
                key=str(uuid.uuid4()); session['effects'][key]=frame; return self.answer({'result':0,'id':key})
            if endpoint=='effect' and method in ('PUT','DELETE'):
                key=body['id']
                if key not in session['effects']: return self.answer({'result':87})
                if method=='DELETE': del session['effects'][key]
                else:
                    frame=session['effects'][key]; session['frames'][frame[0]]=frame; self.server.packet(session,frame)
                return self.answer({'result':0})
            return self.answer({'result':50},404)
        except NotImplementedError: self.answer({'result':50})
        except (ValueError,KeyError,TypeError,OverflowError): self.answer({'result':87},400)
        except (OSError,TimeoutError): self.close_connection=True
    do_GET=do_POST=do_PUT=do_DELETE=request


def main():
    port=int(os.environ.get('OPENRGB_GAME_SYNC_REST_PORT','54235'))
    server=Server(('127.0.0.1',port),int(os.environ.get('OPENRGB_GAME_SYNC_PORT','6743')))
    try:
        while True: server.handle_request(); server.tick()
    finally: server.server_close()

if __name__=='__main__': main()
