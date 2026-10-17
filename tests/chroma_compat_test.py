"""Exercise the actual REST request handler and wire encoder without opening sockets."""
import io
import json
from pathlib import Path
import struct
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from chroma_rest import Handler, Server, effect_frame

class Datagram:
    def __init__(self): self.packets = []
    def sendto(self, packet, destination): self.packets.append(packet)

class ChromaCompatibilityTests(unittest.TestCase):
    def setUp(self):
        self.server = Server.__new__(Server)
        self.server.sessions = {}
        self.server.server_port = 54235
        self.server.udp = Datagram()
        self.server.destination = ('127.0.0.1', 6743)
        self.request('POST', '/razer/chromasdk', {'title': 'test'})
        self.path = '/' + self.response['sessionid'] + '/chromasdk'

    def request(self, method, path, body):
        handler = Handler.__new__(Handler)
        handler.server = self.server
        handler.command = method
        handler.path = path
        raw = json.dumps(body).encode()
        handler.headers = {'Host': '127.0.0.1', 'Content-Length': str(len(raw))}
        handler.rfile = io.BytesIO(raw)
        def answer(data, status=200): self.response, self.status = data, status
        handler.answer = answer
        handler.request()
        return self.response

    def test_keyboard_custom2_overlay_survives_wire_encoding(self):
        param = {'color': [[0x123456]*24 for _ in range(8)], 'key': [[0]*22 for _ in range(6)]}
        param['key'][0][0] = 0x010000ff
        param['key'][5][21] = 0x0100ff00
        self.assertEqual(self.request('PUT', self.path+'/keyboard', {'effect': 'CHROMA_CUSTOM2', 'param': param}), {'result': 0})
        packet = self.server.udp.packets[-1]
        self.assertEqual(struct.unpack_from('<I6H', packet), (0x3153474f, 2, 1, 257, 8, 24, 192))
        colors = struct.unpack_from('<192I', packet, 96)
        self.assertEqual((colors[0], colors[-1], colors[34]), (0xff, 0xff00, 0x123456))

    def test_mouse_and_mousepad_custom2(self):
        mouse = effect_frame('mouse', {'effect': 'CHROMA_CUSTOM2', 'param': [[123]*7 for _ in range(9)]})
        self.assertEqual(mouse, (2, 9, 7, [123]*63))
        mat = effect_frame('mousepad', {'effect': 'CHROMA_CUSTOM2', 'param': [456]*20})
        self.assertEqual(mat, (4, 1, 20, [456]*20))

    def test_legacy_mouse_and_keyboard_remain_supported(self):
        self.assertEqual(effect_frame('mouse', {'effect': 'CHROMA_CUSTOM', 'param': [7]*30}), (2,1,30,[7]*30))
        self.assertEqual(effect_frame('keyboard', {'effect': 'CHROMA_CUSTOM', 'param': [[8]*22 for _ in range(6)]}), (1,6,22,[8]*132))

    def test_create_select_delete_group_with_individual_errors(self):
        result = self.request('POST', self.path+'/keyboard', {'effects': [
            {'effect': 'CHROMA_STATIC', 'param': {'color': 255}},
            {'effect': 'CHROMA_WAVE'},
            {'effect': 'CHROMA_STATIC', 'param': {'color': 65280}},
        ]})
        self.assertEqual([r['result'] for r in result['results']], [0,50,0])
        ids = [result['results'][0]['id'], 'missing', result['results'][2]['id']]
        self.assertFalse(self.server.udp.packets)
        result = self.request('PUT', self.path+'/effect', {'ids': ids})
        self.assertEqual(result, {'results': [{'result':0},{'result':87},{'result':0}]})
        self.assertEqual(len(self.server.udp.packets), 2)
        result = self.request('DELETE', self.path+'/effect', {'ids': ids})
        self.assertEqual(result, {'results': [{'result':0},{'result':87},{'result':0}]})
        self.assertEqual(self.request('PUT', self.path+'/effect', {'id':ids[0]}), {'result':87})

    def test_groups_and_cache_have_a_bounded_size(self):
        result = self.request('POST', self.path+'/keyboard', {'effects': [{'effect':'CHROMA_NONE'}]*256})
        self.assertTrue(all(r['result']==0 for r in result['results']))
        self.assertEqual(self.request('POST', self.path+'/keyboard', {'effect':'CHROMA_NONE'}), {'result':8})
        self.assertEqual(self.status, 503)
        self.assertEqual(self.request('POST', self.path+'/keyboard', {'effects':[{'effect':'CHROMA_NONE'}]*257}), {'result':87})

    def test_invalid_payloads_return_errors_without_mutating_frames(self):
        for body in [{'ids':[{}]}, {'ids':[]}, {'ids':'bad'}, {'id':{}}, {}]:
            self.assertEqual(self.request('PUT', self.path+'/effect', body), {'result':87})
            self.assertEqual(self.status, 400)
        for matrix in [None, [[0]*24]*7, [[True]*24]*8, [[0x1000000]*24]*8]:
            self.assertEqual(self.request('PUT', self.path+'/keyboard', {
                'effect':'CHROMA_CUSTOM2','param':{'color':matrix,'key':[[0]*22]*6}}), {'result':87})
        self.assertEqual(self.request('PUT', self.path+'/keyboard', {
            'effect':'CHROMA_CUSTOM2','param':{'color':[[0]*24]*8,'key':None}}), {'result':87})
        self.assertFalse(self.server.udp.packets)

if __name__ == '__main__':
    unittest.main()
